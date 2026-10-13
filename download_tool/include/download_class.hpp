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

  download_class(const download_class&) = delete;
  download_class& operator=(const download_class&) = delete;

  download_class(std::string filename, std::string addr) : addr(addr), file_name(filename), pause_signal(0) {
  }

  ~download_class(){
    if(server_socket){
      ::close(server_socket);
    }
  }

  int initialize(std::string filename, std::string addr){
    
    if(server_socket){
      ::close(server_socket);
    }

    file_name = filename;
    this->addr = addr;

    return 0;
  }

  int connect(){ 
    server_socket = find_server_socket(addr, "80");
    if(server_socket < 0){
      return -1;
    }

    return 0;
  }

  int close(){
    if(server_socket){
      ::close(server_socket);
      server_socket = 0;
    }
    return 0;
  }

  int get_server_socket(){ 
    return server_socket;
  }
  int send_request(){
    std::string mes = "GET /" + file_name + " HTTP/1.1\r\n" +
                      "Host: " + addr + "\r\n" +
                      "Connection: close\r\n" +
                      "\r\n";
    send_message(server_socket, mes);
    return 0;
  }

private:
  
  int server_socket = 0;
  std::string file_name;
  std::string addr;
  
  int pause_signal;

  
  
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