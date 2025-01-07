#pragma once

#include "cell.hpp"
#include "cluster.hpp"
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

namespace prf {
// 一瞬だけ値が存在するような時変値を表すクラス
// Stream系列が内部で保持する
template <class T> class StreamInternal : public TimeInvariantValues {
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
  StreamInternal<T>(ID cluster_id,
                    std::function<std::optional<T>(ID transaction_id)> updater);

  StreamInternal<T>(ID cluster_id);
  /**
   * トランザクションに対応する値を取得する
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

  template <class U> friend class StreamLoop;
};

template <class T> class Cell;
template <class T> class StreamLoop;

template <class T> class Stream {
protected:
  StreamInternal<T> *internal;

public:
  Stream(StreamInternal<T> *internal);
  Stream();

  template <class F>
  Stream<typename std::invoke_result<F, T &>::type> map(F f) {
    using U = typename std::invoke_result<F, T &>::type;
    ID cluster_id = clusterManager.current_id();
    std::function<std::optional<U>(ID)> updater = [internal = this->internal,
                                                   f](ID transaction_id) {
      std::optional<std::shared_ptr<T>> value =
          internal->sample(transaction_id);
      assert(value &&
             "mapメソッドでトランザクションに対応する値がありませんでした");
      return f(**value);
    };
    StreamInternal<U> *inter = new StreamInternal<U>(cluster_id, updater);
    inter->listen(this->internal);
    return Stream<U>(inter);
  }

  template <class F> void listen(F f);

  template <class F> Stream<T> merge(Stream<T> s2, F f);

  Stream<T> orElse(Stream<T> s2);

  Cell<T> hold(T initial_value);

  template <class U, class F>
  Stream<typename std::invoke_result<F, T &, U &>::type> snapshot(Cell<U> c,
                                                                  F f) {
    using V = typename std::invoke_result<F, T &, U &>::type;
    ID cluster_id = clusterManager.current_id();
    std::function<std::optional<V>(ID)> updater = [internal = this->internal, c,
                                                   f](ID transaction_id) {
      std::shared_ptr<T> v1 = internal->unsafeSample(transaction_id);
      std::shared_ptr<U> v2 = c.internal->unsafeSample(transaction_id);
      return f(*v1, *v2);
    };
    StreamInternal<V> *inter = new StreamInternal<V>(cluster_id, updater);
    inter->listen(this->internal);
    // snapshotはCellの値が変化したときに動くものでは無いので child_to()
    // を呼び出す
    inter->child_to(c.internal);
    return Stream<V>(inter);
  }

  template <class F> Stream<T> filter(F f);

  Stream<T> gate(Cell<bool>);

  friend StreamLoop<T>;
  template <class U> friend class Cell;

  template <class U> friend Stream<U> filterOptional(Stream<std::optional<U>>);
};

template <class T> class StreamSink : public Stream<T> {
private:
public:
  StreamSink();

  void send(T value);
};

template <class T> class StreamLoop : public Stream<T> {
private:
  /**
   * 既にLoopしているか否か
   */
  bool looped;

public:
  StreamLoop();

  void loop(Stream<T> s);
};

// Stream
template <class T>
StreamInternal<T>::StreamInternal(
    ID cluster_id, std::function<std::optional<T>(ID transaction_id)> updater)
    : TimeInvariantValues(cluster_id), updater(updater){};

template <class T>
StreamInternal<T>::StreamInternal(ID cluster_id)
    : StreamInternal(
          cluster_id, (std::function<std::optional<T>(ID transaction_id)>)[](
                          ID transaction_id)
                          ->std::optional<T> {
                            assert(1 == 2 && "無効なupdaterが登録されています");
                          }) {}

template <class T> void StreamInternal<T>::send(T value) {
  if (current_transaction == nullptr) {
    Transaction trans;
    send(value, current_transaction);
  } else {
    send(value, current_transaction);
  }
}

template <class T>
void StreamInternal<T>::send(T value, Transaction *transaction) {
  {
    {
      std::lock_guard<std::mutex> lock(mtx);
      values[transaction->get_id()] = std::make_shared<T>(value);
    }
    this->register_listeners_update(transaction);
    this->register_cleanup(transaction);
  }
}

template <class T>
std::optional<std::shared_ptr<T>> StreamInternal<T>::sample(ID transaction_id) {
  std::lock_guard<std::mutex> lock(mtx);
  auto itr = values.find(transaction_id);
  if (itr == values.end()) {
    return std::nullopt;
  }
  return itr->second;
}

template <class T>
std::shared_ptr<T> StreamInternal<T>::unsafeSample(ID transaction_id) {
  std::optional<std ::shared_ptr<T>> res = this->sample(transaction_id);
  assert(res && "論理時刻に対応する値がStreamに存在しませんでした");
  return *res;
}

template <class T>
void StreamInternal<T>::listenFromOuter(
    std::function<void(std::shared_ptr<T>)> f) {
  listeners.push_back(f);
}

template <class T> void StreamInternal<T>::update(Transaction *transaction) {
  ID transaction_id = transaction->get_id();
  std::optional<T> res = updater(transaction_id);
  if (res) {
    this->send(*res, transaction);
  }
}

template <class T> void StreamInternal<T>::refresh(ID transaction_id) {
  std::lock_guard<std::mutex> lock(mtx);
  auto itr = values.find(transaction_id);
  if (itr != values.end()) {
    values.erase(itr);
  }
}

template <class T> void StreamInternal<T>::finalize(Transaction *transaction) {
  std::shared_ptr<T> value = this->unsafeSample(transaction->get_id());
  for (std::function<void(std::shared_ptr<T>)> &listener : listeners) {
    listener(value);
  }
}

template <class T>
Stream<T>::Stream(StreamInternal<T> *internal) : internal(internal) {}

template <class T>
Stream<T>::Stream()
    : internal(new StreamInternal<T>(clusterManager.current_id())) {}

template <class T> template <class F> void Stream<T>::listen(F f) {
  this->internal->listenFromOuter([f](std::shared_ptr<T> v) -> void { f(*v); });
}

template <class T>
template <class F>
Stream<T> Stream<T>::merge(Stream<T> s2, F f) {
  ID cluster_id = clusterManager.current_id();
  std::function<std::optional<T>(ID)> updater = [internal = this->internal, s2,
                                                 f](ID id) -> T {
    std::optional<std::shared_ptr<T>> v1 = internal->sample(id);
    std::optional<std::shared_ptr<T>> v2 = s2.internal->sample(id);
    if (v1 and v2) {
      return f(**v1, **v2);
    }
    if (not v2) {
      return **v1;
    }
    return **v2;
  };
  StreamInternal<T> *inter = new StreamInternal<T>(cluster_id, updater);
  inter->listen(this->internal);
  inter->listen(s2.internal);
  return Stream<T>(inter);
}

template <class T> Stream<T> Stream<T>::orElse(Stream<T> s2) {
  return this->merge(s2, [](T x, T y) -> T { return x; });
}

template <class T> Cell<T> Stream<T>::hold(T initial_value) {
  ID cluster_id = clusterManager.current_id();
  std::function<std::optional<T>(ID)> updater =
      [internal = this->internal](ID id) -> T {
    std::shared_ptr<T> res = internal->unsafeSample(id);
    return *res;
  };
  CellInternal<T> *inter =
      new CellInternal<T>(cluster_id, initial_value, updater);
  inter->listen(this->internal);
  return Cell<T>(inter);
}

template <class T> template <class F> Stream<T> Stream<T>::filter(F f) {
  ID cluster_id = clusterManager.current_id();
  std::function<std::optional<T>(ID)> updater = [internal = this->internal,
                                                 f](ID id) -> std::optional<T> {
    std::shared_ptr<T> res = internal->unsafeSample(id);
    if (f(*res)) {
      return *res;
    }
    return std::nullopt;
  };
  StreamInternal<T> *inter = new StreamInternal<T>(cluster_id, updater);
  inter->listen(this->internal);
  return Stream<T>(inter);
}

template <class T> Stream<T> filterOptional(Stream<std::optional<T>> s) {
  return s.filter([](std::optional<T> x) -> bool { return x.has_value(); })
      .map([](std::optional<T> x) -> T { return *x; });
}

template <class T> Stream<T> Stream<T>::gate(Cell<bool> c) {
  ID cluster_id = clusterManager.current_id();
  std::function<std::optional<T>(ID)> updater = [internal = this->internal,
                                                 c](ID id) -> std::optional<T> {
    if (not *c.internal->unsafeSample(id)) {
      return std::nullopt;
    }
    return *internal->unsafeSample(id);
  };
  StreamInternal<T> *inter = new StreamInternal<T>(cluster_id, updater);
  inter->listen(this->internal);
  // gateはCellの値が変化したときに動くものでは無いので child_to()
  // を呼び出す
  inter->child_to(c.internal);
  return Stream<T>(inter);
}

template <class T>
StreamSink<T>::StreamSink()
    : prf::Stream<T>(
          new StreamInternal<T>(ClusterManager::UNMANAGED_CLUSTER_ID)) {}

template <class T> void StreamSink<T>::send(T value) {
  this->internal->send(value);
}

template <class T> StreamLoop<T>::StreamLoop() : Stream<T>() {}

template <class T> void StreamLoop<T>::loop(Stream<T> s) {
  assert(not this->looped &&
         "StreamLoopに対して複数回loopメソッドを呼び出しています");
  this->looped = true;

  ID cluster_id = clusterManager.current_id();
  std::function<T(ID)> updater = [s](ID transaction_id) {
    std::shared_ptr<T> res = s.internal->unsafeSample(transaction_id);
    return *res;
  };
  // 強引にupdaterを置き変えているがC++で綺麗なコードを書くことは諦める
  this->internal->updater = updater;
  this->internal->listen_over_loop(s.internal);
}

} // namespace prf
