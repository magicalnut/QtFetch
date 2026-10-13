#pragma once

#include <coroutine>

namespace download { 
  
  
  template <typename T>
  class coro_task {
    public:
    struct promise_type {

      T value;

      coro_task get_return_object() {
        return coro_task(std::coroutine_handle<promise_type>::from_promise(*this));
      }
      std::suspend_always initial_suspend() {
        return {};
      }
      std::suspend_always final_suspend() noexcept {
        return {};
      }

      void return_value(T value) {
        this->value = value;
      }

      void unhandled_exception() {}
    };

    explicit coro_task(std::coroutine_handle<promise_type> handle) : handle_(handle) {}

    ~coro_task() {
      if (handle_) {
        handle_.destroy();
      }
    }

    std::coroutine_handle<promise_type> handle_;
  };

  template <>
  class coro_task<void> {
    public:
    struct promise_type {

      coro_task get_return_object() {
        return coro_task(std::coroutine_handle<promise_type>::from_promise(*this));
      }
      std::suspend_always initial_suspend() {
        return {};
      }
      std::suspend_always final_suspend() noexcept {
        return {};
      }

      void return_void() {}
      void unhandled_exception() {}
    };

    explicit coro_task(std::coroutine_handle<promise_type> handle) : handle_(handle) {}

    ~coro_task() {
      if (handle_) {
        handle_.destroy();
      }
    }

    std::coroutine_handle<promise_type> handle_;
  };


} //namespace