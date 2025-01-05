#pragma once

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
class Transaction;

/**
 * クラスタの更新が終了時にトランザクションへ返すデータ
 */
struct ExecuteResult {
  /**
   * 更新が必要な時変値の一覧
   * IDがクラスタのIDを表している
   */
  std::map<ID, std::set<TimeInvariantValues *>> targets;
  /**
   * トランザクションが終了時に不要な値の破棄が必要な時変値の集合
   */
  std::set<TimeInvariantValues *> cleanups;
};

class Transaction {
private:
  /**
   * このトランザクションのID
   */
  ID id;

  /**
   * このトランザクションが更新中のクラスター
   * UNMANAGED_CLUSTER_ID
   * 以外(XXXSinkでない)に所属するノードを更新中にトランザクションを新しく生成することはできない
   */
  ID updating_cluster;

  /**
   * このオブジェクトの外にもトランザクションが存在するか
   */
  bool inside_transaction;

  /**
   * 更新中のクラスターで更新が必要な時変値の一覧
   */
  std::set<TimeInvariantValues *> targets_inside_current_cluster;

  /**
   * クラスター内の実行をするための優先度付きキュー
   */
  std::priority_queue<std::pair<u64, TimeInvariantValues *>,
                      std::vector<std::pair<u64, TimeInvariantValues *>>,
                      std::greater<std::pair<u64, TimeInvariantValues *>>>
      executor;

  /**
   * 更新中のクラスター以外で更新が必要な時変値の一覧
   */
  std::map<ID, std::set<TimeInvariantValues *>> targets_outside_current_cluster;

  /**
   * トランザクションが終了時に不要な値の破棄が必要な時変値の集合
   */
  std::set<TimeInvariantValues *> cleanups;

  /**
   * 更新処理を開始する
   */
  void start_updating();

  Transaction(ID id, ID updating_cluster);

public:
  Transaction();

  ~Transaction();

  /**
   * このトランザクションが更新処理を実行中であるか
   */
  bool is_in_updating();

  /**
   * このトランザクションのIDを取得
   */
  ID get_id();

  /**
   * 更新の必要な時変値を登録する
   */
  void register_update(TimeInvariantValues *tiv);

  /**
   * 後処理の必要な時変値を登録する
   */
  void register_cleanup(TimeInvariantValues *tiv);

  /**
   * 子トランザクションの実行結果を親トランザクションに登録する
   * 返り値として新規で更新する必要になったクラスタを返す
   */
  std::set<ID> register_execution_result(ExecuteResult result);

  /**
   * 更新予定(+ 済み)のクラスターの一覧を返す
   */
  std::set<ID> target_clusters();

  /**
   * このインスタンスの担当範囲について更新する
   */
  ExecuteResult execute();

  /**
   * 更新処理を行なうスレッドのためのサブトランザクションを生成する
   */
  Transaction *generate_sub_transaction(ID updating_cluster);

  /**
   * トランザクションの終了処理をする
   */
  void finalize();
};

extern std::atomic_ulong next_transaction_id;

// 現在のスレッドで動作しているトランザクション
// 存在しなければnullptrになる
thread_local extern Transaction *current_transaction;
} // namespace prf
