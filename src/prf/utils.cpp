#include "prf/utils.hpp"
#include <mutex>

namespace prf {
namespace utils {

Waiter::Waiter() : already_done(false) {}
Waiter::~Waiter() {}

void Waiter::done() {
  std::lock_guard<std::mutex> lock(this->mtx);
  this->already_done.store(true);
  this->cond.notify_all();
}

void Waiter::wait() {
  std::unique_lock<std::mutex> lock(this->mtx);
  this->cond.wait(lock, [&already_done = this->already_done]() -> bool {
    return already_done.load();
  });
}

bool Waiter::sample() {
  std::lock_guard<std::mutex> lock(this->mtx);
  return this->already_done.load();
}
} // namespace utils
} // namespace prf
