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
  // この時変値を参照している時変値のリスト
  std::vector<TimeInvariantValues *> listners;

protected:
  // この時変値を参照している時変値の更新をトランザクションに登録する
  void register_listeners_update(Transaction *transaction);

  /**
   * この時変値がトランザクションの終了時に後処理の必要があることを登録する
   * このメソッドで登録しておかないと値が残り続けるので注意
   */
  void register_cleanup(Transaction *transaction);

public:
  // この時変値のノード
  // NodeManagerがNodeへの参照で管理されているので、苦し紛れだがこういう実装にしておく(後で直す)
  Node *node;

  TimeInvariantValues(ID cluster_id);

  // 現在のトランザクションでの更新をする
  virtual void update(Transaction *transaction);
  // transaction_idに対応するトランザクションとそれ以前での時変値が不要である場合に消去する
  virtual void refresh(ID transaction_id);

  /**
   * トランザクションの終了時に必要な処理を行なう
   */
  virtual void finalize(Transaction *transaction);

  ID get_cluster_id();

  // 引数の時変値に更新があったときに連動して更新されるようにする
  void listen(TimeInvariantValues *to);
  // 引数の時変値に更新があったときに連動して更新されるようにする(CellLoopやStreamLoop用)
  void listen_over_loop(TimeInvariantValues *to);
  /**
   * 引数の時変値に更新があったときに連動して更新はされないが、子要素ではあるようにする
   */
  void child_to(TimeInvariantValues *to);
};
} // namespace prf
