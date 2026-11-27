#include "thread_pool.hpp"

namespace download{

  thread_pool::thread_pool(int num_thread) : num_thread(num_thread) {
    for(int i = 0; i < num_thread; i++){
      threads.emplace_back([this](){ event_loop(); });
    }
  }

  thread_pool::~thread_pool(){
    exit();
  }

  void thread_pool::exit(){
    {
      std::lock_guard<std::mutex> lock(mtx);
      this->stop_flag = 1;
    }
    cv.notify_all();
    for(auto& t : threads) t.join();
    return;
  }

  void thread_pool::event_loop(){

    while(1){
      std::function<void()> task;
      
      {

        std::unique_lock<std::mutex> lock(mtx);
        cv.wait(lock, [this]() { return (!stop_flag && !tasks.empty()) || stop_flag;  });

        if(stop_flag || tasks.empty() ) return;

        task = std::move(tasks.front());
        tasks.pop();
      }
      task();
    }
  }

} //namespace download
