#pragma once

#include <thread>
#include <future>
#include <mutex>
#include <condition_variable>
#include <vector>
#include <queue>
#include <functional>
#include <utility>
#include <memory>
#include <type_traits>

namespace download{

  class thread_pool{

  public:

    thread_pool(int num_thread);

    ~thread_pool();

    void exit();


    /*
      功能：向线程池提交任务
      参数：
        T: 任务函数
        args: 任务函数的参数
      返回值
        返回执行任务的future句柄
    */
    template<typename T, typename... args>
    auto submit_task(T&& fn, args&&... inp)
      -> std::future<std::invoke_result_t<std::decay_t<T>, std::decay_t<args>...>>
    {

      using ret_type = std::invoke_result_t<std::decay_t<T>, std::decay_t<args>...>;
      
      auto bd = std::bind(std::forward<T>(fn), std::forward<args>(inp)...);
      auto pack = std::packaged_task<ret_type()>(std::move(bd));

      auto fut = pack.get_future();

      auto task = std::make_shared<decltype(pack)>(std::move(pack));

      {

        std::lock_guard<std::mutex> lock(mtx);   
        tasks.emplace([task](){ (*task)(); });
        cv.notify_one();
      }

      return fut;
    }

  private:

    void event_loop();

    int stop_flag = 0;
    int num_thread;
    

    std::mutex mtx;
    std::condition_variable cv;

    std::vector<std::thread> threads;
    std::queue<std::function<void()>> tasks;

  };

} //namespace download
