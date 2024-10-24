#pragma once

#include "types.hpp"

// 依存関係に割り当てる値
namespace prf {
class Rank {
public:
  u64 value;

  Rank();
  Rank(u64);

  // 引数で渡したRankが自分よりも大きいことを確認し、そうでない場合は更新する
  // Rankに更新があるときだけtrueを返す
  bool ensure_after(Rank &);
};

bool operator<(const Rank &, const Rank &);
bool operator>(const Rank &, const Rank &);
bool operator==(const Rank &, const Rank &);
}; // namespace prf
