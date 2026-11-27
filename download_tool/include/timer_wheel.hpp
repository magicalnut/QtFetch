#pragma once

#include <thread>
#include <mutex>
#include <condition_variable>

#include <vector>
#include <list>
#include <chrono>

#include <functional>

namespace download { 

  class Timer_Wheel{  
  public:

    Timer_Wheel& operator=(const Timer_Wheel&) = delete;
    Timer_Wheel(const Timer_Wheel&) = delete;

    Timer_Wheel(int slot_num, std::chrono::milliseconds tick);

    ~Timer_Wheel();

    void stop();

    void start();


    /*
      功能：向定时器中添加一个定时任务
      参数：
        timeout: 定时器时间
        callback: 定时器到期时调用的函数
    */
    void add_timer(std::chrono::milliseconds timeout, std::function<void()> callback);

    /*
      功能：对add_timer的封装，向定时器添加一个重复执行的定时任务
      参数：
        timeout: 定时器时间
        callback: 定时器到期时调用的函数
    */
    void add_always_timer(std::chrono::milliseconds timeout, std::function<void()> callback);

  private:

    int stop_flag = 0;

    int slot_num;
    std::chrono::milliseconds tick;
    int cur_slot = 0;

    std::thread timer_thread;

    std::mutex mtx;
    std::condition_variable cv;

    std::vector<std::list<std::function<void()>>> wheel;
    std::list<std::function<void()>> due_callbacks;

    void run_task();
    
    void timer_loop();

  };  //class

} //namespace
