#pragma once

#include "cluster.hpp"
#include "time_invariant_values.hpp"
#include "transaction.hpp"
#include <functional>
#include <map>
#include <memory>
#include <mutex>

namespace prf {
// 一瞬だけ値が存在するような時変値を表すクラス
// Stream系列が内部で保持する
template <class T> class StreamInternal : public TimeInvariantValues {
private:
  StreamInternal<T>(ID cluster_id, std::function<T(ID transaction_id)> updater);

  StreamInternal<T>(ID cluster_id);

  // トランザクションIDに対応する値を保存する
  std::map<ID, std::shared_ptr<T>> values;

  // values自体の排他ロックのためにある
  // values[x]
  // が返す個々の参照自体には排他ロックが存在しないので、それを前提とした設計をする必要がある
  std::mutex mtx;

  std::function<T(ID transaction_id)> updater;

public:
  // transaction以前(現在実行中のトランザクションを含めた)に生成された値を取得する
  std::shared_ptr<T> sample(Transaction *transaction);

  // transactionに対応する時刻にvalueを登録する
  void send(T value, Transaction *transaction);

  void send(T value);

  void update(ID transaction_id) override;

  void refresh(ID transaction_id) override;
};

template <class T> class Stream {
protected:
  StreamInternal<T> *internal;

public:
  Stream(StreamInternal<T> *internal);
  Stream();

  template <class U> Stream<U> map(std::function<U(std::shared_ptr<T>)> f) {
    ID cluster_id = clusterManager.current_id();
    std::function<U(ID)> updater = [this, f](ID transaction_id) {
      return f(this->internal->sample(transaction_id));
    };
    StreamInternal<U> *inter = new StreamInternal<U>(cluster_id, updater);
    inter->listen(this);
    return Stream(inter);
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
} // namespace prf
