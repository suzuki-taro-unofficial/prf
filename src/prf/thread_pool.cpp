#include "prf/thread_pool.hpp"
#include "prf/stream.hpp"
#include <algorithm>
#include <optional>

namespace prf {
size_t ThreadPool::get_nubmer_of_threads() { return this->number_of_threads; }

ThreadPool::ThreadPool(size_t number_of_threads)
    : number_of_threads(number_of_threads) {
  for (size_t id = 0; id < number_of_threads; ++id) {
    this->threads.push_back(std::thread([&queue = this->queue, id]() {
      while (true) {
        std::optional<Task> otask = queue.pop();
        if (otask.has_value()) {
          (*otask)();
        } else {
          break;
        }
      }
      info_log("ThreadPool: id: %ld 停止します", id);
    }));
  }
}

ThreadPool::~ThreadPool() { this->stop(); }

std::shared_ptr<utils::Waiter> ThreadPool::request(Task task) {
  auto res = std::make_shared<utils::Waiter>();
  this->queue.push([res, task]() -> void {
    task();
    res->done();
  });
  return res;
}

void ThreadPool::stop() {
  // TODO ConcurrentQueueの停止条件をもっと柔軟に変えられるように後でしておく
  // グローバル変数に直接依存は不便...
  this->queue.notify_stop();
  for (auto &thread : this->threads) {
    thread.join();
  }

  info_log("ThreadPoolは停止しました");
}

ThreadPool ThreadPool::create_suitable_pool() {
  size_t hardware_concurrency = std::thread::hardware_concurrency();
  size_t number_of_threads =
      std::max(hardware_concurrency, MINIMUM_NUMBER_OF_THREADS_ON_AUTOMATIC);
  return ThreadPool(number_of_threads);
}

} // namespace prf
