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
 * 実際に実行計画を建てるクラス
 */
class Planner {
private:
  /**
   * このPlannerのインスタンスを参照している箇所の個数
   */
  std::atomic_int references;

  std::thread current_thread;

  /**
   * planning()が終了したときの後処理をする
   */
  void finalize_planning();

protected:
  /**
   * 現在の実行計画を止めるべきか
   * このフラグがtrueであることがplanning()の中で分かった場合、自分自身をdeleteして終了する必要がある
   */
  volatile bool stop;

  /**
   * それぞれクラスタのランクとトランザクションの状態
   * 実行計画を建てるのを開始した時点でスナップショットを受けとり、それ以降内容は変更されない
   */
  std::vector<Rank> cluster_ranks;
  std::deque<TransactionState> transaction_states;

  /**
   * 実行計画に関するメッセージを送る先
   * 単体テスト時はここを適当なキューに置き変えると良い
   */
  ConcurrentQueue<ExecutorMessage> &executor_message_queue;

  /**
   * 現在の情報から実行計画を建てる
   */
  virtual void planning();

public:
  Planner(std::vector<Rank> cluster_ranks,
          std::deque<TransactionState> transaction_states,
          ConcurrentQueue<ExecutorMessage> &executor_message_queue);
  /**
   * バックグラウンドで動作しているスレッドを停止する
   */
  void stop_planning();

  /**
   * 実行計画を建てるスレッドをバックグラウンドで開始する
   */
  void start_planning();
};

class SimplePlanner : public Planner {
  void planning() override;
};

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
  Planner *current_planner;

  /**
   * メッセージそれぞれをハンドリングするメソッド
   */
  void handleStartMessage(const StartTransactionMessage &);
  void handleUpdateMessage(const UpdateTransactionMessage &);
  void handleFinishMessage(const FinishTransactionMessage &);

public:
  PlannerManager(std::vector<Rank> cluster_ranks);

  /**
   * メッセージキューへ来たメッセージを処理する
   * 実行計画を再度建て直す必要があるときはtrueを返す
   */
  bool handleMessage(PlannerMessage);

  /**
   * 現在の状態から実行計画を建て初める
   */
  void start_planning();

  /**
   * Plannerの仕事を開始する
   */
  void start_loop();

  static ConcurrentQueue<PlannerMessage> messages;
};
} // namespace prf
