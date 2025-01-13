#pragma once
#include "prf/thread.hpp"
#include <condition_variable>
#include <mutex>
#include <optional>
#include <queue>

namespace prf {
template <class T> class ConcurrentQueue {
  std::mutex data_lock;
  std::condition_variable wait;
  std::queue<T> data;

public:
  void push(T value);
  /**
   * スレッドを停止するときにnulloptが返される
   */
  std::optional<T> pop();
  std::optional<T> try_pop();

  /**
   * このQueueを利用しているスレッドに停止を通知する
   */
  void notify_stop();
};

template <class T> void ConcurrentQueue<T>::push(T value) {
  std::lock_guard<std::mutex> lock(data_lock);
  data.push(value);
  wait.notify_one();
}

template <class T> std::optional<T> ConcurrentQueue<T>::pop() {
  std::unique_lock<std::mutex> lock(data_lock);
  wait.wait(lock, [this] {
    return not this->data.empty() or stop_the_threads.load();
  });
  if (stop_the_threads.load()) {
    return std::nullopt;
  }
  T res = data.front();
  data.pop();
  if (not data.empty()) {
    wait.notify_one();
  }
  return res;
}

template <class T> std::optional<T> ConcurrentQueue<T>::try_pop() {
  std::lock_guard<std::mutex> lock(data_lock);
  if (data.empty()) {
    return std::nullopt;
  } else {
    std::optional<T> res = data.front();
    data.pop();
    return res;
  }
}

template <class T> void ConcurrentQueue<T>::notify_stop() { wait.notify_all(); }
} // namespace prf
