#pragma once

#include <mutex>
#include <condition_variable>
#include <atomic>

#include <string>
#include <utility>
#include <tuple>
#include <chrono>
#include <memory>

#include "download_class.hpp"
#include "http_parser.hpp"
#include "timer_wheel.hpp"
#include "thread_pool.hpp"
#include "tls.hpp"

namespace download{

  extern Timer_Wheel timer;

  std::string read_all(int);
  std::string read_all(connection& conn);

  class Worker{
  public:
    Worker(const Worker&) = delete;
    Worker& operator=(const Worker&) = delete;


    /*
      功能：
        构造下载类
      参数：
        url: 要下载的url
        save_path: 保存路径
        num_thread: 线程数
    */
    Worker(std::string url, std::string save_path, int num_thread);

    ~Worker();

    //run调用的是底层download_tool 的run函数， 根据num_thread向线程池注册多个任务

    int run();

    void pause();

    void resume();

    void stop();

    int is_done();


    //以下两个函数是注册到时间轮里面的
    
    double get_progress();

    double get_speed();
    std::unique_ptr<SSL_CTX_manager> ctx_manager;

  private:
    int file_fd;
    std::unique_ptr<download_class> downloader;
    // download_class downloader;

    std::mutex mtx;
    std::condition_variable cv;

    std::string protocol;
    std::string host;
    std::string path;

    std::atomic<int> downloaded_size = 0;
    int last_download_size = 0;
    int need_download_size = -1;

    bool exit_flag = false;
    bool done_flag = false;

    std::atomic<double> speed = 0;

    int num_thread;
    
    /*
      功能：把url拆分成(protocol, host, path)
      参数：url - 要解析的url
      返回值：(protocol, host, path)
    */
    std::tuple<std::string, std::string, std::string> parse_url(std::string url);

    Worker(std::tuple<std::string, std::string, std::string> parsed_url, std::string save_path, int num_thread);

    void update_speed_loop();

    /*

      功能：
        在错误码为3XX时，跟随Location(小写)跳转到对应的连接，
        直到错误码为非3XX为止，更新response最后一次200的响应并返回最后一次跳转的真实资源链接
      参数：
        response:当前发生3XX错误码的响应
      返回值
        真实的资源链接
    */
    std::string Redirect(HttpParser& response);


    thread_pool work_threads;
  };
//==========================class===========================================================

} // namespace download
