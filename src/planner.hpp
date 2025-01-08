#pragma once
#include "concurrent_queue.hpp"
#include "executor.hpp"
#include "rank.hpp"
#include "types.hpp"
#include <atomic>
#include <deque>
#include <map>
#include <set>
#include <thread>
#include <variant>
#include <vector>

namespace prf {

/**
 * トランザクションの状態を保持するクラス
 */
class TransactionState {
public:
  /**
   * トランザクションのID
   */
  ID transaction_id;

  /**
   * 状態が確定しているか否か
   * これがfalseである場合にそれより後に生まれたトランザクションへクラスタを割り当ててはいけない
   */
  bool initialized;
  /**
   * 将来更新する予定のクラスタ
   */
  std::set<ID> future;
  /**
   * 更新中のクラスタ
   */
  std::set<ID> now;

  /**
   * クラスターのランクからfutureとnowに含まれている個数を引ける辞書
   */
  std::map<u64, u64> target_ranks;

  TransactionState(ID transaction_id);
};

/**
 * トランザクションの状態が変わったことを報せる
 */
class UpdateTransactionMessage {
public:
  ID transaction_id;
  /**
   * 更新を開始した
   */
  std::vector<ID> now;
  /**
   * 将来的に更新するものの追加
   */
  std::vector<ID> future;
  /**
   * 更新が終了した
   */
  std::vector<ID> finish;
};

/**
 * トランザクションの更新が終了した
 */
class FinishTransactionMessage {
public:
  /**
   * 更新が終了したトランザクションのID
   */
  ID transaction_id;
};

/**
 * 新しくトランザクションが発生した
 */
class StartTransactionMessage {
public:
  /**
   * 新しいトランザクションのID
   */
  ID transaction_id;
};

using PlannerMessage =
    std::variant<StartTransactionMessage, UpdateTransactionMessage,
                 FinishTransactionMessage>;

/**
 * メッセージがPlannerの再実行を必要とするものかを判定する
 */
bool need_refresh_message(const PlannerMessage &message);

/**
 * 実行計画を建てる関数
 */
using Planner =
    std::function<void(std::vector<Rank> &, std::deque<TransactionState> &,
                       ConcurrentQueue<ExecutorMessage> &, std::atomic_bool &)>;

/**
 * 実行計画を建てるPlannerを管理するクラス
 * このクラスとExecutorクラスは相互に通信することで動作する。
 * 大事な点として、このクラスは直接トランザクションを触らず、
 * 外界から来た現在のトランザクションの状態を元に提案という形でExecutorに投げる。
 * そのため、Executorから状態の変化がメッセージとして来るまでは、こちらが保持している状態を更新することは無い。
 * こういう理由があるため、Executorには複数回内容が同じであるメッセージが来るかもしれない。
 */
class PlannerManager {
private:
  /**
   * クラスターそれぞれのランク
   */
  std::vector<Rank> cluster_ranks;

  /**
   * 更新中のトランザクションの状態を保持する
   * dequeのfrontから順に古いトランザクションの状態が格納されている。
   */
  std::deque<TransactionState> transaction_states;

  /**
   * 現在バックグラウンドで稼動しているPlanner
   */
  std::vector<std::thread> running_planners;

  std::atomic_bool stop_planning_needed;

  /**
   * 実行計画を建てる関数の列
   * 複数の視点から実行計画を建てられるように列で受けとるようにしておく
   */
  std::vector<Planner> planners;

  /**
   * メッセージそれぞれをハンドリングするメソッド
   */
  void handleStartMessage(const StartTransactionMessage &);
  void handleUpdateMessage(const UpdateTransactionMessage &);
  void handleFinishMessage(const FinishTransactionMessage &);

  /**
   * Plannerの実行を止める
   */
  void stop_planning();

public:
  PlannerManager(std::vector<Rank> cluster_ranks,
                 std::vector<Planner> planners);

  /**
   * メッセージキューへ来たメッセージを処理する
   */
  void handleMessage(PlannerMessage);

  /**
   * 現在の状態から実行計画を建て初める
   */
  void start_planning();

  /**
   * PlannerManagerの仕事を開始する
   */
  void start_loop();

  /**
   * globalPlannerManagerを初期化する
   */
  static void initialize(std::vector<Rank> ranks);

  static ConcurrentQueue<PlannerMessage> messages;

  static PlannerManager *globalPlannerManager;
};

/**
 * 逐次実行だけできるPlanner
 */
void simple_planner(std::vector<Rank> &cluster_ranks,
                    std::deque<TransactionState> &transaction_states,
                    ConcurrentQueue<ExecutorMessage> &executor_message_queue,
                    std::atomic_bool &stop);

/**
 * ランクの情報から並列に動作するよう更新依頼を作るPlanner
 */
void rank_based_planner(
    std::vector<Rank> &cluster_ranks,
    std::deque<TransactionState> &transaction_states,
    ConcurrentQueue<ExecutorMessage> &executor_message_queue,
    std::atomic_bool &stop);
} // namespace prf
