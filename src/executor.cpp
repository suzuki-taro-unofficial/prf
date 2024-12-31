#include "executor.hpp"
#include "concurrent_queue.hpp"
#include "planner.hpp"
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

      continue;
    }
    if (std::holds_alternative<StartUpdateClusterMessage>(msg)) {
      StartUpdateClusterMessage &ftmsg =
          std::get<StartUpdateClusterMessage>(msg);

      // とりあえず何もしない
      continue;
    }
    if (std::holds_alternative<FinalizeTransactionMessage>(msg)) {
      FinalizeTransactionMessage &ftmsg =
          std::get<FinalizeTransactionMessage>(msg);

      // とりあえず何もしない
      continue;
    }
  }
}

} // namespace prf
