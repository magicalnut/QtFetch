#pragma once

#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>

#include <mutex>
#include <condition_variable>
#include <fcntl.h>

#include <string>
#include <iostream>


#include "download_class.hpp"
#include "coro.hpp"


#define DEBUG

namespace download{

  class client{
  public:

    client(const client&) = delete;
    client& operator=(const client&) = delete;

  /*
  初始构造
    filename： 请求的对方服务器的文件
    addr:     请求的url 或 ip  （均可）


    例如 client(index.html, www.baidu.com)  下载百度的主页html文件
  */
  client(std::string filename, std::string addr) : download(filename, addr){
    reset_sign();
  }

  /*
    更换下载的源文件和地址
    参数同上
  */
  int change_download_source(std::string filename, std::string addr){
    reset_sign();
    return download.initialize(filename, addr);
  }

  /*
    入口
    save_as_filename 保存的地址和文件，若文件已存在会直接清空文件
  */
 coro_task<bool> start_download(std::string save_as_filename){
  
    if(download.connect() < 0)  co_return -1;
    file_fd = open(save_as_filename.c_str(), O_RDWR | O_CREAT | O_TRUNC | O_APPEND, 0666);
    if(file_fd < 0) co_return -1;

    download.send_request();

    char tmp[4096];
    while(1){
      int sz = recv(download.get_server_socket(), tmp, sizeof(tmp), 0);
      if(sz <= 0) break;

      int sz1 = write(file_fd, tmp, sz);
      if(sz1 <= 0)  break;
    }

    download.close();
  }
  
  private:
    download_class download;
    int file_fd;

    bool read_end;
    bool write_end;
    void reset_sign(){
      read_end = 0;
      write_end = 0;
    }

    std::string buf;

  };
//==========================class===============

  int find_server_socket(std::string addr, std::string port = "80"){
    int server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket < 0){
      std::cout << "Error opening socket" << std::endl;
      return -1;
    }
    addrinfo hints {};
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
  
    addrinfo *server_info;
    int ret = getaddrinfo(addr.c_str(), port.c_str(), &hints, &server_info);
    if (ret != 0 || server_info == nullptr){
      std::cout << "Error getaddrinfo: " << gai_strerror(ret) << std::endl;
      return -1;
    }
    if (connect(server_socket, server_info->ai_addr, server_info->ai_addrlen) < 0){
      std::cout << "Error connecting" << std::endl;
      freeaddrinfo(server_info);
      return -1;
    }

#ifdef DEBUG
    char ip[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &((sockaddr_in*)server_info->ai_addr)->sin_addr, ip, sizeof(ip));
    std::cout << "将从地址" << ip << "处下载" << std::endl;
#endif

    freeaddrinfo(server_info);
    return server_socket;
  }


} // namespace download