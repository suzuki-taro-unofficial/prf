#include "thread.hpp"
#include "executor.hpp"
#include "planner.hpp"
#include <atomic>
#include <chrono>
#include <thread>

namespace prf {
std::atomic_bool stop_the_threads(false);
std::atomic_int8_t wait_threads(0);

void stop_execution() {
  wait_threads.store(2);
  stop_the_threads.store(true);
  PlannerManager::messages.notify_stop();
  Executor::messages.notify_stop();
  while (wait_threads.load() != 0) {
    // この関数自体常用することを想定していないので雑に待つ実装にしておく
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  }
  stop_the_threads.store(false);
}
} // namespace prf
