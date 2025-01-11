#include "prf/types.hpp"
#include <atomic>
#include <condition_variable>
#include <map>
#include <mutex>
#include <type_traits>

namespace prf {
// 値の列に対して0を含む異なる自然数を適当に振り分ける
// 振り分けられる自然数の最大+1は返り値のmapの要素数と一致する
template <class T>
auto numbering(T xs)
    -> std::map<typename std::remove_reference<decltype(*xs.begin())>::type,
                u64> {
  std::map<typename std::remove_reference<decltype(*xs.begin())>::type, u64>
      numbered;
  for (auto x : xs) {
    numbered[x] = 0;
  }
  u64 num = 0;
  for (auto &i : numbered) {
    i.second = num;
    ++num;
  }
  return numbered;
}

// mapのキーとバリューを入れ替える
template <class T, class U> std::map<U, T> transpose(std::map<T, U> m) {
  std::map<U, T> res;
  for (auto i : m) {
    res[i.second] = i.first;
  }
  return res;
}

class Unit {};

namespace utils {
/**
 * スレッド間で待つのに使うクラス
 */
class Waiter {
private:
  std::atomic_bool already_done;
  std::mutex mtx;
  std::condition_variable cond;

public:
  Waiter(const Waiter &) = delete;
  Waiter &operator=(const Waiter &) = delete;

  Waiter();
  ~Waiter();

  /**
   * 待機しているスレッドに終了したことを通知する
   */
  void done();

  /**
   *  終了するまで大気する
   */
  void wait();

  /**
   * 終了しているかを確認する
   */
  bool sample();
};
} // namespace utils
} // namespace prf
