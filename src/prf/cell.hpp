#pragma once

#include "prf/cluster.hpp"
#include "prf/executor.hpp"
#include "prf/logger.hpp"
#include "prf/time_invariant_values.hpp"
#include "prf/transaction.hpp"
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
   * 無い時はエラーで終了する
   */
  std::shared_ptr<T> unsafeSample(ID transaction_id);

  /**
   * FRPの外からlistenする
   */
  void listenFromOuter(std::function<void(std::shared_ptr<T>)>);

  // transactionに対応する時刻にvalueを登録する
  void send(T value, InnerTransaction *transaction);

  void send(T value);

  void update(InnerTransaction *transaction) override;

  void refresh(ID transaction_id) override;

  void finalize(InnerTransaction *transaction) override;

  template <class U> friend class CellLoop;
  template <class U> friend class GlobalCellLoop;
};

template <class T> class CellLoop;

template <class T> class Cell {
protected:
  CellInternal<T> *internal;

  /**
   * CellLoopで利用される初期値を与えずにインスタンスを生成するもの
   * 第二引数はダミー値です
   */
  Cell(ID, bool);

public:
  Cell(CellInternal<T> *internal);

  /**
   * 初期値を指定してCellを生成する
   */
  Cell(T);

  template <class F>
  Cell<typename std::invoke_result<F, T &>::type> map(F f) const {
    using U = typename std::invoke_result<F, T &>::type;
    ID cluster_id = clusterManager.current_id();
    std::function<U(ID)> updater = [internal = this->internal,
                                    f](ID transaction_id) -> U {
      std::shared_ptr<T> value = internal->unsafeSample(transaction_id);
      return f(*value);
    };
    CellInternal<U> *inter = new CellInternal<U>(cluster_id, updater);
    inter->listen(this->internal);
    return Cell<U>(inter);
  }

  template <class F> void listen(F f) const;

  template <class U1, class F>
  Cell<typename std::invoke_result<F, T &, U1 &>::type> lift(Cell<U1> c1,
                                                             F f) const;

  template <class U1, class U2, class F>
  Cell<typename std::invoke_result<F, T &, U1 &, U2 &>::type>
  lift(Cell<U1> c1, Cell<U2> c2, F f) const;

  template <class U1, class U2, class U3, class F>
  Cell<typename std::invoke_result<F, T &, U1 &, U2 &, U3 &>::type>
  lift(Cell<U1> c1, Cell<U2> c2, Cell<U3> c3, F f) const;

  template <class U1, class U2, class U3, class U4, class F>
  Cell<typename std::invoke_result<F, T &, U1 &, U2 &, U3 &, U4 &>::type>
  lift(Cell<U1> c1, Cell<U2> c2, Cell<U3> c3, Cell<U4> c4, F f) const;

  template <class U1, class U2, class U3, class U4, class U5, class F>
  Cell<typename std::invoke_result<F, T &, U1 &, U2 &, U3 &, U4 &, U5 &>::type>
  lift(Cell<U1> c1, Cell<U2> c2, Cell<U3> c3, Cell<U4> c4, Cell<U5> c5,
       F f) const;

  template <class U1, class U2, class U3, class U4, class U5, class U6, class F>
  Cell<typename std::invoke_result<F, T &, U1 &, U2 &, U3 &, U4 &, U5 &,
                                   U6 &>::type>
  lift(Cell<U1> c1, Cell<U2> c2, Cell<U3> c3, Cell<U4> c4, Cell<U5> c5,
       Cell<U6> c6, F f) const;

  template <class U1, class U2, class U3, class U4, class U5, class U6,
            class U7, class F>
  Cell<typename std::invoke_result<F, T &, U1 &, U2 &, U3 &, U4 &, U5 &, U6 &,
                                   U7 &>::type>
  lift(Cell<U1> c1, Cell<U2> c2, Cell<U3> c3, Cell<U4> c4, Cell<U5> c5,
       Cell<U6> c6, Cell<U7> c7, F f) const;

  template <class U1, class U2, class U3, class U4, class U5, class U6,
            class U7, class U8, class F>
  Cell<typename std::invoke_result<F, T &, U1 &, U2 &, U3 &, U4 &, U5 &, U6 &,
                                   U7 &, U8 &>::type>
  lift(Cell<U1> c1, Cell<U2> c2, Cell<U3> c3, Cell<U4> c4, Cell<U5> c5,
       Cell<U6> c6, Cell<U7> c7, Cell<U8> c8, F f) const;

  template <class U1, class U2, class U3, class U4, class U5, class U6,
            class U7, class U8, class U9, class F>
  Cell<typename std::invoke_result<F, T &, U1 &, U2 &, U3 &, U4 &, U5 &, U6 &,
                                   U7 &, U8 &, U9 &>::type>
  lift(Cell<U1> c1, Cell<U2> c2, Cell<U3> c3, Cell<U4> c4, Cell<U5> c5,
       Cell<U6> c6, Cell<U7> c7, Cell<U8> c8, Cell<U9> c9, F f) const;

  template <class U1, class U2, class U3, class U4, class U5, class U6,
            class U7, class U8, class U9, class U10, class F>
  Cell<typename std::invoke_result<F, T &, U1 &, U2 &, U3 &, U4 &, U5 &, U6 &,
                                   U7 &, U8 &, U9 &, U10 &>::type>
  lift(Cell<U1> c1, Cell<U2> c2, Cell<U3> c3, Cell<U4> c4, Cell<U5> c5,
       Cell<U6> c6, Cell<U7> c7, Cell<U8> c8, Cell<U9> c9, Cell<U10> c10,
       F f) const;

  friend CellLoop<T>;
  template <class U> friend class Stream;
  template <class U> friend class Cell;
  template <class U> friend class GlobalCellLoop;
};

template <class T> class CellSink : public Cell<T> {
private:
public:
  // 初期値有でCellSinkを初期化する
  CellSink(T);

  void send(T value) const;
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

template <class T> class GlobalCellLoop : public Cell<T> {
  /**
   * 既にLoopしているか否か
   */
  bool looped;

public:
  GlobalCellLoop();

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
      [this, initial_value](InnerTransaction *transaction) -> void {
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
      [this, initial_value](InnerTransaction *transaction) -> void {
        this->send(initial_value);
      });
}

template <class T> void CellInternal<T>::send(T value) {
  if (current_transaction == nullptr) {
    InnerTransaction trans;
    send(value, current_transaction);
  } else {
    send(value, current_transaction);
  }
}

template <class T>
void CellInternal<T>::send(T value, InnerTransaction *transaction) {
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
  if (not res.has_value()) {
    failure_log("論理時刻に対応する値がStreamに存在しませんでした");
  }
  return *res;
}

template <class T>
void CellInternal<T>::listenFromOuter(
    std::function<void(std::shared_ptr<T>)> f) {
  listeners.push_back(f);
}

template <class T> void CellInternal<T>::update(InnerTransaction *transaction) {
  ID transaction_id = transaction->get_id();
  this->updater;
  std::optional<T> res = this->updater(transaction_id);
  if (res) {
    this->send(*res, transaction);
  }
}

template <class T> void CellInternal<T>::refresh(ID transaction_id) {
  std::lock_guard<std::mutex> lock(mtx);
  if (values.find(transaction_id) == values.end()) {
    failure_log("このトランザクションで新しく値が設定されていません");
  }
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

template <class T>
void CellInternal<T>::finalize(InnerTransaction *transaction) {
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

template <class T> template <class F> void Cell<T>::listen(F f) const {
  this->internal->listenFromOuter([f](std::shared_ptr<T> v) -> void { f(*v); });
}

template <class T>
CellSink<T>::CellSink(T initial_value)
    : prf::Cell<T>(new CellInternal<T>(ClusterManager::UNMANAGED_CLUSTER_ID,
                                       initial_value)) {}

template <class T> void CellSink<T>::send(T value) const {
  this->internal->send(value);
}

template <class T>
Cell<T>::Cell(ID cluster_id, bool unuse)
    : internal(new CellInternal<T>(cluster_id,
                                   std::function<std::optional<T>(ID)>())) {}

template <class T>
CellLoop<T>::CellLoop()
    : Cell<T>(clusterManager.current_id(), false), looped(false) {}

template <class T> void CellLoop<T>::loop(Cell<T> c) {
  if (this->looped) {
    failure_log("CellLoopに対して複数回loopメソッドを呼び出しています");
  }

  this->looped = true;
  ID cluster_id = clusterManager.current_id();
  std::function<std::optional<T>(ID)> updater =
      [c](ID transaction_id) -> std::optional<T> {
    return *c.internal->unsafeSample(transaction_id);
  };
  // 強引にupdaterを置き変えているがC++で綺麗なコードを書くことは諦める
  this->internal->updater = updater;
  this->internal->listen_over_loop(c.internal);
}

template <class T>
GlobalCellLoop<T>::GlobalCellLoop()
    : Cell<T>(clusterManager.current_id(), false), looped(false) {}

template <class T> void GlobalCellLoop<T>::loop(Cell<T> c) {
  if (this->looped) {
    failure_log("GlobalCellLoopに対して複数回loopメソッドを呼び出しています");
  }

  this->looped = true;
  // 本来の用途を逸脱するが、設計の妥協としておく
  std::function<std::optional<T>(ID)> updater = [internal = this->internal,
                                                 c](ID id) -> std::optional<T> {
    if (current_transaction == nullptr) {
      failure_log("トランザクションの外でlistenしています");
    }
    T res = *c.internal->unsafeSample(id);
    current_transaction->register_before_update_hook(
        [internal, res](ID id) -> void {
          std::lock_guard<std::mutex> lock(internal->mtx);
          internal->values[id] = std::make_shared<T>(res);
        });
    return std::nullopt;
  };
  this->internal->global_listen(c.internal);
  this->internal->updater = updater;
}

template <class T>
template <class U1, class F>
Cell<typename std::invoke_result<F, T &, U1 &>::type> Cell<T>::lift(Cell<U1> c1,
                                                                    F f) const {
  using V = typename std::invoke_result<F, T &, U1 &>::type;
  ID cluster_id = clusterManager.current_id();
  std::function<std::optional<V>(ID)> updater =
      [internal = this->internal, c1,
       f](ID transaction_id) -> std::optional<V> {
    // Loopを利用していると、片方のCellを初期化する前に呼び出される可能性があるので、nullのときは同じくnullを返す
    std::optional<std::shared_ptr<T>> v = internal->sample(transaction_id);
    if (not v) {
      return std::nullopt;
    }
    std::optional<std::shared_ptr<U1>> v1 = c1.internal->sample(transaction_id);
    if (not v1) {
      return std::nullopt;
    }
    return f(**v, **v1);
  };
  CellInternal<V> *inter = new CellInternal<V>(cluster_id, updater);
  inter->listen(this->internal);
  inter->listen(c1.internal);
  return Cell<V>(inter);
}

template <class T>
template <class U1, class U2, class F>
Cell<typename std::invoke_result<F, T &, U1 &, U2 &>::type>
Cell<T>::lift(Cell<U1> c1, Cell<U2> c2, F f) const {
  using V = typename std::invoke_result<F, T &, U1 &, U2 &>::type;
  ID cluster_id = clusterManager.current_id();
  std::function<std::optional<V>(ID)> updater =
      [internal = this->internal, c1, c2,
       f](ID transaction_id) -> std::optional<V> {
    // Loopを利用していると、片方のCellを初期化する前に呼び出される可能性があるので、nullのときは同じくnullを返す
    std::optional<std::shared_ptr<T>> v = internal->sample(transaction_id);
    if (not v) {
      return std::nullopt;
    }
    std::optional<std::shared_ptr<U1>> v1 = c1.internal->sample(transaction_id);
    if (not v1) {
      return std::nullopt;
    }
    std::optional<std::shared_ptr<U2>> v2 = c2.internal->sample(transaction_id);
    if (not v2) {
      return std::nullopt;
    }
    return f(**v, **v1, **v2);
  };
  CellInternal<V> *inter = new CellInternal<V>(cluster_id, updater);
  inter->listen(this->internal);
  inter->listen(c1.internal);
  inter->listen(c2.internal);
  return Cell<V>(inter);
}

template <class T>
template <class U1, class U2, class U3, class F>
Cell<typename std::invoke_result<F, T &, U1 &, U2 &, U3 &>::type>
Cell<T>::lift(Cell<U1> c1, Cell<U2> c2, Cell<U3> c3, F f) const {
  using V = typename std::invoke_result<F, T &, U1 &, U2 &, U3 &>::type;
  ID cluster_id = clusterManager.current_id();
  std::function<std::optional<V>(ID)> updater =
      [internal = this->internal, c1, c2, c3,
       f](ID transaction_id) -> std::optional<V> {
    // Loopを利用していると、片方のCellを初期化する前に呼び出される可能性があるので、nullのときは同じくnullを返す
    std::optional<std::shared_ptr<T>> v = internal->sample(transaction_id);
    if (not v) {
      return std::nullopt;
    }
    std::optional<std::shared_ptr<U1>> v1 = c1.internal->sample(transaction_id);
    if (not v1) {
      return std::nullopt;
    }
    std::optional<std::shared_ptr<U2>> v2 = c2.internal->sample(transaction_id);
    if (not v2) {
      return std::nullopt;
    }
    std::optional<std::shared_ptr<U3>> v3 = c3.internal->sample(transaction_id);
    if (not v3) {
      return std::nullopt;
    }
    return f(**v, **v1, **v2, **v3);
  };
  CellInternal<V> *inter = new CellInternal<V>(cluster_id, updater);
  inter->listen(this->internal);
  inter->listen(c1.internal);
  inter->listen(c2.internal);
  inter->listen(c3.internal);
  return Cell<V>(inter);
}

template <class T>
template <class U1, class U2, class U3, class U4, class F>
Cell<typename std::invoke_result<F, T &, U1 &, U2 &, U3 &, U4 &>::type>
Cell<T>::lift(Cell<U1> c1, Cell<U2> c2, Cell<U3> c3, Cell<U4> c4, F f) const {
  using V = typename std::invoke_result<F, T &, U1 &, U2 &, U3 &, U4 &>::type;
  ID cluster_id = clusterManager.current_id();
  std::function<std::optional<V>(ID)> updater =
      [internal = this->internal, c1, c2, c3, c4,
       f](ID transaction_id) -> std::optional<V> {
    // Loopを利用していると、片方のCellを初期化する前に呼び出される可能性があるので、nullのときは同じくnullを返す
    std::optional<std::shared_ptr<T>> v = internal->sample(transaction_id);
    if (not v) {
      return std::nullopt;
    }
    std::optional<std::shared_ptr<U1>> v1 = c1.internal->sample(transaction_id);
    if (not v1) {
      return std::nullopt;
    }
    std::optional<std::shared_ptr<U2>> v2 = c2.internal->sample(transaction_id);
    if (not v2) {
      return std::nullopt;
    }
    std::optional<std::shared_ptr<U3>> v3 = c3.internal->sample(transaction_id);
    if (not v3) {
      return std::nullopt;
    }
    std::optional<std::shared_ptr<U4>> v4 = c4.internal->sample(transaction_id);
    if (not v4) {
      return std::nullopt;
    }
    return f(**v, **v1, **v2, **v3, **v4);
  };
  CellInternal<V> *inter = new CellInternal<V>(cluster_id, updater);
  inter->listen(this->internal);
  inter->listen(c1.internal);
  inter->listen(c2.internal);
  inter->listen(c3.internal);
  inter->listen(c4.internal);
  return Cell<V>(inter);
}

template <class T>
template <class U1, class U2, class U3, class U4, class U5, class F>
Cell<typename std::invoke_result<F, T &, U1 &, U2 &, U3 &, U4 &, U5 &>::type>
Cell<T>::lift(Cell<U1> c1, Cell<U2> c2, Cell<U3> c3, Cell<U4> c4, Cell<U5> c5,
              F f) const {
  using V =
      typename std::invoke_result<F, T &, U1 &, U2 &, U3 &, U4 &, U5 &>::type;
  ID cluster_id = clusterManager.current_id();
  std::function<std::optional<V>(ID)> updater =
      [internal = this->internal, c1, c2, c3, c4, c5,
       f](ID transaction_id) -> std::optional<V> {
    // Loopを利用していると、片方のCellを初期化する前に呼び出される可能性があるので、nullのときは同じくnullを返す
    std::optional<std::shared_ptr<T>> v = internal->sample(transaction_id);
    if (not v) {
      return std::nullopt;
    }
    std::optional<std::shared_ptr<U1>> v1 = c1.internal->sample(transaction_id);
    if (not v1) {
      return std::nullopt;
    }
    std::optional<std::shared_ptr<U2>> v2 = c2.internal->sample(transaction_id);
    if (not v2) {
      return std::nullopt;
    }
    std::optional<std::shared_ptr<U3>> v3 = c3.internal->sample(transaction_id);
    if (not v3) {
      return std::nullopt;
    }
    std::optional<std::shared_ptr<U4>> v4 = c4.internal->sample(transaction_id);
    if (not v4) {
      return std::nullopt;
    }
    std::optional<std::shared_ptr<U5>> v5 = c5.internal->sample(transaction_id);
    if (not v5) {
      return std::nullopt;
    }
    return f(**v, **v1, **v2, **v3, **v4, **v5);
  };
  CellInternal<V> *inter = new CellInternal<V>(cluster_id, updater);
  inter->listen(this->internal);
  inter->listen(c1.internal);
  inter->listen(c2.internal);
  inter->listen(c3.internal);
  inter->listen(c4.internal);
  inter->listen(c5.internal);
  return Cell<V>(inter);
}

template <class T>
template <class U1, class U2, class U3, class U4, class U5, class U6, class F>
Cell<typename std::invoke_result<F, T &, U1 &, U2 &, U3 &, U4 &, U5 &,
                                 U6 &>::type>
Cell<T>::lift(Cell<U1> c1, Cell<U2> c2, Cell<U3> c3, Cell<U4> c4, Cell<U5> c5,
              Cell<U6> c6, F f) const {
  using V = typename std::invoke_result<F, T &, U1 &, U2 &, U3 &, U4 &, U5 &,
                                        U6 &>::type;
  ID cluster_id = clusterManager.current_id();
  std::function<std::optional<V>(ID)> updater =
      [internal = this->internal, c1, c2, c3, c4, c5, c6,
       f](ID transaction_id) -> std::optional<V> {
    // Loopを利用していると、片方のCellを初期化する前に呼び出される可能性があるので、nullのときは同じくnullを返す
    std::optional<std::shared_ptr<T>> v = internal->sample(transaction_id);
    if (not v) {
      return std::nullopt;
    }
    std::optional<std::shared_ptr<U1>> v1 = c1.internal->sample(transaction_id);
    if (not v1) {
      return std::nullopt;
    }
    std::optional<std::shared_ptr<U2>> v2 = c2.internal->sample(transaction_id);
    if (not v2) {
      return std::nullopt;
    }
    std::optional<std::shared_ptr<U3>> v3 = c3.internal->sample(transaction_id);
    if (not v3) {
      return std::nullopt;
    }
    std::optional<std::shared_ptr<U4>> v4 = c4.internal->sample(transaction_id);
    if (not v4) {
      return std::nullopt;
    }
    std::optional<std::shared_ptr<U5>> v5 = c5.internal->sample(transaction_id);
    if (not v5) {
      return std::nullopt;
    }
    std::optional<std::shared_ptr<U6>> v6 = c6.internal->sample(transaction_id);
    if (not v6) {
      return std::nullopt;
    }
    return f(**v, **v1, **v2, **v3, **v4, **v5, **v6);
  };
  CellInternal<V> *inter = new CellInternal<V>(cluster_id, updater);
  inter->listen(this->internal);
  inter->listen(c1.internal);
  inter->listen(c2.internal);
  inter->listen(c3.internal);
  inter->listen(c4.internal);
  inter->listen(c5.internal);
  inter->listen(c6.internal);
  return Cell<V>(inter);
}

template <class T>
template <class U1, class U2, class U3, class U4, class U5, class U6, class U7,
          class F>
Cell<typename std::invoke_result<F, T &, U1 &, U2 &, U3 &, U4 &, U5 &, U6 &,
                                 U7 &>::type>
Cell<T>::lift(Cell<U1> c1, Cell<U2> c2, Cell<U3> c3, Cell<U4> c4, Cell<U5> c5,
              Cell<U6> c6, Cell<U7> c7, F f) const {
  using V = typename std::invoke_result<F, T &, U1 &, U2 &, U3 &, U4 &, U5 &,
                                        U6 &, U7 &>::type;
  ID cluster_id = clusterManager.current_id();
  std::function<std::optional<V>(ID)> updater =
      [internal = this->internal, c1, c2, c3, c4, c5, c6, c7,
       f](ID transaction_id) -> std::optional<V> {
    // Loopを利用していると、片方のCellを初期化する前に呼び出される可能性があるので、nullのときは同じくnullを返す
    std::optional<std::shared_ptr<T>> v = internal->sample(transaction_id);
    if (not v) {
      return std::nullopt;
    }
    std::optional<std::shared_ptr<U1>> v1 = c1.internal->sample(transaction_id);
    if (not v1) {
      return std::nullopt;
    }
    std::optional<std::shared_ptr<U2>> v2 = c2.internal->sample(transaction_id);
    if (not v2) {
      return std::nullopt;
    }
    std::optional<std::shared_ptr<U3>> v3 = c3.internal->sample(transaction_id);
    if (not v3) {
      return std::nullopt;
    }
    std::optional<std::shared_ptr<U4>> v4 = c4.internal->sample(transaction_id);
    if (not v4) {
      return std::nullopt;
    }
    std::optional<std::shared_ptr<U5>> v5 = c5.internal->sample(transaction_id);
    if (not v5) {
      return std::nullopt;
    }
    std::optional<std::shared_ptr<U6>> v6 = c6.internal->sample(transaction_id);
    if (not v6) {
      return std::nullopt;
    }
    std::optional<std::shared_ptr<U7>> v7 = c7.internal->sample(transaction_id);
    if (not v7) {
      return std::nullopt;
    }
    return f(**v, **v1, **v2, **v3, **v4, **v5, **v6, **v7);
  };
  CellInternal<V> *inter = new CellInternal<V>(cluster_id, updater);
  inter->listen(this->internal);
  inter->listen(c1.internal);
  inter->listen(c2.internal);
  inter->listen(c3.internal);
  inter->listen(c4.internal);
  inter->listen(c5.internal);
  inter->listen(c6.internal);
  inter->listen(c7.internal);
  return Cell<V>(inter);
}

template <class T>
template <class U1, class U2, class U3, class U4, class U5, class U6, class U7,
          class U8, class F>
Cell<typename std::invoke_result<F, T &, U1 &, U2 &, U3 &, U4 &, U5 &, U6 &,
                                 U7 &, U8 &>::type>
Cell<T>::lift(Cell<U1> c1, Cell<U2> c2, Cell<U3> c3, Cell<U4> c4, Cell<U5> c5,
              Cell<U6> c6, Cell<U7> c7, Cell<U8> c8, F f) const {
  using V = typename std::invoke_result<F, T &, U1 &, U2 &, U3 &, U4 &, U5 &,
                                        U6 &, U7 &, U8 &>::type;
  ID cluster_id = clusterManager.current_id();
  std::function<std::optional<V>(ID)> updater =
      [internal = this->internal, c1, c2, c3, c4, c5, c6, c7, c8,
       f](ID transaction_id) -> std::optional<V> {
    // Loopを利用していると、片方のCellを初期化する前に呼び出される可能性があるので、nullのときは同じくnullを返す
    std::optional<std::shared_ptr<T>> v = internal->sample(transaction_id);
    if (not v) {
      return std::nullopt;
    }
    std::optional<std::shared_ptr<U1>> v1 = c1.internal->sample(transaction_id);
    if (not v1) {
      return std::nullopt;
    }
    std::optional<std::shared_ptr<U2>> v2 = c2.internal->sample(transaction_id);
    if (not v2) {
      return std::nullopt;
    }
    std::optional<std::shared_ptr<U3>> v3 = c3.internal->sample(transaction_id);
    if (not v3) {
      return std::nullopt;
    }
    std::optional<std::shared_ptr<U4>> v4 = c4.internal->sample(transaction_id);
    if (not v4) {
      return std::nullopt;
    }
    std::optional<std::shared_ptr<U5>> v5 = c5.internal->sample(transaction_id);
    if (not v5) {
      return std::nullopt;
    }
    std::optional<std::shared_ptr<U6>> v6 = c6.internal->sample(transaction_id);
    if (not v6) {
      return std::nullopt;
    }
    std::optional<std::shared_ptr<U7>> v7 = c7.internal->sample(transaction_id);
    if (not v7) {
      return std::nullopt;
    }
    std::optional<std::shared_ptr<U8>> v8 = c8.internal->sample(transaction_id);
    if (not v8) {
      return std::nullopt;
    }
    return f(**v, **v1, **v2, **v3, **v4, **v5, **v6, **v7, **v8);
  };
  CellInternal<V> *inter = new CellInternal<V>(cluster_id, updater);
  inter->listen(this->internal);
  inter->listen(c1.internal);
  inter->listen(c2.internal);
  inter->listen(c3.internal);
  inter->listen(c4.internal);
  inter->listen(c5.internal);
  inter->listen(c6.internal);
  inter->listen(c7.internal);
  inter->listen(c8.internal);
  return Cell<V>(inter);
}

template <class T>
template <class U1, class U2, class U3, class U4, class U5, class U6, class U7,
          class U8, class U9, class F>
Cell<typename std::invoke_result<F, T &, U1 &, U2 &, U3 &, U4 &, U5 &, U6 &,
                                 U7 &, U8 &, U9 &>::type>
Cell<T>::lift(Cell<U1> c1, Cell<U2> c2, Cell<U3> c3, Cell<U4> c4, Cell<U5> c5,
              Cell<U6> c6, Cell<U7> c7, Cell<U8> c8, Cell<U9> c9, F f) const {
  using V = typename std::invoke_result<F, T &, U1 &, U2 &, U3 &, U4 &, U5 &,
                                        U6 &, U7 &, U8 &, U9 &>::type;
  ID cluster_id = clusterManager.current_id();
  std::function<std::optional<V>(ID)> updater =
      [internal = this->internal, c1, c2, c3, c4, c5, c6, c7, c8, c9,
       f](ID transaction_id) -> std::optional<V> {
    // Loopを利用していると、片方のCellを初期化する前に呼び出される可能性があるので、nullのときは同じくnullを返す
    std::optional<std::shared_ptr<T>> v = internal->sample(transaction_id);
    if (not v) {
      return std::nullopt;
    }
    std::optional<std::shared_ptr<U1>> v1 = c1.internal->sample(transaction_id);
    if (not v1) {
      return std::nullopt;
    }
    std::optional<std::shared_ptr<U2>> v2 = c2.internal->sample(transaction_id);
    if (not v2) {
      return std::nullopt;
    }
    std::optional<std::shared_ptr<U3>> v3 = c3.internal->sample(transaction_id);
    if (not v3) {
      return std::nullopt;
    }
    std::optional<std::shared_ptr<U4>> v4 = c4.internal->sample(transaction_id);
    if (not v4) {
      return std::nullopt;
    }
    std::optional<std::shared_ptr<U5>> v5 = c5.internal->sample(transaction_id);
    if (not v5) {
      return std::nullopt;
    }
    std::optional<std::shared_ptr<U6>> v6 = c6.internal->sample(transaction_id);
    if (not v6) {
      return std::nullopt;
    }
    std::optional<std::shared_ptr<U7>> v7 = c7.internal->sample(transaction_id);
    if (not v7) {
      return std::nullopt;
    }
    std::optional<std::shared_ptr<U8>> v8 = c8.internal->sample(transaction_id);
    if (not v8) {
      return std::nullopt;
    }
    std::optional<std::shared_ptr<U9>> v9 = c9.internal->sample(transaction_id);
    if (not v9) {
      return std::nullopt;
    }
    return f(**v, **v1, **v2, **v3, **v4, **v5, **v6, **v7, **v8, **v9);
  };
  CellInternal<V> *inter = new CellInternal<V>(cluster_id, updater);
  inter->listen(this->internal);
  inter->listen(c1.internal);
  inter->listen(c2.internal);
  inter->listen(c3.internal);
  inter->listen(c4.internal);
  inter->listen(c5.internal);
  inter->listen(c6.internal);
  inter->listen(c7.internal);
  inter->listen(c8.internal);
  inter->listen(c9.internal);
  return Cell<V>(inter);
}

template <class T>
template <class U1, class U2, class U3, class U4, class U5, class U6, class U7,
          class U8, class U9, class U10, class F>
Cell<typename std::invoke_result<F, T &, U1 &, U2 &, U3 &, U4 &, U5 &, U6 &,
                                 U7 &, U8 &, U9 &, U10 &>::type>
Cell<T>::lift(Cell<U1> c1, Cell<U2> c2, Cell<U3> c3, Cell<U4> c4, Cell<U5> c5,
              Cell<U6> c6, Cell<U7> c7, Cell<U8> c8, Cell<U9> c9, Cell<U10> c10,
              F f) const {
  using V = typename std::invoke_result<F, T &, U1 &, U2 &, U3 &, U4 &, U5 &,
                                        U6 &, U7 &, U8 &, U9 &, U10 &>::type;
  ID cluster_id = clusterManager.current_id();
  std::function<std::optional<V>(ID)> updater =
      [internal = this->internal, c1, c2, c3, c4, c5, c6, c7, c8, c9, c10,
       f](ID transaction_id) -> std::optional<V> {
    // Loopを利用していると、片方のCellを初期化する前に呼び出される可能性があるので、nullのときは同じくnullを返す
    std::optional<std::shared_ptr<T>> v = internal->sample(transaction_id);
    if (not v) {
      return std::nullopt;
    }
    std::optional<std::shared_ptr<U1>> v1 = c1.internal->sample(transaction_id);
    if (not v1) {
      return std::nullopt;
    }
    std::optional<std::shared_ptr<U2>> v2 = c2.internal->sample(transaction_id);
    if (not v2) {
      return std::nullopt;
    }
    std::optional<std::shared_ptr<U3>> v3 = c3.internal->sample(transaction_id);
    if (not v3) {
      return std::nullopt;
    }
    std::optional<std::shared_ptr<U4>> v4 = c4.internal->sample(transaction_id);
    if (not v4) {
      return std::nullopt;
    }
    std::optional<std::shared_ptr<U5>> v5 = c5.internal->sample(transaction_id);
    if (not v5) {
      return std::nullopt;
    }
    std::optional<std::shared_ptr<U6>> v6 = c6.internal->sample(transaction_id);
    if (not v6) {
      return std::nullopt;
    }
    std::optional<std::shared_ptr<U7>> v7 = c7.internal->sample(transaction_id);
    if (not v7) {
      return std::nullopt;
    }
    std::optional<std::shared_ptr<U8>> v8 = c8.internal->sample(transaction_id);
    if (not v8) {
      return std::nullopt;
    }
    std::optional<std::shared_ptr<U9>> v9 = c9.internal->sample(transaction_id);
    if (not v9) {
      return std::nullopt;
    }
    std::optional<std::shared_ptr<U10>> v10 =
        c10.internal->sample(transaction_id);
    if (not v10) {
      return std::nullopt;
    }
    return f(**v, **v1, **v2, **v3, **v4, **v5, **v6, **v7, **v8, **v9, **v10);
  };
  CellInternal<V> *inter = new CellInternal<V>(cluster_id, updater);
  inter->listen(this->internal);
  inter->listen(c1.internal);
  inter->listen(c2.internal);
  inter->listen(c3.internal);
  inter->listen(c4.internal);
  inter->listen(c5.internal);
  inter->listen(c6.internal);
  inter->listen(c7.internal);
  inter->listen(c8.internal);
  inter->listen(c9.internal);
  inter->listen(c10.internal);
  return Cell<V>(inter);
}

} // namespace prf
