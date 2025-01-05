#include "prf.hpp"

#include "executor.hpp"
#include "node.hpp"
#include "planner.hpp"
#include "rank.hpp"

namespace prf {
void build() {
  NodeManager::globalNodeManager->build();
  std::vector<Rank> ranks = NodeManager::globalNodeManager->get_cluster_ranks();

  Executor::initialize();
  PlannerManager::initialize(ranks);
}

void initialize() {
  // メモリリークは一旦考えない方向で
  NodeManager::globalNodeManager = new NodeManager;
  PlannerManager::globalPlannerManager = nullptr;
  Executor::global_executor = nullptr;

  while (PlannerManager::messages.try_pop()) {
  }
  while (Executor::messages.try_pop()) {
  }
}

} // namespace prf
