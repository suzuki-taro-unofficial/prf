#include "prf/transaction.hpp"
#include "cassert"
#include "prf/cluster.hpp"
#include "prf/executor.hpp"
#include "prf/logger.hpp"
#include "prf/time_invariant_values.hpp"
#include <atomic>
#include <mutex>

namespace prf {

bool InnerTransaction::is_in_updating() {
  return updating_cluster != ClusterManager::UNMANAGED_CLUSTER_ID;
}

InnerTransaction *
InnerTransaction::generate_sub_transaction(ID updating_cluster) {
  std::lock_guard<std::mutex> lock(this->mtx);
  InnerTransaction *trans = new InnerTransaction(id, updating_cluster);
  for (auto x : targets_outside_current_cluster[updating_cluster]) {
    trans->register_update(x);
  }
  return trans;
}

InnerTransaction::InnerTransaction(ID id, ID updating_cluster)
    : id(id), updating_cluster(updating_cluster), updating(false),
      inside_transaction(updating_cluster !=
                         ClusterManager::UNMANAGED_CLUSTER_ID) {}

InnerTransaction::InnerTransaction() {
  // 既にトランザクションがある場合はそちらを使う
  if (current_transaction != nullptr) {
    if (current_transaction->is_in_updating()) {
      failure_log("更新処理中にトランザクションを生成することはできません");
    }

    inside_transaction = true;
    id = current_transaction->get_id();
    return;
  }
  updating_cluster = ClusterManager::UNMANAGED_CLUSTER_ID;
  inside_transaction = false;
  updating = false;
  id = next_transaction_id.fetch_add(1);
  current_transaction = this;
}

InnerTransaction::~InnerTransaction() {
  // 別のトランザクションが外にある場合は何もしない
  if (inside_transaction) {
    return;
  }
  // 既に更新処理を終えているなら何もしない
  if (updating) {
    return;
  }
  start_updating();
  current_transaction = nullptr;
}

void InnerTransaction::register_update(TimeInvariantValues *tiv) {
  ID id = tiv->get_cluster_id();
  if (updating_cluster == id) {
    // 実行用のキューに同時に同一の時変値が複数詰まれないようにするためにこうする
    if (targets_inside_current_cluster.count(tiv) == 0) {
      targets_inside_current_cluster.insert(tiv);
      u64 rank = tiv->node->get_in_cluster_rank().value;
      std::pair<u64, TimeInvariantValues *> entry(rank, tiv);
      executor.push(entry);
    }
  } else {
    targets_outside_current_cluster[id].insert(tiv);
  }
}

void InnerTransaction::register_cleanup(TimeInvariantValues *tiv) {
  cleanups.insert(tiv);
}

void InnerTransaction::register_before_update_hook(
    std::function<void(ID)> hook) {
  this->before_update_hooks.push_back(hook);
}

ExecuteResult InnerTransaction::execute() {
  while (not executor.empty()) {
    auto entry = executor.top();
    executor.pop();
    TimeInvariantValues *tiv = entry.second;
    targets_inside_current_cluster.erase(tiv);
    tiv->update(this);
  }
  ExecuteResult result;
  result.cleanups = this->cleanups;
  result.targets = targets_outside_current_cluster;
  result.before_update_hooks = this->before_update_hooks;
  return result;
}

void InnerTransaction::start_updating() {
  TransactionExecuteMessage *msg = new TransactionExecuteMessage(this);
  ExecutorMessage emsg = msg;
  Executor::messages.push(emsg);
  msg->wait();
  delete msg;
}

std::set<ID> InnerTransaction::register_execution_result(ExecuteResult result) {
  if (this->is_in_updating()) {
    failure_log("更新用のトランザクションで呼び出すことを想定していません");
  }
  std::lock_guard<std::mutex> lock(this->mtx);
  std::set<ID> res;
  for (auto clustered_tivs : result.targets) {
    ID cluster_id = clustered_tivs.first;
    if (this->targets_outside_current_cluster.count(cluster_id) == 0) {
      res.insert(cluster_id);
    }
    for (auto tiv : clustered_tivs.second) {
      this->targets_outside_current_cluster[cluster_id].insert(tiv);
    }
  }
  for (auto cleanup : result.cleanups) {
    this->cleanups.insert(cleanup);
  }
  return res;
}

std::set<ID> InnerTransaction::target_clusters() {
  if (this->is_in_updating()) {
    failure_log("更新用のトランザクションで呼び出すことを想定していません");
  }
  std::set<ID> res;
  for (auto &i : this->targets_outside_current_cluster) {
    res.insert(i.first);
  }
  return res;
}

void InnerTransaction::finalize() {
  for (auto cleanup : this->cleanups) {
    cleanup->finalize(this);
  }
  for (auto cleanup : this->cleanups) {
    cleanup->refresh(this->get_id());
  }
}

ID InnerTransaction::get_id() { return id; }

std::atomic_ulong next_transaction_id(0);
thread_local InnerTransaction *current_transaction = nullptr;

JoinHandler::JoinHandler(TransactionExecuteMessage *message)
    : message(message) {}

JoinHandler::JoinHandler(JoinHandler &&other) {
  this->message = other.message;
  other.message = nullptr;
}

JoinHandler::~JoinHandler() {
  this->join();
  delete this->message;
}

void JoinHandler::join() {
  if (this->message == nullptr) {
    return;
  }
  this->message->wait();
}

Transaction::Transaction() {
  if (current_transaction == nullptr) {
    current_transaction = new InnerTransaction;
    this->inner = current_transaction;
  } else {
    this->inner = nullptr;
  }
}

Transaction::~Transaction() {
  if (this->inner != nullptr) {
    delete this->inner;
    current_transaction = nullptr;
  }
}

JoinHandler Transaction::get_join_handler() {
  if (this->inner == nullptr) {
    failure_log(
        "既にハンドラを取得しているか、このオブジェクトからJoinHandlerを取得す"
        "ることはできません");
  }

  TransactionExecuteMessage *msg = new TransactionExecuteMessage(this->inner);
  ExecutorMessage emsg = msg;
  Executor::messages.push(emsg);

  // グローバルのトランザクションを消す
  current_transaction = nullptr;

  this->inner = nullptr;

  return JoinHandler(msg);
}

} // namespace prf
