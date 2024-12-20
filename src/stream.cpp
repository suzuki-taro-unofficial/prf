#include "stream.hpp"
#include "cluster.hpp"
#include "time_invariant_values.hpp"
#include "transaction.hpp"
#include <cassert>
#include <memory>
#include <mutex>

namespace prf {
// Stream
template <class T>
StreamInternal<T>::StreamInternal(ID cluster_id,
                                  std::function<T(ID transaction_id)> updater)
    : TimeInvariantValues(cluster_id), updater(updater){};

template <class T>
StreamInternal<T>::StreamInternal(ID cluster_id)
    : StreamInternal(cluster_id, [](ID transaction_id) {
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
    values[transaction->get_id()] = std::make_shared(value);
    transaction->register_update(this);
  }
}

template <class T>
std::shared_ptr<T> StreamInternal<T>::sample(Transaction *transaction) {
  std::lock_guard<std::mutex> lock(mtx);
  ID id = transaction->get_id();
  auto itr = values.find(id);
  assert(itr != values.end() &&
         "TransactionIDに対応する値がStreamInternalにありません");
  return *itr;
}

template <class T> void StreamInternal<T>::update(ID transaction_id) {
  std::shared_ptr<T> value = std::make_shared<T>(updater(transaction_id));
  std::lock_guard<std::mutex> lock(mtx);
  values[transaction_id] = value;
}

template <class T> void StreamInternal<T>::refresh(ID transaction_id) {
  std::lock_guard<std::mutex> lock(mtx);
  auto itr = values.find(transaction_id);
  if (itr != values.end()) {
    values.erase(itr);
  }
}

template <class T>
Stream<T>::Stream(StreamInternal<T> *internal) : internal(internal) {}

template <class T>
Stream<T>::Stream() : internal(clusterManager.current_id()) {}

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
