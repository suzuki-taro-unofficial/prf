#include "planner.hpp"
#include "executor.hpp"
#include "logger.hpp"
#include "rank.hpp"

namespace prf {
bool PlannerManager::handleMessage(PlannerMessage message) {
  if (std::holds_alternative<StartTransactionMessage>(message)) {
    const StartTransactionMessage &msg =
        std::get<StartTransactionMessage>(message);
    this->handleStartMessage(msg);
    return false;
  }
  if (std::holds_alternative<UpdateTransactionMessage>(message)) {
    const UpdateTransactionMessage &msg =
        std::get<UpdateTransactionMessage>(message);
    this->handleUpdateMessage(msg);
    return true;
  }
  if (std::holds_alternative<FinishTransactionMessage>(message)) {
    const FinishTransactionMessage &msg =
        std::get<FinishTransactionMessage>(message);
    this->handleFinishMessage(msg);
    return true;
  }
  warn_log("メッセージが適切に処理されなかった");
  return false; // ここには来ないはずだが、一応falseを返しておく
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

PlannerManager::PlannerManager(std::vector<Rank> cluster_ranks)
    : cluster_ranks(cluster_ranks), transaction_states(),
      current_planner(nullptr) {}

void PlannerManager::start_planning() {
  if (current_planner != nullptr) {
    current_planner->stop_planning();
  }
  current_planner = new SimplePlanner(
      this->cluster_ranks, this->transaction_states, Executor::messages);

  current_planner->start_planning();
}

void PlannerManager::start_loop() {
  while (true) {
    std::optional<PlannerMessage> omsg = PlannerManager::messages.pop();
    if (not omsg) {
      break;
    }
    bool need_refresh = this->handleMessage(*omsg);
    if (need_refresh) {
      this->start_planning();
    }
  }
  if (this->current_planner != nullptr) {
    this->current_planner->stop_planning();
  }
  info_log("PlannerManagerの実行を停止します");
}

void PlannerManager::initialize(std::vector<Rank> ranks) {
  globalPlannerManager = new PlannerManager(ranks);
  PlannerManager *ptr = globalPlannerManager;
  std::thread t([ptr]() -> void { ptr->start_loop(); });
  t.detach();
}

Planner::Planner(std::vector<Rank> cluster_ranks,
                 std::deque<TransactionState> transaction_states,
                 ConcurrentQueue<ExecutorMessage> &executor_message_queue)
    : cluster_ranks(cluster_ranks), transaction_states(transaction_states),
      executor_message_queue(executor_message_queue), stop(false),
      references(2) {}

void Planner::stop_planning() {
  this->stop = true;
  this->current_thread.detach();
  // もうスレッドの実行が終了していたなら消す
  if (this->references.fetch_sub(1) == 1) {
    delete this;
  }
}

void Planner::start_planning() {
  this->current_thread = std::thread([this]() {
    this->planning();

    finalize_planning();
  });
}

void Planner::finalize_planning() {
  // PlannerManagerから終了扱いされていたら消す
  if (this->references.fetch_sub(1) == 1) {
    delete this;
  }
}

void Planner::planning() {}

SimplePlanner::SimplePlanner(
    std::vector<Rank> cluster_ranks,
    std::deque<TransactionState> transaction_states,
    ConcurrentQueue<ExecutorMessage> &executor_message_queue)
    : Planner(cluster_ranks, transaction_states, executor_message_queue) {}

void SimplePlanner::planning() {
  // シンプルな実行計画
  // 一番新しいトランザクションにクラスタを割り当てて終了
  if (this->transaction_states.empty()) {
    return;
  }
  TransactionState &state = transaction_states.front();
  // 初期化されていないなら割り当てをしてはいけない
  if (not state.initialized) {
    return;
  }
  if (state.now.empty()) {
    // 何も実行していないなら新しく割り当てる
    // 一番ランクの値が少ないクラスタを割り当てる
    u64 target_rank = state.target_ranks.begin()->first;

    for (auto cluster_id : state.future) {
      if (this->cluster_ranks[cluster_id] == target_rank) {
        StartUpdateClusterMessage msg;
        msg.transaction_id = state.transaction_id;
        msg.cluster_id = cluster_id;
        this->executor_message_queue.push(msg);
        return;
      }
    }
  } else {
    // 何も実行していない かつ
    // 何も実行予定でない場合は、そのトランザクションを終了する
    if (state.future.empty()) {
      FinalizeTransactionMessage msg;
      msg.transaction_id = state.transaction_id;
      this->executor_message_queue.push(msg);
    }
  }
}

ConcurrentQueue<PlannerMessage> PlannerManager::messages;
PlannerManager *PlannerManager::globalPlannerManager = nullptr;

} // namespace prf
