#pragma once

#include "cluster.hpp"
#include "time_invariant_values.hpp"
#include "transaction.hpp"
#include <cassert>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <optional>

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
  // transaction以前(現在実行中のトランザクションを含めた)に生成された値を取得する
  std::optional<std::shared_ptr<T>> sample(ID transaction_id);

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

template <class T> class Stream {
protected:
  StreamInternal<T> *internal;

public:
  Stream(StreamInternal<T> *internal);
  Stream();

  template <class U> Stream<U> map(std::function<U(std::shared_ptr<T>)> f) {
    ID cluster_id = clusterManager.current_id();
    std::function<std::optional<U>(ID)> updater = [this, f](ID transaction_id) {
      std::optional<std::shared_ptr<T>> value =
          this->internal->sample(transaction_id);
      assert(value &&
             "mapメソッドでトランザクションに対応する値がありませんでした");
      return f(*value);
    };
    StreamInternal<U> *inter = new StreamInternal<U>(cluster_id, updater);
    inter->listen(this->internal);
    return Stream(inter);
  }

  void listen(std::function<void(std::shared_ptr<T>)> f) {
    this->internal->listenFromOuter(f);
  }
};

template <class T> class StreamSink : public Stream<T> {
private:
public:
  StreamSink();

  void send(T value);
};

template <class T> class StreamLoop : public Stream<T> {
private:
public:
  StreamLoop(Stream<T> s);
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
    std::lock_guard<std::mutex> lock(mtx);
    values[transaction->get_id()] = std::make_shared<T>(value);
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
  std::shared_ptr<T> value = *this->sample(transaction->get_id());
  for (std::function<void(std::shared_ptr<T>)> &listener : listeners) {
    listener(value);
  }
}

template <class T>
Stream<T>::Stream(StreamInternal<T> *internal) : internal(internal) {}

template <class T>
Stream<T>::Stream()
    : internal(new StreamInternal<T>(clusterManager.current_id())) {}

template <class T> StreamSink<T>::StreamSink() : prf::Stream<T>() {}
template <class T> void StreamSink<T>::send(T value) {
  this->internal->send(value);
}

template <class T> StreamLoop<T>::StreamLoop(Stream<T> s) {
  ID cluster_id = clusterManager.current_id();
  std::function<T(ID)> updater = [this, s](ID transaction_id) {
    return *s.internal->sample(transaction_id);
  };
  StreamInternal<T> *inter = new StreamInternal<T>(cluster_id, updater);
  inter->listen_over_loop(this);
  // 強引にinternalを書き変えている(C++で綺麗に実装するのは諦める)
  this->internal = inter;
}
} // namespace prf
