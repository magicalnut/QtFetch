#pragma once

#include "platform.hpp"
#include <string>
#include <cstddef>
#include <atomic>

#include "thread_pool.hpp"

//download(string filename, string addr)
namespace download { 

int find_server_socket(std::string addr, std::string port);
int send_message(int server_socket, std::string mes);

class download_class{
public:

  download_class& operator=(const download_class&) = delete;

  download_class(std::string filename, std::string host, std::mutex &mtx, std::condition_variable &cv, int file_fd, std::atomic<int> &download_history)
  : host(host), file_name(filename), mtx(mtx), cv(cv), file_fd(file_fd), download_history(download_history)
  {
  }

  ~download_class(){
    if(server_socket){
      close_socket(server_socket);
    }
    if(file_fd){
      ::close(file_fd);
    }
  }

  // int initialize(std::string filename, std::string addr, std::string save_as_file){
  //   close();
  //   file_name = filename;
  //   this->addr = addr;
  //   this->save_as_filename = save_as_file;
  //   done_signal = 0;
  //   pause_signal = 1;
  //   return 0;
  // }

  int run(int start_byte, int end_byte){
      pause_signal = 0;
      int offset = start_byte;

      int server_fd = find_server_socket(host, "80");
      send_request(start_byte, end_byte, server_fd);

      std::string buf;
      char tmp[4096];
      bool header_done = false;

      while(1){
          // 暂停 / 取消检查（放在 recv 之前，这样暂停才真的生效）
          {
              std::unique_lock<std::mutex> lk(mtx);
              cv.wait(lk, [&](){ return !pause_signal || done_signal; });
          }
          if(done_signal) break;

          int sz = recv(server_fd, tmp, sizeof(tmp), 0);
          if(sz < 0){
              close_socket(server_fd);
              throw std::runtime_error("cannot recv");
          }
          if(sz == 0) break;              // 连接正常关闭

          buf.append(tmp, sz);

          if(!header_done){
              int pos = buf.find("\r\n\r\n");
              if(pos == std::string::npos) continue;   // 头还没收全
              buf = buf.substr(pos + 4);
              header_done = true;
          }

          while(!buf.empty()){
              int n = pwrite_at(file_fd, buf.data(), buf.size(), offset);
              if(n <= 0){
                  close_socket(server_fd);
                  throw std::runtime_error("cannot write");
              }
              offset += n;
              download_history += n;
              buf.erase(0, n);            // 只删已写出的部分，正确处理部分写
          }
      }

      close_socket(server_fd);
      return 0;
  }
  
  void pause(){
    pause_signal = 1;
  }
  
  void resume(){
    pause_signal = 0;
    cv.notify_all();
  }

  int is_done(){
    return done_signal;
  }

  void exit(){
    done_signal = 1;
    cv.notify_all();
  }
  

private:

  std::mutex& mtx;
  std::condition_variable& cv;

  std::atomic<int> pause_signal = 1;
  std::atomic<int> done_signal = 0;

  int server_socket = 0;
  int file_fd = 0;

  std::string file_name;
  std::string host;

  std::atomic<int>& download_history;

  int close(){
    if(server_socket){
      close_socket(server_socket);
      server_socket = 0;
    }
    return 0;
  }

  int send_request(int start_byte, int end_byte, int fd){
    std::string mes = "GET /" + file_name + " HTTP/1.1\r\n" +
                      "Host: " + host + "\r\n" +
                      "Connection: close\r\n" +
                      "Range: bytes=" + std::to_string(start_byte) + "-" + std::to_string(end_byte) + "\r\n" +
                      "\r\n";
    send_message(fd, mes);
    return 0;
  }
  int connect(){
    server_socket = find_server_socket(host, "80");
    if(server_socket < 0){
      throw std::runtime_error("cannot connect to server");
      return -1;
    }
    return 0;
  }

};

  inline int send_message(int server_socket, std::string mes){
    size_t sent = 0;
    while (sent < mes.size()){
      int n = send(server_socket, mes.c_str() + sent, mes.size() - sent, 0);
      if (n < 0) return -1;
      sent += n;
    }
    return 0;
  }

  
  


} // namespace download