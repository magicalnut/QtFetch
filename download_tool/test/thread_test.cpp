#include "../include/thread_pool.hpp"

#include <atomic>
#include <chrono>
#include <future>
#include <iostream>
#include <string>
#include <thread>
#include <vector>

using namespace std::chrono_literals;

// 简单的断言：失败时打印 FAIL 并让进程返回非零，ctest 即可判定失败。
static int g_checks = 0;
static int g_failed = 0;

static void check(bool ok, const std::string& msg) {
  g_checks++;
  if (ok) {
    std::cout << "[PASS] " << msg << std::endl;
  } else {
    std::cout << "[FAIL] " << msg << std::endl;
    g_failed++;
  }
}

int main() {
  // 1. 任务全部执行且只执行一次
  {
    download::thread_pool pool(4);
    std::atomic<int> count{0};
    std::vector<std::future<void>> futures;

    for (int i = 0; i < 10; i++) {
      futures.push_back(pool.submit_task([&count]() { count++; }));
    }
    for (auto& f : futures) f.get();

    check(count.load() == 10,
          "10 个任务全部执行且只执行一次 (实际=" + std::to_string(count) +
              ")");
  }

  // 2. submit_task 的返回值可以通过 future 取回
  {
    download::thread_pool pool(2);
    auto fut = pool.submit_task([](int a, int b) { return a + b; }, 2, 3);

    int result = fut.get();
    check(result == 5,
          "submit_task 返回任务结果 (实际=" + std::to_string(result) + ")");
  }

  // 3. 多个任务确实并发执行
  {
    download::thread_pool pool(4);

    std::atomic<int> in_flight{0};
    std::atomic<int> max_in_flight{0};

    auto work = [&] {
      int cur = ++in_flight;
      int prev = max_in_flight.load();
      while (cur > prev && !max_in_flight.compare_exchange_weak(prev, cur)) {
      }
      std::this_thread::sleep_for(80ms);
      in_flight--;
    };

    std::vector<std::future<void>> futures;
    for (int i = 0; i < 4; i++) futures.push_back(pool.submit_task(work));
    for (auto& f : futures) f.get();

    check(max_in_flight.load() >= 2,
          "至少两个任务同时在执行 (峰值=" +
              std::to_string(max_in_flight.load()) + ")");
  }

  std::cout << "共 " << g_checks << " 项断言, 失败 " << g_failed << " 项"
            << std::endl;
  return g_failed == 0 ? 0 : 1;
}
