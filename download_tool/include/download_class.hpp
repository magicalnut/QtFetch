#pragma once

#include <unistd.h>
#include <sys/socket.h>
#include <string>
#include <cstddef>

#include "coro.hpp"

//download(string filename, string addr)
namespace download { 

int find_server_socket(std::string addr, std::string port);
int send_message(int server_socket, std::string mes);

class download_class{
public:

  download_class& operator=(const download_class&) = delete;

  download_class(std::string filename, std::string addr, std::string save_as_file) 
  : addr(addr), file_name(filename), save_as_filename(save_as_file)
  {
  }

  ~download_class(){
    if(server_socket){
      ::close(server_socket);
    }
    if(file_fd){
      ::close(file_fd);
    }
  }

  int initialize(std::string filename, std::string addr, std::string save_as_file){
    close();
    file_name = filename;
    this->addr = addr;
    this->save_as_filename = save_as_file;
    done_signal = 0;
    pause_signal = 1;
    return 0;
  }

  int run(){
    if(connect() < 0)  return -1;
    file_fd = open(save_as_filename.c_str(), O_RDWR | O_CREAT | O_TRUNC | O_APPEND, 0666);
    if(file_fd < 0) return -1;

    send_request();
    pause_signal = 0;
    char tmp[4096];
    while(1){

      {
        std::unique_lock<std::mutex> lk(mtx);
        cv.wait(lk, [&](){return (!pause_signal || done_signal);});
      }

      int sz = recv(get_server_socket(), tmp, sizeof(tmp), 0);
      if(sz <= 0) return 0;

      int sz1 = write(file_fd, tmp, sz);
      if(sz1 <= 0)  return -1;
    }

    close();
    return 0;
  }
  

  int get_server_socket(){ 
    return server_socket;
  }

  void pause(){
    pause_signal = 1;
  }
  
  void resume(){
    pause_signal = 0;
    cv.notify_one();
  }

  int is_done(){
    return done_signal;
  }

  void exit(){
    done_signal = 1;
    cv.notify_one();
  }
  

private:

  std::mutex mtx;
  std::condition_variable cv;

  int pause_signal = 1;
  int done_signal = 0;

  int server_socket = 0;
  int file_fd = 0;

  std::string save_as_filename;

  std::string file_name;
  std::string addr;

  int close(){
    if(server_socket){
      ::close(server_socket);
      server_socket = 0;
    }
    return 0;
  }

  int send_request(){
    std::string mes = "GET /" + file_name + " HTTP/1.1\r\n" +
                      "Host: " + addr + "\r\n" +
                      "Connection: close\r\n" +
                      "\r\n";
    send_message(server_socket, mes);
    return 0;
  }

  int connect(){ 
    server_socket = find_server_socket(addr, "80");
    if(server_socket < 0){
      return -1;
    }

    return 0;
  }

};

  inline int send_message(int server_socket, std::string mes){
    size_t sent = 0;
    while (sent < mes.size()){
      ssize_t n = send(server_socket, mes.c_str() + sent, mes.size() - sent, 0);
      if (n < 0) return -1;
      sent += n;
    }
    return 0;
  }

  


} // namespace download