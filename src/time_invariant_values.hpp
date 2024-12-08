#pragma once

#include "node.hpp"
#include "types.hpp"
#include <vector>

namespace prf {
    // 時変値を表すクラス
    // StreamとCellが共通して持つべき性質を切り出しているだけなので、これを継承したクラスを利用すること
class TimeInvariantValues {
public:
  // transaction_idに対応するトランザクションでの更新をする
  virtual void update(ID transaction_id);
  // transaction_idに対応するトランザクションとそれ以前での時変値が不要である場合に消去する
  virtual void refresh(ID transaction_id);
};
} // namespace prf
