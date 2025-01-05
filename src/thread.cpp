#include "thread.hpp"
#include "executor.hpp"
#include "planner.hpp"
#include <atomic>

namespace prf {
std::atomic_bool stop_the_threads(false);
void stop_execution() {
  stop_the_threads.store(true);
  PlannerManager::messages.notify_stop();
  Executor::messages.notify_stop();
}
} // namespace prf
