#include "executor.hpp"
#include "concurrent_queue.hpp"
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
  }
}

void Executor::start_loop() {
  while (true) {
    ExecutorMessage msg = messages.pop();

    if (std::holds_alternative<TransactionExecuteMessage>(msg)) {
      TransactionExecuteMessage &temsg =
          std::get<TransactionExecuteMessage>(msg);

      // とりあえず終了しておく
      temsg.done();
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
