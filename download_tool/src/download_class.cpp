#include "download_class.hpp"
#include "tls.hpp"

#include <stdexcept>

namespace download {

  /*
    功能：
      通用全局工具函数，用于发送消息，封装消息一次性发不完整的情况

    参数：
      server_socket: 目标服务器的socket
      msg: 要发送的消息
  */
  int send_message(connection* conn, std::string msg){
    size_t sent = 0;
    while (sent < msg.size()){
      int n = conn->send(msg.c_str() + sent, msg.size() - sent);
      if (n <= 0) return -1;
      sent += n;
    }
    return 0;
  }

  /*
    功能：
      裸 socket 版本的 send_message，client.cpp 仍在使用，
      与 connection* 版本重载共存
  */
  int send_message(int server_socket, std::string msg){
    size_t sent = 0;
    while (sent < msg.size()){
      int n = ::send(server_socket, msg.c_str() + sent, msg.size() - sent, 0);
      if (n <= 0) return -1;
      sent += n;
    }
    return 0;
  }

  download_class::download_class(std::string path, std::string host, std::string port, std::mutex &mtx, std::condition_variable &cv, int file_fd, std::atomic<int> &downloaded_size)
  : host(host), path(path), port(port), mtx(mtx), cv(cv), file_fd(file_fd), downloaded_size(downloaded_size)
  {
  }

  download_class::~download_class(){
    if(server_socket){
      close_socket(server_socket);
    }
    if(file_fd){
      ::close(file_fd);
    }
  }

  int download_class::run(int start_byte, int end_byte){
      pause_signal = 0;
      int offset = start_byte;

      //int server_fd = connection::find_server_socket(host, port);
      connection* conn;
      if(protocol == "https") conn = new tls_connection(host, "443", ctx);
      else conn = new tcp_connection(host, "80");
      send_request(start_byte, end_byte, conn);

      std::string buf;
      char tmp[4096];
      bool header_done = false;

      while(1){

          {
              std::unique_lock<std::mutex> lk(mtx);
              cv.wait(lk, [&](){ return !pause_signal || done_signal; });
          }
          if(done_signal) break;
          int sz = conn->recv(tmp, sizeof(tmp));
          if(sz < 0){
              delete conn;
              throw std::runtime_error("cannot recv");
          }
          if(sz == 0) break;

          buf.append(tmp, sz);

          if(!header_done){
              int pos = buf.find("\r\n\r\n");
              if(pos == std::string::npos) continue;
              buf = buf.substr(pos + 4);
              header_done = true;
          }

          while(offset <= end_byte && !buf.empty()){
              int n = pwrite_at(file_fd, buf.data(), buf.size(), offset);
              if(n <= 0){
                
                if(errno == EINTR || errno == EAGAIN) continue;

                delete conn;
                throw std::runtime_error("cannot write");
              }
              offset += n;
              downloaded_size += n;
              buf.erase(0, n);
          }
          if(offset > end_byte)  break;
      }

      delete conn;
      return 0;
  }
  
  void download_class::pause(){
    pause_signal = 1;
  }
  
  void download_class::resume(){
    pause_signal = 0;
    cv.notify_all();
  }

  int download_class::is_done(){
    return done_signal;
  }

  void download_class::exit(){
    done_signal = 1;
    cv.notify_all();
  }
  

  int download_class::close(){
    if(server_socket){
      close_socket(server_socket);
      server_socket = 0;
    }
    return 0;
  }

  int download_class::send_request(int start_byte, int end_byte, connection* conn){       
    std::string msg = "GET /" + path + " HTTP/1.1\r\n" +
                      "Host: " + host + "\r\n" +
                      "Connection: close\r\n" +
                      "Range: bytes=" + std::to_string(start_byte) + "-" + std::to_string(end_byte) + "\r\n" +
                      "\r\n";
    send_message(conn, msg);
    return 0;
  }

  int download_class::connect(){
    server_socket = connection::find_server_socket(host, port);
    if(server_socket < 0){
      throw std::runtime_error("cannot connect to server");
    }
    return 0;
  }

  void download_class::set_protocol(std::string protocol, SSL_CTX* ctx){
    this->protocol = protocol;
    this->ctx = ctx;
  }

} // namespace download
