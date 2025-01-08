#include "planner.hpp"
#include "concurrent_queue.hpp"
#include "executor.hpp"
#include "logger.hpp"
#include "prf.hpp"
#include "rank.hpp"
#include "thread.hpp"
#include <atomic>
#include <limits>
#include <set>
#include <thread>
#include <variant>

namespace prf {
void PlannerManager::handleMessage(PlannerMessage message) {
  if (std::holds_alternative<StartTransactionMessage>(message)) {
    const StartTransactionMessage &msg =
        std::get<StartTransactionMessage>(message);
    this->handleStartMessage(msg);
    return;
  }
  if (std::holds_alternative<UpdateTransactionMessage>(message)) {
    const UpdateTransactionMessage &msg =
        std::get<UpdateTransactionMessage>(message);
    this->handleUpdateMessage(msg);
    return;
  }
  if (std::holds_alternative<FinishTransactionMessage>(message)) {
    const FinishTransactionMessage &msg =
        std::get<FinishTransactionMessage>(message);
    this->handleFinishMessage(msg);
    return;
  }
  warn_log("メッセージが適切に処理されなかった");
  return; // ここには来ないはず
}

bool need_refresh_message(const PlannerMessage &message) {
  if (std::holds_alternative<UpdateTransactionMessage>(message)) {
    return true;
  }
  if (std::holds_alternative<FinishTransactionMessage>(message)) {
    return true;
  }
  return false;
}

TransactionState::TransactionState(ID transaction_id)
    : transaction_id(transaction_id), initialized(false), now(), future(),
      target_ranks() {}

void PlannerManager::handleStartMessage(
    const StartTransactionMessage &message) {
  // キューが空のときはそのまま追加する
  if (this->transaction_states.empty()) {
    TransactionState add(message.transaction_id);
    this->transaction_states.push_back(add);
  } else {
    // そうでない場合はIDに対応する状態を追加する
    // ただし、transaction_states
    // はIDが連番で入ることを期待しているので、順番に入れる
    while (this->transaction_states.back().transaction_id <
           message.transaction_id) {
      TransactionState add(this->transaction_states.back().transaction_id + 1);
      this->transaction_states.push_back(add);
    }
  }
}

void PlannerManager::handleUpdateMessage(
    const UpdateTransactionMessage &message) {
  if (this->transaction_states.empty()) {
    warn_log("現在アクティブなトランザクションが存在しない");
    return;
  }
  ID id_min = this->transaction_states.front().transaction_id;
  ID id_max = this->transaction_states.back().transaction_id;
  ID id_arg = message.transaction_id;
  {

    if (id_arg < id_min or id_max < id_arg) {

      warn_log("対応するトランザクションが存在しなかった (min: %lu, max: "
               "%lu, arg: %lu",
               id_min, id_max, id_arg);
      return;
    }
  }

  info_log("トランザクションの状態の変更が通知されました ID: %ld", id_arg);

  // transaction_statesは連番で入っていることを期待しているので
  // 下記のようにランダムアクセスできる
  u64 idx = (u64)id_arg - (u64)id_min;
  TransactionState &state = this->transaction_states[idx];

  for (const ID id : message.future) {
    if (state.future.count(id) == 0) {
      state.future.insert(id);
      ++state.target_ranks[this->cluster_ranks[id].value];
    }
  }
  for (const ID id : message.now) {
    auto itr = state.future.find(id);
    if (itr == state.future.end()) {
      warn_log(
          "事前に実行する予定と通知されていないトランザクションを更新している "
          "(transaction_id: %lu, cluster_id: %lu))",
          id_arg, id);
    } else {
      state.future.erase(itr);
    }
    state.now.insert(id);
  }
  for (const ID id : message.finish) {
    auto itr = state.now.find(id);
    if (itr == state.now.end()) {
      warn_log("実行中と通知されていないトランザクションを終了している "
               "(transaction_id: %lu, cluster_id: %lu))",
               id_arg, id);
    } else {
      state.now.erase(itr);
      u64 rank = this->cluster_ranks[id].value;
      auto itr = state.target_ranks.find(rank);
      if (itr->second == 1) {
        state.target_ranks.erase(itr);
      } else {
        --itr->second;
      }
    }
  }

  // 状態が更新されたトランザクションは初期化されたものと見做す
  state.initialized = true;
}

void PlannerManager::handleFinishMessage(
    const FinishTransactionMessage &message) {
  ID id_arg = message.transaction_id;
  if (this->transaction_states.empty()) {
    warn_log("対応するトランザクションが存在しません (transaction_id: %lu)",
             id_arg);
    return;
  }
  ID id_min = this->transaction_states.front().transaction_id;

  if (id_min != id_arg) {
    warn_log("現存する一番古いトランザクション以外は終了できません "
             "(transaction_id: %lu)",
             message.transaction_id);
    return;
  }
  // トランザクションが初期化されているかを確認していないが、問題無いはず
  this->transaction_states.pop_front();
}

PlannerManager::PlannerManager(std::vector<Rank> cluster_ranks,
                               std::vector<Planner> planners)
    : cluster_ranks(cluster_ranks), transaction_states(),
      stop_planning_needed(false), planners(planners) {}

void PlannerManager::start_planning() {
  for (auto &planner : this->planners) {
    std::thread t([this, &planner]() {
      planner(this->cluster_ranks, this->transaction_states, Executor::messages,
              this->stop_planning_needed);
    });
    this->running_planners.push_back(std::move(t));
  }
}

void PlannerManager::stop_planning() {
  this->stop_planning_needed.store(true);
  for (auto &planner : this->running_planners) {
    planner.join();
  }
  this->running_planners.clear();
  this->stop_planning_needed.store(false);
}

void PlannerManager::start_loop() {
  while (true) {
    std::optional<PlannerMessage> omsg = PlannerManager::messages.pop();
    if (not omsg) {
      break;
    }
    bool need_refresh = need_refresh_message(*omsg);
    if (need_refresh) {
      this->stop_planning();
    }
    this->handleMessage(*omsg);
    if (need_refresh) {
      this->start_planning();
    }
  }
  this->stop_planning();
  info_log("PlannerManagerの実行を停止します");
}

void PlannerManager::initialize(std::vector<Rank> ranks) {
  std::vector<Planner> planners({simple_planner});
  if (use_parallel_execution) {
    // デバッグをやりやすくするため、一旦Plannerは同時に一つまでにしておく
    planners.clear();
    planners.push_back(rank_based_planner);
  }
  globalPlannerManager =
      new PlannerManager(ranks, std::vector<Planner>(planners));
  PlannerManager *ptr = globalPlannerManager;
  std::thread t([ptr]() -> void {
    ptr->start_loop();
    wait_threads.fetch_sub(1);
  });
  t.detach();
}

void simple_planner(std::vector<Rank> &cluster_ranks,
                    std::deque<TransactionState> &transaction_states,
                    ConcurrentQueue<ExecutorMessage> &executor_message_queue,
                    std::atomic_bool &stop) {
  // シンプルな実行計画
  // 一番新しいトランザクションにクラスタを割り当てて終了
  if (transaction_states.empty()) {
    return;
  }
  TransactionState &state = transaction_states.front();
  // 初期化されていないなら割り当てをしてはいけない
  if (not state.initialized) {
    return;
  }
  if (state.now.empty() and state.future.empty()) {
    // 更新できるクラスタがもう無い場合は終了する
    FinalizeTransactionMessage msg;
    msg.transaction_id = state.transaction_id;
    executor_message_queue.push(msg);
    return;
  }
  if (state.now.empty()) {
    // 何も実行していないなら新しく割り当てる
    // 一番ランクの値が少ないクラスタを割り当てる
    u64 target_rank = state.target_ranks.begin()->first;

    for (auto cluster_id : state.future) {
      if (cluster_ranks[cluster_id] == target_rank) {
        StartUpdateClusterMessage msg;
        msg.transaction_id = state.transaction_id;
        msg.cluster_id = cluster_id;
        executor_message_queue.push(msg);
        return;
      }
    }
  } else {
    // 何かを実行中だったら新しく割り当てない
  }
}

void rank_based_planner(
    std::vector<Rank> &cluster_ranks,
    std::deque<TransactionState> &transaction_states,
    ConcurrentQueue<ExecutorMessage> &executor_message_queue,
    std::atomic_bool &stop) {
  info_log("rank_based_plannerの作業を開始します");

  // 自分より先のトランザクションが使用しているクラスターの仲で一番低いランク
  ID target_rank = std::numeric_limits<ID>::max();
  // target_rank
  // のクラスターの中で自分より先のトランザクションが使用する可能性のあるクラスター
  std::set<ID> used_clusters;
  // 今見ているトランザクションがキューの中で一番若いか
  bool is_head = true;

  // 先に産まれたトランザクションを順に割り当てていく
  for (size_t i = 0; i < transaction_states.size(); ++i) {
    // 終了命令が来ていたら終了する
    if (stop.load()) {
      info_log("rank_based_plannerの作業を外部の信号により終了します");
      break;
    }
    TransactionState &state = transaction_states[i];
    // トランザクションの初期化が未だである場合はそれ以降の作業を終了する
    if (not state.initialized) {
      break;
    }

    for (ID now : state.now) {
      u64 rank = cluster_ranks[now].value;
      if (rank < target_rank) {
        target_rank = rank;
        used_clusters.clear();
      }
      used_clusters.insert(now);
    }
    for (ID future : state.future) {
      u64 rank = cluster_ranks[future].value;
      if (target_rank < rank) {
        // 他のトランザクションが触るかもしれないので考えない
        continue;
      }
      if (rank < target_rank) {
        target_rank = rank;
        used_clusters.clear();
      }
      if (used_clusters.count(future)) {
        // 他のトランザクションが既に触っているなら割り当てない
        continue;
      }
      used_clusters.insert(future);
      StartUpdateClusterMessage msg;
      msg.transaction_id = state.transaction_id;
      msg.cluster_id = future;
      executor_message_queue.push(msg);
    }
    if (is_head and state.future.empty() and state.now.empty()) {
      FinalizeTransactionMessage msg;
      msg.transaction_id = state.transaction_id;
      executor_message_queue.push(msg);
    }
    is_head = false;
  }

  info_log("rank_based_plannerの作業が無くなったため終了します");
}

ConcurrentQueue<PlannerMessage> PlannerManager::messages;
PlannerManager *PlannerManager::globalPlannerManager = nullptr;

} // namespace prf
