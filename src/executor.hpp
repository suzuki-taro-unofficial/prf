#pragma once
#include "concurrent_queue.hpp"
#include "transaction.hpp"
#include "types.hpp"
#include <condition_variable>
#include <functional>
#include <map>
#include <mutex>
#include <set>
#include <variant>

namespace prf {

class InnerTransaction;

/**
 * あるトランザクションの更新を開始するメッセージ
 */
class TransactionExecuteMessage {
  std::mutex mtx;
  std::condition_variable cond;
  bool already_done;

public:
  /**
   * 更新して欲しいトランザクション
   */
  InnerTransaction *transaction;

  TransactionExecuteMessage(InnerTransaction *transaction);
  /**
   * 更新処理が終了したことを通知する
   */
  void done();

  /**
   * 更新処理が終了するまでこのスレッドをブロッキングする
   */
  void wait();
};

/**
 * トランザクションにクラスタの更新を開始させるメッセージ
 */
class StartUpdateClusterMessage {
public:
  /**
   * 更新するトランザクション
   */
  ID transaction_id;
  /**
   * 更新対象のクラスタ
   */
  ID cluster_id;
};

/**
 * あるトランザクションの更新を終了することを通知するメッセージ
 */
class FinalizeTransactionMessage {
public:
  /**
   * 更新を終了するトランザクション
   */
  ID transaction_id;
};

/**
 * Executorが受け付けるメッセージの型
 */
using ExecutorMessage =
    std::variant<TransactionExecuteMessage *, StartUpdateClusterMessage,
                 FinalizeTransactionMessage>;

/**
 *トランザクションの更新処理をするクラス
 */
class Executor {
private:
  /**
   * Executorが管理しているクラスター
   * トランザクションID -> Message
   */
  std::map<ID, TransactionExecuteMessage *> transactions;

  /**
   * トランザクションが更新しているクラスターの一覧
   * トランザクションID -> {更新中(+済み)のクラスタ}
   */
  std::map<ID, std::set<ID>> transaction_updatings;

  std::map<ID, std::vector<std::function<void(ID)>>>
      before_update_hooks_buffers;

  std::vector<std::function<void(ID)>> before_update_hooks;

  static void invoke_after_build_hooks();

public:
  /**
   * Executorが起動していない場合に起動する
   */
  static void initialize();

  /**
   * Executorの処理を開始する
   */
  void start_loop();

  /**
   * Executorへのメッセージのキュー
   */
  static ConcurrentQueue<ExecutorMessage> messages;
  static Executor *global_executor;
  static std::mutex executor_mutex;

  /**
   * PRFのセットアップが終了したあとに呼び出す関数のリスト
   */
  static std::vector<std::function<void(InnerTransaction *)>> after_build_hooks;
};
} // namespace prf
