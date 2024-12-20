#pragma once
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
  T pop();
  std::optional<T> try_pop();
};
} // namespace prf
