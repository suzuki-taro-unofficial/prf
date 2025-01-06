#pragma once

#include "cluster.hpp"
#include "executor.hpp"
#include "logger.hpp"
#include "time_invariant_values.hpp"
#include "transaction.hpp"
#include <cassert>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <type_traits>

// 常に値が存在するような時変値であるCellの実装
namespace prf {

// Cell系列が内部で保持するクラス
template <class T> class CellInternal : public TimeInvariantValues {
private:
  // トランザクションIDに対応する値を保存する
  std::map<ID, std::shared_ptr<T>> values;

  // values自体の排他ロックのためにある
  // values[x]
  // が返す個々の参照自体には排他ロックが存在しないので、それを前提とした設計をする必要がある
  std::mutex mtx;

  std::function<std::optional<T>(ID transaction_id)> updater;

  /**
   * 時変値の変化をFRPの外でlistenしている関数のリスト
   */
  std::vector<std::function<void(std::shared_ptr<T>)>> listeners;

public:
  CellInternal<T>(ID cluster_id,
                  std::function<std::optional<T>(ID transaction_id)> updater);

  CellInternal<T>(ID cluster_id, T initial_value,
                  std::function<std::optional<T>(ID transaction_id)> updater);

  CellInternal<T>(ID cluster_id, T initial_value);

  /**
   * transaction以前(現在実行中のトランザクションを含めた)に生成された値を取得する
   * 存在しなかった場合は std::nullopt を返す
   */
  std::optional<std::shared_ptr<T>> sample(ID transaction_id);

  /**
   * sample() と違いその論理時刻に値が存在することが保証される場合に呼び出す
   * 無い時はassertのエラーで終了する
   */
  std::shared_ptr<T> unsafeSample(ID transaction_id);

  /**
   * FRPの外からlistenする
   */
  void listenFromOuter(std::function<void(std::shared_ptr<T>)>);

  // transactionに対応する時刻にvalueを登録する
  void send(T value, Transaction *transaction);

  void send(T value);

  void update(Transaction *transaction) override;

  void refresh(ID transaction_id) override;

  void finalize(Transaction *transaction) override;
};

template <class T> class CellLoop;

template <class T> class Cell {
protected:
  CellInternal<T> *internal;

  /**
   * CellLoopで利用される初期値を与えずにインスタンスを生成するもの
   */
  Cell();

public:
  Cell(CellInternal<T> *internal);

  /**
   * 初期値を指定してCellを生成する
   */
  Cell(T);

  template <class F> Cell<typename std::invoke_result<F, T &>::type> map(F f) {
    using U = typename std::invoke_result<F, T &>::type;
    ID cluster_id = clusterManager.current_id();
    std::function<U(ID)> updater = [this, f](ID transaction_id) -> U {
      std::shared_ptr<T> value = this->internal->unsafeSample(transaction_id);
      return f(*value);
    };
    CellInternal<U> *inter = new CellInternal<U>(cluster_id, updater);
    inter->listen(this->internal);
    return Cell<U>(inter);
  }

  template <class F> void listen(F f);

  template <class U, class F>
  Cell<typename std::invoke_result<F, T &, U &>::type> lift(Cell<U> c2, F f);

  friend CellLoop<T>;
  template <class U> friend class Stream;
  template <class U> friend class Cell;
};

template <class T> class CellSink : public Cell<T> {
private:
public:
  // 初期値有でCellSinkを初期化する
  CellSink(T);

  void send(T value);
};

template <class T> class CellLoop : public Cell<T> {
  /**
   * 既にLoopしているか否か
   */
  bool looped;

public:
  CellLoop();

  void loop(Cell<T> c);
};

template <class T>
CellInternal<T>::CellInternal(
    ID cluster_id, std::function<std::optional<T>(ID transaction_id)> updater)
    : TimeInvariantValues(cluster_id), updater(updater){};

template <class T>
CellInternal<T>::CellInternal(
    ID cluster_id, T initial_value,
    std::function<std::optional<T>(ID transaction_id)> updater)
    : TimeInvariantValues(cluster_id), updater(updater) {
  // 初期値はEecutorの初期化処理に含める
  Executor::after_build_hooks.push_back(
      [this, initial_value](Transaction *transaction) -> void {
        this->send(initial_value);
      });
}

template <class T>
CellInternal<T>::CellInternal(ID cluster_id, T initial_value)
    : CellInternal(
          cluster_id, (std::function<std::optional<T>(ID transaction_id)>)[](
                          ID transaction_id)
                          ->std::optional<T> {
                            warn_log("無効なupdaterが登録されています");
                            return std::nullopt;
                          }) {
  // 初期値はEecutorの初期化処理に含める
  Executor::after_build_hooks.push_back(
      [this, initial_value](Transaction *transaction) -> void {
        this->send(initial_value);
      });
}

template <class T> void CellInternal<T>::send(T value) {
  if (current_transaction == nullptr) {
    Transaction trans;
    send(value, current_transaction);
  } else {
    send(value, current_transaction);
  }
}

template <class T>
void CellInternal<T>::send(T value, Transaction *transaction) {
  {
    std::lock_guard<std::mutex> lock(mtx);
    values[transaction->get_id()] = std::make_shared<T>(value);
  }
  this->register_listeners_update(transaction);
  this->register_cleanup(transaction);
}

template <class T>
std::optional<std::shared_ptr<T>> CellInternal<T>::sample(ID transaction_id) {
  std::lock_guard<std::mutex> lock(mtx);
  // Cellは複数の論理時間に渡って値が存在するので、指定したトランザクション以前を探すことになる
  auto itr = values.upper_bound(transaction_id);
  if (itr == values.begin()) {
    return std::nullopt;
  }
  --itr;
  return itr->second;
}

template <class T>
std::shared_ptr<T> CellInternal<T>::unsafeSample(ID transaction_id) {
  std::optional<std::shared_ptr<T>> res = this->sample(transaction_id);
  assert(res && "論理時刻に対応する値がStreamに存在しませんでした");
  return *res;
}

template <class T>
void CellInternal<T>::listenFromOuter(
    std::function<void(std::shared_ptr<T>)> f) {
  listeners.push_back(f);
}

template <class T> void CellInternal<T>::update(Transaction *transaction) {
  ID transaction_id = transaction->get_id();
  this->updater;
  std::optional<T> res = this->updater(transaction_id);
  if (res) {
    this->send(*res, transaction);
  }
}

template <class T> void CellInternal<T>::refresh(ID transaction_id) {
  std::lock_guard<std::mutex> lock(mtx);
  assert(values.find(transaction_id) != values.end() &&
         "このトランザクションで新しく値が設定されていません");
  // 指定されたTransactionより以前にある値を消去する
  while (true) {
    auto itr = values.begin();
    if (itr->first < transaction_id) {
      values.erase(itr);
    } else {
      break;
    }
  }
}

template <class T> void CellInternal<T>::finalize(Transaction *transaction) {
  std::shared_ptr<T> value = this->unsafeSample(transaction->get_id());
  for (std::function<void(std::shared_ptr<T>)> &listener : listeners) {
    listener(value);
  }
}

template <class T>
Cell<T>::Cell(CellInternal<T> *internal) : internal(internal) {}

template <class T>
Cell<T>::Cell(T initial_value)
    : internal(
          new CellInternal<T>(clusterManager.current_id(), initial_value)) {}

template <class T> template <class F> void Cell<T>::listen(F f) {
  this->internal->listenFromOuter([f](std::shared_ptr<T> v) -> void { f(*v); });
}

template <class T>
CellSink<T>::CellSink(T initial_value)
    : prf::Cell<T>(new CellInternal<T>(ClusterManager::UNMANAGED_CLUSTER_ID,
                                       initial_value)) {}

template <class T> void CellSink<T>::send(T value) {
  this->internal->send(value);
}

template <class T> Cell<T>::Cell() : internal(nullptr) {}

template <class T> CellLoop<T>::CellLoop() : Cell<T>() {}

template <class T> void CellLoop<T>::loop(Cell<T> c) {
  assert(not this->looped &&
         "CellLoopに対して複数回loopメソッドを呼び出しています");

  this->looped = true;
  ID cluster_id = clusterManager.current_id();
  std::function<T(ID)> updater = [this, c](ID transaction_id) {
    return *c.internal->unsafeSample(transaction_id);
  };
  CellInternal<T> *inter = new CellInternal<T>(cluster_id, updater);
  inter->listen_over_loop(c.internal);
  this->internal = inter;
}

template <class T>
template <class U, class F>
Cell<typename std::invoke_result<F, T &, U &>::type> Cell<T>::lift(Cell<U> c2,
                                                                   F f) {
  using V = typename std::invoke_result<F, T &, U &>::type;
  ID cluster_id = clusterManager.current_id();
  std::function<std::optional<V>(ID)> updater =
      [this, c2, f](ID transaction_id) -> std::optional<V> {
    // Loopを利用していると、片方のCellを初期化する前に呼び出される可能性があるので、nullのときは同じくnullを返す
    std::optional<std::shared_ptr<T>> v1 =
        this->internal->sample(transaction_id);
    if (not v1) {
      return std::nullopt;
    }
    std::optional<std::shared_ptr<U>> v2 = c2.internal->sample(transaction_id);
    if (not v2) {
      return std::nullopt;
    }
    return f(**v1, **v2);
  };
  CellInternal<V> *inter = new CellInternal<V>(cluster_id, updater);
  inter->listen(this->internal);
  inter->listen(c2.internal);
  return Cell<V>(inter);
}

} // namespace prf
