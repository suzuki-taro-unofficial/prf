#include "prf/cluster.hpp"

namespace prf {
// ClusterManager

/**
 * 現在属しているクラスターのID
 * 0はSink系に与えられているので1からになっている
 * ここで発行する値は一時的なものであって、NodeManagerが再度割り当てることに注意
 */
u64 ClusterManager::global_current_id = 1;
// クラスターを生成しなくても暗黙的に生成されたクラスターに属すようにしておく
u64 ClusterManager::current_clusters = 1;

u64 ClusterManager::next_id() {
  ClusterManager::global_current_id += 1;
  return ClusterManager::global_current_id;
}

u64 ClusterManager::current_id() { return ClusterManager::global_current_id; }

bool ClusterManager::is_in_cluster() {
  return ClusterManager::current_clusters > 0;
}

void ClusterManager::enter_cluster() {
  next_id();
  ++ClusterManager::current_clusters;
}

void ClusterManager::exit_cluster() {
  next_id();
  --ClusterManager::current_clusters;
}
ClusterManager clusterManager = ClusterManager();

/**
 * この値が0であることを前提としてコードが組まれているので(0以外にすると対応がかなりめんどう)、変更することはオススメしない
 */
const ID ClusterManager::UNMANAGED_CLUSTER_ID = 0;

// class Cluster
Cluster::Cluster() { clusterManager.enter_cluster(); }
Cluster::~Cluster() { clusterManager.exit_cluster(); }
} // namespace prf
