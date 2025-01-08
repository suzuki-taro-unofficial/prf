#pragma once

#include "concurrent_queue.hpp"
#include "utils.hpp"
#include <functional>
#include <thread>
#include <vector>

namespace prf {

/**
 * スレッド数を自動で計算するときの、スレッド数の最低値保証
 * 1コア環境だとしても複数スレッドあると都合が良い場合もあるので余裕を持っておく
 */
const size_t MINIMUM_NUMBER_OF_THREADS_ON_AUTOMATIC = 4;
class ThreadPool {
private:
  using Task = std::function<void()>;
  ConcurrentQueue<Task> queue;

  size_t number_of_threads;

  std::vector<std::thread> threads;

public:
  size_t get_nubmer_of_threads();

  ThreadPool(const ThreadPool &) = delete;
  ThreadPool &operator=(const ThreadPool &) = delete;

  ThreadPool(size_t);
  ~ThreadPool();

  /**
   * スレッドプールに仕事を依頼する
   * 返されるWaiterクラスを使って仕事が終了した通知を受けとることができる
   */
  std::shared_ptr<utils::Waiter> request(Task);

  /**
   * プールされているスレッドを停止する
   */
  void stop();

  /**
   * ハードウェアスレッドの個数から適当なスレッドプールを作り出す
   */
  static ThreadPool create_suitable_pool();
};
} // namespace prf
