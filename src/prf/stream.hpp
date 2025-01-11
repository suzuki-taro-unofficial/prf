#pragma once

#include "prf/cell.hpp"
#include "prf/cluster.hpp"
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
  void send(T value, InnerTransaction *transaction);

  void send(T value);

  void update(InnerTransaction *transaction) override;

  void refresh(ID transaction_id) override;

  void finalize(InnerTransaction *transaction) override;

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
  Stream<typename std::invoke_result<F, T &>::type> map(F f) const {
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

  template <class F> void listen(F f) const;

  template <class F> Stream<T> merge(Stream<T> s2, F f) const;

  Stream<T> or_else(Stream<T> s2) const;

  Cell<T> hold(T initial_value) const;

  template <class U> Stream<U> map_to(U x) const {
    this->map([x](const T &tmp) -> U { return x; });
  }

  template <class U1, class F>
  Stream<typename std::invoke_result<F, T &, U1 &>::type> snapshot(Cell<U1> c1,
                                                                   F f) const {
    using V = typename std::invoke_result<F, T &, U1 &>::type;
    ID cluster_id = clusterManager.current_id();
    std::function<std::optional<V>(ID)> updater = [internal = this->internal,
                                                   c1, f](ID transaction_id) {
      std::shared_ptr<T> v = internal->unsafeSample(transaction_id);
      std::shared_ptr<U1> v1 = c1.internal->unsafeSample(transaction_id);
      return f(*v, *v1);
    };
    StreamInternal<V> *inter = new StreamInternal<V>(cluster_id, updater);
    inter->listen(this->internal);
    // snapshotはCellの値が変化したときに動くものでは無いので child_to()
    // を呼び出す
    inter->child_to(c1.internal);
    return Stream<V>(inter);
  }

  template <class U1, class U2, class F>
  Stream<typename std::invoke_result<F, T &, U1 &, U2 &>::type>
  snapshot(Cell<U1> c1, Cell<U2> c2, F f) const {
    using V = typename std::invoke_result<F, T &, U1 &, U2 &>::type;
    ID cluster_id = clusterManager.current_id();
    std::function<std::optional<V>(ID)> updater =
        [internal = this->internal, c1, c2, f](ID transaction_id) {
          std::shared_ptr<T> v = internal->unsafeSample(transaction_id);
          std::shared_ptr<U1> v1 = c1.internal->unsafeSample(transaction_id);
          std::shared_ptr<U2> v2 = c2.internal->unsafeSample(transaction_id);
          return f(*v, *v1, *v2);
        };
    StreamInternal<V> *inter = new StreamInternal<V>(cluster_id, updater);
    inter->listen(this->internal);
    // snapshotはCellの値が変化したときに動くものでは無いので child_to()
    // を呼び出す
    inter->child_to(c1.internal);
    inter->child_to(c2.internal);
    return Stream<V>(inter);
  }

  template <class U1, class U2, class U3, class F>
  Stream<typename std::invoke_result<F, T &, U1 &, U2 &, U3 &>::type>
  snapshot(Cell<U1> c1, Cell<U2> c2, Cell<U3> c3, F f) const {
    using V = typename std::invoke_result<F, T &, U1 &, U2 &, U3 &>::type;
    ID cluster_id = clusterManager.current_id();
    std::function<std::optional<V>(ID)> updater =
        [internal = this->internal, c1, c2, c3, f](ID transaction_id) {
          std::shared_ptr<T> v = internal->unsafeSample(transaction_id);
          std::shared_ptr<U1> v1 = c1.internal->unsafeSample(transaction_id);
          std::shared_ptr<U2> v2 = c2.internal->unsafeSample(transaction_id);
          std::shared_ptr<U3> v3 = c3.internal->unsafeSample(transaction_id);
          return f(*v, *v1, *v2, *v3);
        };
    StreamInternal<V> *inter = new StreamInternal<V>(cluster_id, updater);
    inter->listen(this->internal);
    // snapshotはCellの値が変化したときに動くものでは無いので child_to()
    // を呼び出す
    inter->child_to(c1.internal);
    inter->child_to(c2.internal);
    inter->child_to(c3.internal);
    return Stream<V>(inter);
  }

  template <class U1, class U2, class U3, class U4, class F>
  Stream<typename std::invoke_result<F, T &, U1 &, U2 &, U3 &, U4 &>::type>
  snapshot(Cell<U1> c1, Cell<U2> c2, Cell<U3> c3, Cell<U4> c4, F f) const {
    using V = typename std::invoke_result<F, T &, U1 &, U2 &, U3 &, U4 &>::type;
    ID cluster_id = clusterManager.current_id();
    std::function<std::optional<V>(ID)> updater =
        [internal = this->internal, c1, c2, c3, c4, f](ID transaction_id) {
          std::shared_ptr<T> v = internal->unsafeSample(transaction_id);
          std::shared_ptr<U1> v1 = c1.internal->unsafeSample(transaction_id);
          std::shared_ptr<U2> v2 = c2.internal->unsafeSample(transaction_id);
          std::shared_ptr<U3> v3 = c3.internal->unsafeSample(transaction_id);
          std::shared_ptr<U4> v4 = c4.internal->unsafeSample(transaction_id);
          return f(*v, *v1, *v2, *v3, *v4);
        };
    StreamInternal<V> *inter = new StreamInternal<V>(cluster_id, updater);
    inter->listen(this->internal);
    // snapshotはCellの値が変化したときに動くものでは無いので child_to()
    // を呼び出す
    inter->child_to(c1.internal);
    inter->child_to(c2.internal);
    inter->child_to(c3.internal);
    inter->child_to(c4.internal);
    return Stream<V>(inter);
  }

  template <class U1, class U2, class U3, class U4, class U5, class F>
  Stream<
      typename std::invoke_result<F, T &, U1 &, U2 &, U3 &, U4 &, U5 &>::type>
  snapshot(Cell<U1> c1, Cell<U2> c2, Cell<U3> c3, Cell<U4> c4, Cell<U5> c5,
           F f) const {
    using V =
        typename std::invoke_result<F, T &, U1 &, U2 &, U3 &, U4 &, U5 &>::type;
    ID cluster_id = clusterManager.current_id();
    std::function<std::optional<V>(ID)> updater =
        [internal = this->internal, c1, c2, c3, c4, c5, f](ID transaction_id) {
          std::shared_ptr<T> v = internal->unsafeSample(transaction_id);
          std::shared_ptr<U1> v1 = c1.internal->unsafeSample(transaction_id);
          std::shared_ptr<U2> v2 = c2.internal->unsafeSample(transaction_id);
          std::shared_ptr<U3> v3 = c3.internal->unsafeSample(transaction_id);
          std::shared_ptr<U4> v4 = c4.internal->unsafeSample(transaction_id);
          std::shared_ptr<U5> v5 = c5.internal->unsafeSample(transaction_id);
          return f(*v, *v1, *v2, *v3, *v4, *v5);
        };
    StreamInternal<V> *inter = new StreamInternal<V>(cluster_id, updater);
    inter->listen(this->internal);
    // snapshotはCellの値が変化したときに動くものでは無いので child_to()
    // を呼び出す
    inter->child_to(c1.internal);
    inter->child_to(c2.internal);
    inter->child_to(c3.internal);
    inter->child_to(c4.internal);
    inter->child_to(c5.internal);
    return Stream<V>(inter);
  }

  template <class U1, class U2, class U3, class U4, class U5, class U6, class F>
  Stream<typename std::invoke_result<F, T &, U1 &, U2 &, U3 &, U4 &, U5 &,
                                     U6 &>::type>
  snapshot(Cell<U1> c1, Cell<U2> c2, Cell<U3> c3, Cell<U4> c4, Cell<U5> c5,
           Cell<U6> c6, F f) const {
    using V = typename std::invoke_result<F, T &, U1 &, U2 &, U3 &, U4 &, U5 &,
                                          U6 &>::type;
    ID cluster_id = clusterManager.current_id();
    std::function<std::optional<V>(ID)> updater = [internal = this->internal,
                                                   c1, c2, c3, c4, c5, c6,
                                                   f](ID transaction_id) {
      std::shared_ptr<T> v = internal->unsafeSample(transaction_id);
      std::shared_ptr<U1> v1 = c1.internal->unsafeSample(transaction_id);
      std::shared_ptr<U2> v2 = c2.internal->unsafeSample(transaction_id);
      std::shared_ptr<U3> v3 = c3.internal->unsafeSample(transaction_id);
      std::shared_ptr<U4> v4 = c4.internal->unsafeSample(transaction_id);
      std::shared_ptr<U5> v5 = c5.internal->unsafeSample(transaction_id);
      std::shared_ptr<U6> v6 = c6.internal->unsafeSample(transaction_id);
      return f(*v, *v1, *v2, *v3, *v4, *v5, *v6);
    };
    StreamInternal<V> *inter = new StreamInternal<V>(cluster_id, updater);
    inter->listen(this->internal);
    // snapshotはCellの値が変化したときに動くものでは無いので child_to()
    // を呼び出す
    inter->child_to(c1.internal);
    inter->child_to(c2.internal);
    inter->child_to(c3.internal);
    inter->child_to(c4.internal);
    inter->child_to(c5.internal);
    inter->child_to(c6.internal);
    return Stream<V>(inter);
  }

  template <class U1, class U2, class U3, class U4, class U5, class U6,
            class U7, class F>
  Stream<typename std::invoke_result<F, T &, U1 &, U2 &, U3 &, U4 &, U5 &, U6 &,
                                     U7 &>::type>
  snapshot(Cell<U1> c1, Cell<U2> c2, Cell<U3> c3, Cell<U4> c4, Cell<U5> c5,
           Cell<U6> c6, Cell<U7> c7, F f) const {
    using V = typename std::invoke_result<F, T &, U1 &, U2 &, U3 &, U4 &, U5 &,
                                          U6 &, U7 &>::type;
    ID cluster_id = clusterManager.current_id();
    std::function<std::optional<V>(ID)> updater = [internal = this->internal,
                                                   c1, c2, c3, c4, c5, c6, c7,
                                                   f](ID transaction_id) {
      std::shared_ptr<T> v = internal->unsafeSample(transaction_id);
      std::shared_ptr<U1> v1 = c1.internal->unsafeSample(transaction_id);
      std::shared_ptr<U2> v2 = c2.internal->unsafeSample(transaction_id);
      std::shared_ptr<U3> v3 = c3.internal->unsafeSample(transaction_id);
      std::shared_ptr<U4> v4 = c4.internal->unsafeSample(transaction_id);
      std::shared_ptr<U5> v5 = c5.internal->unsafeSample(transaction_id);
      std::shared_ptr<U6> v6 = c6.internal->unsafeSample(transaction_id);
      std::shared_ptr<U7> v7 = c7.internal->unsafeSample(transaction_id);
      return f(*v, *v1, *v2, *v3, *v4, *v5, *v6, *v7);
    };
    StreamInternal<V> *inter = new StreamInternal<V>(cluster_id, updater);
    inter->listen(this->internal);
    // snapshotはCellの値が変化したときに動くものでは無いので child_to()
    // を呼び出す
    inter->child_to(c1.internal);
    inter->child_to(c2.internal);
    inter->child_to(c3.internal);
    inter->child_to(c4.internal);
    inter->child_to(c5.internal);
    inter->child_to(c6.internal);
    inter->child_to(c7.internal);
    return Stream<V>(inter);
  }

  template <class U1, class U2, class U3, class U4, class U5, class U6,
            class U7, class U8, class F>
  Stream<typename std::invoke_result<F, T &, U1 &, U2 &, U3 &, U4 &, U5 &, U6 &,
                                     U7 &, U8 &>::type>
  snapshot(Cell<U1> c1, Cell<U2> c2, Cell<U3> c3, Cell<U4> c4, Cell<U5> c5,
           Cell<U6> c6, Cell<U7> c7, Cell<U8> c8, F f) const {
    using V = typename std::invoke_result<F, T &, U1 &, U2 &, U3 &, U4 &, U5 &,
                                          U6 &, U7 &, U8 &>::type;
    ID cluster_id = clusterManager.current_id();
    std::function<std::optional<V>(ID)> updater = [internal = this->internal,
                                                   c1, c2, c3, c4, c5, c6, c7,
                                                   c8, f](ID transaction_id) {
      std::shared_ptr<T> v = internal->unsafeSample(transaction_id);
      std::shared_ptr<U1> v1 = c1.internal->unsafeSample(transaction_id);
      std::shared_ptr<U2> v2 = c2.internal->unsafeSample(transaction_id);
      std::shared_ptr<U3> v3 = c3.internal->unsafeSample(transaction_id);
      std::shared_ptr<U4> v4 = c4.internal->unsafeSample(transaction_id);
      std::shared_ptr<U5> v5 = c5.internal->unsafeSample(transaction_id);
      std::shared_ptr<U6> v6 = c6.internal->unsafeSample(transaction_id);
      std::shared_ptr<U7> v7 = c7.internal->unsafeSample(transaction_id);
      std::shared_ptr<U8> v8 = c8.internal->unsafeSample(transaction_id);
      return f(*v, *v1, *v2, *v3, *v4, *v5, *v6, *v7, *v8);
    };
    StreamInternal<V> *inter = new StreamInternal<V>(cluster_id, updater);
    inter->listen(this->internal);
    // snapshotはCellの値が変化したときに動くものでは無いので child_to()
    // を呼び出す
    inter->child_to(c1.internal);
    inter->child_to(c2.internal);
    inter->child_to(c3.internal);
    inter->child_to(c4.internal);
    inter->child_to(c5.internal);
    inter->child_to(c6.internal);
    inter->child_to(c7.internal);
    inter->child_to(c8.internal);
    return Stream<V>(inter);
  }

  template <class U1, class U2, class U3, class U4, class U5, class U6,
            class U7, class U8, class U9, class F>
  Stream<typename std::invoke_result<F, T &, U1 &, U2 &, U3 &, U4 &, U5 &, U6 &,
                                     U7 &, U8 &, U9 &>::type>
  snapshot(Cell<U1> c1, Cell<U2> c2, Cell<U3> c3, Cell<U4> c4, Cell<U5> c5,
           Cell<U6> c6, Cell<U7> c7, Cell<U8> c8, Cell<U9> c9, F f) const {
    using V = typename std::invoke_result<F, T &, U1 &, U2 &, U3 &, U4 &, U5 &,
                                          U6 &, U7 &, U8 &, U9 &>::type;
    ID cluster_id = clusterManager.current_id();
    std::function<std::optional<V>(ID)> updater =
        [internal = this->internal, c1, c2, c3, c4, c5, c6, c7, c8, c9,
         f](ID transaction_id) {
          std::shared_ptr<T> v = internal->unsafeSample(transaction_id);
          std::shared_ptr<U1> v1 = c1.internal->unsafeSample(transaction_id);
          std::shared_ptr<U2> v2 = c2.internal->unsafeSample(transaction_id);
          std::shared_ptr<U3> v3 = c3.internal->unsafeSample(transaction_id);
          std::shared_ptr<U4> v4 = c4.internal->unsafeSample(transaction_id);
          std::shared_ptr<U5> v5 = c5.internal->unsafeSample(transaction_id);
          std::shared_ptr<U6> v6 = c6.internal->unsafeSample(transaction_id);
          std::shared_ptr<U7> v7 = c7.internal->unsafeSample(transaction_id);
          std::shared_ptr<U8> v8 = c8.internal->unsafeSample(transaction_id);
          std::shared_ptr<U9> v9 = c9.internal->unsafeSample(transaction_id);
          return f(*v, *v1, *v2, *v3, *v4, *v5, *v6, *v7, *v8, *v9);
        };
    StreamInternal<V> *inter = new StreamInternal<V>(cluster_id, updater);
    inter->listen(this->internal);
    // snapshotはCellの値が変化したときに動くものでは無いので child_to()
    // を呼び出す
    inter->child_to(c1.internal);
    inter->child_to(c2.internal);
    inter->child_to(c3.internal);
    inter->child_to(c4.internal);
    inter->child_to(c5.internal);
    inter->child_to(c6.internal);
    inter->child_to(c7.internal);
    inter->child_to(c8.internal);
    inter->child_to(c9.internal);
    return Stream<V>(inter);
  }

  template <class U1, class U2, class U3, class U4, class U5, class U6,
            class U7, class U8, class U9, class U10, class F>
  Stream<typename std::invoke_result<F, T &, U1 &, U2 &, U3 &, U4 &, U5 &, U6 &,
                                     U7 &, U8 &, U9 &, U10 &>::type>
  snapshot(Cell<U1> c1, Cell<U2> c2, Cell<U3> c3, Cell<U4> c4, Cell<U5> c5,
           Cell<U6> c6, Cell<U7> c7, Cell<U8> c8, Cell<U9> c9, Cell<U10> c10,
           F f) const {
    using V = typename std::invoke_result<F, T &, U1 &, U2 &, U3 &, U4 &, U5 &,
                                          U6 &, U7 &, U8 &, U9 &, U10 &>::type;
    ID cluster_id = clusterManager.current_id();
    std::function<std::optional<V>(ID)> updater =
        [internal = this->internal, c1, c2, c3, c4, c5, c6, c7, c8, c9, c10,
         f](ID transaction_id) {
          std::shared_ptr<T> v = internal->unsafeSample(transaction_id);
          std::shared_ptr<U1> v1 = c1.internal->unsafeSample(transaction_id);
          std::shared_ptr<U2> v2 = c2.internal->unsafeSample(transaction_id);
          std::shared_ptr<U3> v3 = c3.internal->unsafeSample(transaction_id);
          std::shared_ptr<U4> v4 = c4.internal->unsafeSample(transaction_id);
          std::shared_ptr<U5> v5 = c5.internal->unsafeSample(transaction_id);
          std::shared_ptr<U6> v6 = c6.internal->unsafeSample(transaction_id);
          std::shared_ptr<U7> v7 = c7.internal->unsafeSample(transaction_id);
          std::shared_ptr<U8> v8 = c8.internal->unsafeSample(transaction_id);
          std::shared_ptr<U9> v9 = c9.internal->unsafeSample(transaction_id);
          std::shared_ptr<U10> v10 = c10.internal->unsafeSample(transaction_id);
          return f(*v, *v1, *v2, *v3, *v4, *v5, *v6, *v7, *v8, *v9, *v10);
        };
    StreamInternal<V> *inter = new StreamInternal<V>(cluster_id, updater);
    inter->listen(this->internal);
    // snapshotはCellの値が変化したときに動くものでは無いので child_to()
    // を呼び出す
    inter->child_to(c1.internal);
    inter->child_to(c2.internal);
    inter->child_to(c3.internal);
    inter->child_to(c4.internal);
    inter->child_to(c5.internal);
    inter->child_to(c6.internal);
    inter->child_to(c7.internal);
    inter->child_to(c8.internal);
    inter->child_to(c9.internal);
    inter->child_to(c10.internal);
    return Stream<V>(inter);
  }

  template <class F> Stream<T> filter(F f) const;

  Stream<T> gate(Cell<bool>) const;

  friend StreamLoop<T>;
  template <class U> friend class Cell;

  template <class U> friend Stream<U> filterOptional(Stream<std::optional<U>>);
};

template <class T> class StreamSink : public Stream<T> {
private:
public:
  StreamSink();

  void send(T value) const;
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
    InnerTransaction trans;
    send(value, current_transaction);
  } else {
    send(value, current_transaction);
  }
}

template <class T>
void StreamInternal<T>::send(T value, InnerTransaction *transaction) {
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

template <class T>
void StreamInternal<T>::update(InnerTransaction *transaction) {
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

template <class T>
void StreamInternal<T>::finalize(InnerTransaction *transaction) {
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

template <class T> template <class F> void Stream<T>::listen(F f) const {
  this->internal->listenFromOuter([f](std::shared_ptr<T> v) -> void { f(*v); });
}

template <class T>
template <class F>
Stream<T> Stream<T>::merge(Stream<T> s2, F f) const {
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

template <class T> Stream<T> Stream<T>::or_else(Stream<T> s2) const {
  return this->merge(s2, [](T x, T y) -> T { return x; });
}

template <class T> Cell<T> Stream<T>::hold(T initial_value) const {
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

template <class T> template <class F> Stream<T> Stream<T>::filter(F f) const {
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

template <class T> Stream<T> Stream<T>::gate(Cell<bool> c) const {
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

template <class T> void StreamSink<T>::send(T value) const {
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
