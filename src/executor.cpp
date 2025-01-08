#include "executor.hpp"
#include "concurrent_queue.hpp"
#include "logger.hpp"
#include "planner.hpp"
#include "thread.hpp"
#include "thread_pool.hpp"
#include "time_invariant_values.hpp"
#include "transaction.hpp"
#include <mutex>
#include <thread>
#include <variant>

namespace prf {
TransactionExecuteMessage::TransactionExecuteMessage(
    InnerTransaction *transaction)
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
    std::thread t([] {
      global_executor->start_loop();
      wait_threads.fetch_sub(1);
    });
    // 一度起動したらそのまま放置(異常時の対処は一旦放置)
    t.detach();
    Executor::invoke_after_build_hooks();
  }
}

void Executor::start_loop() {
  while (true) {
    std::optional<ExecutorMessage> omsg = messages.pop();
    if (not omsg) {
      break;
    }
    ExecutorMessage msg = *omsg;

    if (std::holds_alternative<TransactionExecuteMessage *>(msg)) {
      TransactionExecuteMessage *&temsg =
          std::get<TransactionExecuteMessage *>(msg);

      ID transaction_id = temsg->transaction->get_id();

      info_log("新しいトランザクションが開始しました ID: %ld", transaction_id);

      this->invoke_before_update_hooks(transaction_id);
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
      if (this->transaction_updatings[transaction_id].count(cluster_id) != 0) {
        // 既に更新している場合はスキップする
        continue;
      }
      this->transaction_updatings[transaction_id].insert(cluster_id);

      info_log("クラスタの更新を依頼されました Transaction: %ld, "
               "Cluster: %ld",
               transaction_id, cluster_id);

      InnerTransaction *transaction =
          this->transactions[transaction_id]->transaction;

      this->thread_pool.request([this, transaction, transaction_id,
                                 cluster_id]() -> void {
        {
          // 更新が開始したことを通知
          UpdateTransactionMessage utmsg;
          utmsg.transaction_id = transaction_id;
          utmsg.now.push_back(cluster_id);
          PlannerManager::messages.push(utmsg);
        }

        InnerTransaction *subtransaction =
            transaction->generate_sub_transaction(cluster_id);

        // current_transactionをsubtransactionに設定してから更新する
        current_transaction = subtransaction;
        ExecuteResult result = subtransaction->execute();
        current_transaction = nullptr;

        std::set<ID> futures = transaction->register_execution_result(result);

        {
          std::lock_guard<std::mutex> lock(this->before_update_hooks_mtx);
          for (auto hook : result.before_update_hooks) {
            this->before_update_hooks_buffers[transaction_id].push_back(hook);
          }
        }

        {
          // 更新の終了を通知
          UpdateTransactionMessage utmsg;
          utmsg.transaction_id = transaction_id;
          for (auto future : futures) {
            utmsg.future.push_back(future);
          }
          utmsg.finish.push_back(cluster_id);
          PlannerManager::messages.push(utmsg);
        }

        info_log("クラスタの更新が終了しました Transaction: %ld, "
                 "Cluster: %ld",
                 transaction_id, cluster_id);
      });
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

      info_log("トランザクションの終了を依頼されました ID: %ld",
               transaction_id);

      this->transactions[transaction_id]->transaction->finalize();
      this->transactions[transaction_id]->done();

      this->transactions.erase(transaction_id);
      this->transaction_updatings.erase(transaction_id);

      {
        std::lock_guard<std::mutex> lock(this->before_update_hooks_mtx);
        for (auto hook : this->before_update_hooks_buffers[transaction_id]) {
          this->before_update_hooks.push_back(hook);
        }
        this->before_update_hooks_buffers.erase(transaction_id);
      }

      {
        // トランザクションの終了をPlannerに通知
        FinishTransactionMessage ftmsg;
        ftmsg.transaction_id = transaction_id;
        PlannerManager::messages.push(ftmsg);
      }
      continue;
    }
  }
  info_log("Executorの実行を停止します");
  this->thread_pool.stop();
}

void Executor::invoke_after_build_hooks() {
  InnerTransaction transaction;
  for (auto &hook : after_build_hooks) {
    hook(&transaction);
  }
  after_build_hooks.clear();
}

std::vector<std::function<void(InnerTransaction *)>>
    Executor::after_build_hooks =
        std::vector<std::function<void(InnerTransaction *)>>();

ConcurrentQueue<ExecutorMessage> Executor::messages;
Executor *Executor::global_executor = nullptr;
std::mutex Executor::executor_mutex;

Executor::Executor() : thread_pool(ThreadPool::create_suitable_pool()) {}

void Executor::invoke_before_update_hooks(ID transaction_id) {
  std::lock_guard<std::mutex> lock(this->before_update_hooks_mtx);
  for (auto hook : this->before_update_hooks) {
    hook(transaction_id);
  }
  this->before_update_hooks.clear();
}

} // namespace prf
