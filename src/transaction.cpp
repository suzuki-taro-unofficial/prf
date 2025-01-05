#include "transaction.hpp"
#include "cassert"
#include "cluster.hpp"
#include "executor.hpp"
#include "time_invariant_values.hpp"
#include <atomic>

namespace prf {

bool Transaction::is_in_updating() {
  return updating_cluster != ClusterManager::UNMANAGED_CLUSTER_ID;
}

Transaction *Transaction::generate_sub_transaction(ID updating_cluster) {
  Transaction *trans = new Transaction(id, updating_cluster);
  for (auto x : targets_outside_current_cluster[updating_cluster]) {
    trans->register_update(x);
  }
  return trans;
}

Transaction::Transaction(ID id, ID updating_cluster)
    : id(id), updating_cluster(updating_cluster),
      inside_transaction(updating_cluster !=
                         ClusterManager::UNMANAGED_CLUSTER_ID) {}

Transaction::Transaction() {
  // 既にトランザクションがある場合はそちらを使う
  if (current_transaction != nullptr) {
    assert((not current_transaction->is_in_updating()) &&
           "更新処理中にトランザクションを生成することはできません");
    inside_transaction = true;
    id = current_transaction->get_id();
    return;
  }
  updating_cluster = ClusterManager::UNMANAGED_CLUSTER_ID;
  inside_transaction = false;
  id = next_transaction_id.fetch_add(1);
  current_transaction = this;
}

Transaction::~Transaction() {
  // 別のトランザクションが外にある場合は何もしない
  if (inside_transaction) {
    return;
  }
  start_updating();
  current_transaction = nullptr;
}

void Transaction::register_update(TimeInvariantValues *tiv) {
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

ExecuteResult Transaction::execute() {
  std::set<TimeInvariantValues *> cleanups;
  while (not executor.empty()) {
    auto entry = executor.top();
    executor.pop();
    TimeInvariantValues *tiv = entry.second;
    targets_inside_current_cluster.erase(tiv);
    tiv->update(this);
  }
  ExecuteResult result;
  result.cleanups = cleanups;
  result.targets = targets_outside_current_cluster;
  return result;
}

void Transaction::start_updating() {
  TransactionExecuteMessage *msg = new TransactionExecuteMessage(this);
  ExecutorMessage emsg = msg;
  Executor::messages.push(emsg);
  msg->wait();
  delete msg;
  delete this;
}

std::set<ID> Transaction::register_execution_result(ExecuteResult result) {
  assert(not this->is_in_updating() &&
         "更新用のトランザクションで呼び出すことを想定していません");
  std::set<ID> res;
  for (auto clustered_tivs : result.targets) {
    ID cluster_id = clustered_tivs.first;
    if (this->targets_outside_current_cluster.count(cluster_id) == 0) {
      res.insert(id);
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

std::set<ID> Transaction::target_clusters() {
  assert(not this->is_in_updating() &&
         "更新用のトランザクションで呼び出すことを想定していません");
  std::set<ID> res;
  for (auto &i : this->targets_outside_current_cluster) {
    res.insert(i.first);
  }
  return res;
}

void Transaction::finalize() {
  for (auto cleanup : this->cleanups) {
    cleanup->finalize(this);
  }
  for (auto cleanup : this->cleanups) {
    cleanup->refresh(this->get_id());
  }
}

std::atomic_ulong next_transaction_id(0);
thread_local Transaction *current_transaction = nullptr;

} // namespace prf
