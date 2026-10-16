#pragma once

#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>

#include <mutex>
#include <condition_variable>
#include <fcntl.h>
#include <atomic>
#include <thread>
#include <unistd.h>

#include <string>
#include <iostream>


#include "download_class.hpp"
#include "coro.hpp"


#define DEBUG

namespace download{

  class Worker{
  public:
    Worker(const Worker&) = delete;
    Worker& operator=(const Worker&) = delete;

  Worker(std::string url, std::string save_path, int num_thread) : 
    Worker(parse_url(url), std::move(save_path), num_thread)
  {}

  ~Worker() {
    downloader.exit();
  }

  int run(){
    std::thread([this](){
      downloader.run();
    }).detach();

    return 0;
  }

  void pause(){
    downloader.pause();
  }

  void resume(){
    downloader.resume();
  }

  void kill_task(){
    downloader.exit();
  }

  int is_done(){
    return downloader.is_done();
  }

  int change_download_task(std::string url, std::string save_path){
    
    downloader.exit();

    auto res = parse_url(url);
    std::string filename = res.second;
    std::string addr = res.first;
    return downloader.initialize(filename, addr, save_path);
  }

  private:
    download_class downloader;
    int file_fd;

    int have_download = 0;

    std::mutex mtx;
    std::condition_variable cv;

    std::string buf;

    std::pair<std::string, std::string> parse_url(std::string url){
      int pos = url.find("://");
      if (pos == std::string::npos) pos = 0;
      else  pos += 3;

      int cut = url.find("/", pos);

      if(cut == std::string::npos) return {url.substr(pos), ""};
      else return {url.substr(pos, cut - pos), url.substr(cut + 1)};
    }

    Worker(std::pair<std::string, std::string> parse_res, std::string save_path, int num_thread)
      : downloader(parse_res.second, parse_res.first, save_path)
    {
    }
    
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