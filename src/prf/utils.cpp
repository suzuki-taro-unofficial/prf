#include "prf/utils.hpp"
#include <chrono>
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
  while (not this->already_done.load()) {
    std::unique_lock<std::mutex> lock(this->mtx);
    this->cond.wait_for(lock, std::chrono::milliseconds(1),
                        [&already_done = this->already_done]() -> bool {
                          return already_done.load();
                        });
  }
}

bool Waiter::sample() {
  std::lock_guard<std::mutex> lock(this->mtx);
  return this->already_done.load();
}
} // namespace utils
} // namespace prf
