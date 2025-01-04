#include "executor.hpp"
#include "concurrent_queue.hpp"
#include "logger.hpp"
#include "planner.hpp"
#include "time_invariant_values.hpp"
#include <thread>
#include <variant>

namespace prf {
TransactionExecuteMessage::TransactionExecuteMessage(Transaction *transaction)
    : transaction(transaction), already_done(false) {}

void TransactionExecuteMessage::done() {
  std::lock_guard<std::mutex> lock(mtx);
  already_done = true;
  cond.notify_all();
}

void TransactionExecuteMessage::wait() {
  std::unique_lock<std::mutex> lock(mtx);
  cond.wait(lock, [this] { return this->already_done; });
}

void Executor::initialize() {
  std::lock_guard<std::mutex> lock(executor_mutex);
  if (global_executor == nullptr) {
    global_executor = new Executor();
    // global_executorの処理はバックグラウンドのスレッドで行なう
    std::thread t([] { global_executor->start_loop(); });
    // 一度起動したらそのまま放置(異常時の対処は一旦放置)
    t.detach();
  }
}

void Executor::start_loop() {
  while (true) {
    ExecutorMessage msg = messages.pop();

    if (std::holds_alternative<TransactionExecuteMessage *>(msg)) {
      TransactionExecuteMessage *&temsg =
          std::get<TransactionExecuteMessage *>(msg);

      ID transaction_id = temsg->transaction->get_id();

      this->transactions[transaction_id] = temsg;

      // Plannerにトランザクションの開始を通知
      StartTransactionMessage stmsg;
      stmsg.transaction_id = transaction_id;
      PlannerManager::messages.push(stmsg);

      std::set<ID> clusters = temsg->transaction->target_clusters();
      UpdateTransactionMessage utmsg;
      utmsg.transaction_id = transaction_id;
      for (ID cluster : clusters) {
        utmsg.future.push_back(cluster);
      }
      PlannerManager::messages.push(utmsg);

      continue;
    }
    if (std::holds_alternative<StartUpdateClusterMessage>(msg)) {
      StartUpdateClusterMessage &ftmsg =
          std::get<StartUpdateClusterMessage>(msg);

      ID transaction_id = ftmsg.transaction_id;
      ID cluster_id = ftmsg.cluster_id;

      if (this->transactions.count(transaction_id) == 0) {
        warn_log("トランザクションがExecutorに登録されていません ID: %ld",
                 transaction_id);
        continue;
      }

      if (transaction_updatings[transaction_id].count(cluster_id) != 0) {
        // 既に更新している場合はスキップする
        continue;
      }

      Transaction *transaction =
          this->transactions[transaction_id]->transaction;
      Transaction *subtransaction =
          transaction->generate_sub_transaction(cluster_id);

      {
        // 更新が開始したことを通知
        UpdateTransactionMessage utmsg;
        utmsg.transaction_id = transaction_id;
        utmsg.now.push_back(transaction_id);
        PlannerManager::messages.push(utmsg);
      }

      // 更新は取り敢えずこのスレッド上で行なう
      ExecuteResult result = subtransaction->execute();
      std::set<ID> futures = transaction->register_execution_result(result);

      {
        // 更新の終了を通知
        UpdateTransactionMessage utmsg;
        utmsg.transaction_id = transaction_id;
        for (auto future : futures) {
          utmsg.future.push_back(future);
        }
        utmsg.finish.push_back(transaction_id);
        PlannerManager::messages.push(utmsg);
      }

      continue;
    }
    if (std::holds_alternative<FinalizeTransactionMessage>(msg)) {
      FinalizeTransactionMessage &ftmsg =
          std::get<FinalizeTransactionMessage>(msg);

      ID transaction_id = ftmsg.transaction_id;

      if (this->transactions.count(transaction_id) == 0) {
        warn_log("トランザクションがExecutorに登録されていません ID: %ld",
                 transaction_id);
        continue;
      }

      this->transactions[transaction_id]->transaction->finalize();
      this->transactions[transaction_id]->done();

      {
        // トランザクションの終了をPlannerに通知
        FinishTransactionMessage ftmsg;
        ftmsg.transaction_id = transaction_id;

        PlannerManager::messages.push(ftmsg);
      }
      continue;
    }
  }
}

} // namespace prf
