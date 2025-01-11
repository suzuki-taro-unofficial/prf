#pragma once

#include "prf/executor.hpp"
#include "prf/time_invariant_values.hpp"
#include "prf/types.hpp"
#include <atomic>
#include <functional>
#include <map>
#include <queue>
#include <set>
#include <vector>

namespace prf {

class TransactionExecuteMessage;
class TimeInvariantValues;
class InnerTransaction;

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

  /**
   * トランザクションが終了時に不要な値の破棄が必要な時変値の集合
   */
  std::vector<std::function<void(ID)>> before_update_hooks;
};

class JoinHandler {
private:
  TransactionExecuteMessage *message;

public:
  /**
   * コピーコンストラクタは呼び出せないようにしておく
   */
  JoinHandler(const JoinHandler &) = delete;
  JoinHandler &operator=(const JoinHandler &) = delete;

  /**
   * JoinHandlerはstd::moveでしか移動できないようにする
   */
  JoinHandler(JoinHandler &&);

  JoinHandler(TransactionExecuteMessage *);
  ~JoinHandler();

  void join();
};

/**
 * 内部で利用するトランザクション
 */
class InnerTransaction {
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
   * このオブジェクトに関連したトランザクションが既に更新処理を初めているか
   */
  bool updating;

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

  std::vector<std::function<void(ID)>> before_update_hooks;

  /**
   * 更新処理を開始する
   */
  void start_updating();

  /**
   * 更新処理のときは排他ロックを取る
   */
  std::mutex mtx;

  InnerTransaction(ID id, ID updating_cluster);

public:
  InnerTransaction();

  ~InnerTransaction();

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
   * トランザクション開始前に呼び出す必要のある関数を登録する
   */
  void register_before_update_hook(std::function<void(ID)>);

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
  InnerTransaction *generate_sub_transaction(ID updating_cluster);

  /**
   * トランザクションの終了処理をする
   */
  void finalize();
};

extern std::atomic_ulong next_transaction_id;

// 現在のスレッドで動作しているトランザクション
// 存在しなければnullptrになる
thread_local extern InnerTransaction *current_transaction;

/**
 * ユーザーがトランザクションを制御するために作成するクラス
 */
class Transaction {
private:
  InnerTransaction *inner;

public:
  Transaction(const Transaction &) = delete;
  Transaction &operator=(const Transaction &) = delete;

  Transaction();
  ~Transaction();

  /**
   * トランザクションの終了処理をJoinHandlerに移譲する
   * これを呼び出すと ~Transaction()
   * ではブロッキングが発生しなくなり、更新をしたい場合は返される JoinHandler の
   * join() を呼び出す必要がある
   * また、このメソッドを呼び出すことでトランザクションがそこで終了するので、それ以降はsend等をした際は他のトランザションの管轄となる
   */
  JoinHandler get_join_handler();
};

} // namespace prf
