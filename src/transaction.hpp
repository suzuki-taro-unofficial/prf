#pragma once

#include "node.hpp"
#include "time_invariant_values.hpp"
#include "types.hpp"
#include <atomic>
#include <functional>
#include <map>
#include <queue>
#include <set>
#include <vector>

namespace prf {
class TimeInvariantValues;

// クラスタの更新が終了時にトランザクションへ返すデータ
struct ExecuteResult {
  // 更新が必要な時変値の一覧
  // IDがクラスタのIDを表している
  std::map<ID, std::set<TimeInvariantValues *>> targets;
  // トランザクションが終了時に不要な値の破棄が必要な時変値の集合
  std::set<TimeInvariantValues *> cleanups;
};

class Transaction {
private:
  // このトランザクションのID
  ID id;

  // このトランザクションが更新中のクラスター
  // IDが0以外(XXXSinkでない)ノードを更新中にトランザクションを新しく生成することはできない
  ID updating_cluster;

  // このオブジェクトの外にもトランザクションが存在するか
  bool inside_transaction;

  // 更新中のクラスターで更新が必要な時変値の一覧
  std::set<TimeInvariantValues *> targets_inside_current_cluster;

  // クラスター内の実行をするための優先度付きキュー
  std::priority_queue<std::pair<u64, TimeInvariantValues *>,
                      std::vector<std::pair<u64, TimeInvariantValues *>>,
                      std::greater<std::pair<u64, TimeInvariantValues *>>>
      executor;
  // 更新中のクラスター以外で更新が必要な時変値の一覧
  std::map<ID, std::set<TimeInvariantValues *>> targets_outside_current_cluster;

  // 更新処理を行なうスレッドのためのサブトランザクションを生成する
  Transaction *generate_sub_transaction(ID updating_cluster);

  Transaction(ID id, ID updating_cluster);

public:
  Transaction();

  ~Transaction();

  // このトランザクションが更新処理を実行中であるか
  bool is_in_updating();

  ID get_id();

  // 更新の必要な時変値を登録する
  void register_update(TimeInvariantValues *tiv);

  ExecuteResult execute(ID transaction_id);
};

extern std::atomic_ulong next_transaction_id;

// 現在のスレッドで動作しているトランザクション
// 存在しなければnullptrになる
thread_local extern Transaction *current_transaction;
} // namespace prf
