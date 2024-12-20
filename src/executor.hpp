#pragma once
#include "concurrent_queue.hpp"
#include "transaction.hpp"
#include <condition_variable>
#include <mutex>
#include <variant>

namespace prf {

/**
 * あるトランザクションの更新を開始するメッセージ
 */
class TransactionExecuteMessage {
  /**
   * 更新して欲しいトランザクション
   */
  Transaction *transaction;

  std::mutex mtx;
  std::condition_variable cond;
  bool already_done;

public:
  TransactionExecuteMessage(Transaction *transaction);

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
    std::variant<TransactionExecuteMessage, StartUpdateClusterMessage, FinalizeTransactionMessage>;

/**
 *トランザクションの更新処理をするクラス
 */
class Executor {
private:
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
};
} // namespace prf
