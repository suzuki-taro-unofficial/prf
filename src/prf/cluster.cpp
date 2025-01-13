#include "prf/cluster.hpp"
#include "logger.hpp"
#include "prf/node.hpp"

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
void Cluster::close() {
  if (this->closed) {
    failure_log("既にクラスターは終了しています");
  }
  this->closed = true;
  clusterManager.exit_cluster();
}
Cluster::Cluster() : Cluster("NO_NAME") {}
Cluster::~Cluster() {
  if (this->closed) {
    return;
  }
  clusterManager.exit_cluster();
}

Cluster::Cluster(std::string name) : closed(false) {
  clusterManager.enter_cluster();
  NodeManager::globalNodeManager->register_cluster_name(
      clusterManager.current_id(), name);
}
} // namespace prf
