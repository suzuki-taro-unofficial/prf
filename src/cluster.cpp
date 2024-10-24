#include "cluster.hpp"

namespace prf {
// ClusterManager
u64 ClusterManager::global_current_id = 0;
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

// class Cluster
Cluster::Cluster() { clusterManager.enter_cluster(); }
Cluster::~Cluster() { clusterManager.exit_cluster(); }
} // namespace prf
