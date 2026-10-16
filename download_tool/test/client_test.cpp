#include <iostream>
#include <fstream>
#include <iterator>
#include <filesystem>
#include <thread>
#include <chrono>

#include "../include/client.hpp"

int main()
{

  //用法
  download::Worker c("www.baidu.com/", "test_baidu.html", 1);

  
  c.run();
  
  std::this_thread::sleep_for(std::chrono::seconds(2));
  
  // auto task = c.start_download(save.string());
  // task.handle_.resume();                
  
  
  //下面是测试信息
  std::filesystem::path save = std::filesystem::absolute("test_baidu.html");  //保存的文件名，文件会下载程序运行的路径，后期可以改成指定的绝对路径
  std::ifstream f(save, std::ios::binary);

  if (!f) {
    std::cout << "FAIL: file not created" << std::endl;
    return 1;
  }

  std::string content((std::istreambuf_iterator<char>(f)),
                      std::istreambuf_iterator<char>());
  if (content.empty() || content.rfind("HTTP/", 0) != 0) {
    std::cout << "FAIL: no HTTP response" << std::endl;
    return 1;
  }

  std::cout << "PASS: downloaded " << content.size() << " bytes" << std::endl;
  return 0;
}
