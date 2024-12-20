#include "concurrent_queue.hpp"
#include <mutex>

namespace prf {
template <class T> void ConcurrentQueue<T>::push(T value) {
  std::lock_guard<std::mutex> lock(data_lock);
  data.push(value);
  wait.notify_one();
}

template <class T> T ConcurrentQueue<T>::pop() {
  std::unique_lock<std::mutex> lock(data_lock);
  wait.wait(lock, [this] { return not this->data.empty(); });
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

} // namespace prf
