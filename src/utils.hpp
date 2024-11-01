#include "types.hpp"
#include <map>
#include <type_traits>

namespace prf {
// 値の列に対して0を含む異なる自然数を適当に振り分ける
// 振り分けられる自然数+1は返り値のmapの要素数と一致する
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
} // namespace prf
