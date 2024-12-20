#pragma once

#include "node.hpp"
#include "transaction.hpp"
#include "types.hpp"

namespace prf {
class Transaction;

// 時変値を表すクラス
// StreamとCellが共通して持つべき性質を切り出しているだけなので、これを継承したクラスを利用すること
class TimeInvariantValues {
private:
  // この時変値を参照している時変値の更新をトランザクションに登録する
  void register_listerns_update(Transaction *transaction);

  std::vector<TimeInvariantValues *> listners;

public:
  // この時変値のノード
  // NodeManagerがNodeへの参照で管理されているので、苦し紛れだがこういう実装にしておく(後で直す)
  Node *node;

  TimeInvariantValues(ID cluster_id);

  // transaction_idに対応するトランザクションでの更新をする
  virtual void update(ID transaction_id);
  // transaction_idに対応するトランザクションとそれ以前での時変値が不要である場合に消去する
  virtual void refresh(ID transaction_id);

  ID get_cluster_id();

  // 引数の時変値に更新があったときに連動して更新されるようにする
  void listen(TimeInvariantValues *to);
  // 引数の時変値に更新があったときに連動して更新されるようにする(CellLoopやStreamLoop用)
  void listen_over_loop(TimeInvariantValues *to);
};
} // namespace prf
