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

} // namespace prf
