#include "timer_wheel.hpp"

#include <algorithm>
#include <stdexcept>

namespace download {

  Timer_Wheel timer(100, std::chrono::milliseconds(100));

  Timer_Wheel::Timer_Wheel(int slot_num, std::chrono::milliseconds tick)
    : slot_num(slot_num), tick(tick), wheel(slot_num)
  {

    start();
  }

  Timer_Wheel::~Timer_Wheel(){
    stop();
  }

  void Timer_Wheel::stop(){
    if(stop_flag) return;

    {
      std::unique_lock<std::mutex> lock(mtx);
      stop_flag = 1;
       for (auto& slot : wheel) slot.clear();
    }

    cv.notify_all();
    
    if (timer_thread.get_id() != std::this_thread::get_id()) {
      timer_thread.join();
    } else {
      timer_thread.detach();
    }
  }

  void Timer_Wheel::start(){
    std::lock_guard<std::mutex> lock(mtx);
    stop_flag = 0;
    cv.notify_all();

    timer_thread = std::thread([this](){ 
      timer_loop();
    });
  }

  void Timer_Wheel::add_timer(std::chrono::milliseconds timeout, std::function<void()> callback){
    int ticks = (timeout + tick - std::chrono::milliseconds(1)) / tick;
      ticks = (std::max)(1, ticks);

    if(ticks >= slot_num)
      throw std::runtime_error("timeout too long");

    {
      std::lock_guard<std::mutex> lock(mtx);
      wheel[(cur_slot + ticks) % slot_num ].push_back(callback);
    }
  }

   
  void Timer_Wheel::add_always_timer(std::chrono::milliseconds timeout, std::function<void()> callback){

    int ticks = (timeout + tick - std::chrono::milliseconds(1)) / tick;
      ticks = (std::max)(1, ticks);

    if(ticks >= slot_num)
      throw std::runtime_error("timeout too long");

    auto pack = [callback, this, timeout](){
      callback();
      add_always_timer(timeout, callback);
    };
    {
      std::lock_guard<std::mutex> lock(mtx);
      wheel[(cur_slot + ticks) % slot_num ].push_back(pack);
    }


  }

  void Timer_Wheel::run_task(){ 
    for(auto &callback : due_callbacks){
      callback();
    }
    due_callbacks.clear();
  }
  
  void Timer_Wheel::timer_loop(){ 
    
    while(1){ 
    
      {
        std::unique_lock<std::mutex> lk(mtx);
        cv.wait_for(lk, tick, [this](){ return stop_flag; });
        if(stop_flag) break;
        swap(wheel[cur_slot], due_callbacks);
      }

      run_task();
      {
        std::unique_lock<std::mutex> lk(mtx);
        ++ cur_slot %= slot_num;
      }
      
    }


  }

} // namespace download
