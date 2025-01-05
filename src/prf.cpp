#include "prf.hpp"

#include "executor.hpp"
#include "node.hpp"
#include "planner.hpp"
#include "rank.hpp"
#include "types.hpp"

namespace prf {
void build() {
  globalNodeManager.build();
  std::vector<Rank> ranks = globalNodeManager.get_cluster_ranks();

  Executor::initialize();
  PlannerManager::initialize(ranks);
}
} // namespace prf
