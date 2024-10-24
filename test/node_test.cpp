#include "node.hpp"

#include <cassert>

void build_test1() {
  prf::NodeManager nodeManager;

  prf::Node node1(0);
  prf::Node node2(0);

  nodeManager.register_node(&node1);
  nodeManager.register_node(&node2);

  nodeManager.build();

  assert(node1.get_cluster_id() != node2.get_cluster_id() &&
         "依存関係の無いノードはビルド時に別クラスタへ再割り当てされる");
}

void build_test2() {
  prf::NodeManager nodeManager;
  prf::Node node1(0);
  prf::Node node2(1);

  node1.link_to(&node2);

  nodeManager.register_node(&node1);
  nodeManager.register_node(&node2);

  nodeManager.build();

  assert(node1.get_cluster_id() != node2.get_cluster_id() &&
         "依存関係の有るノードでもクラスタが異なればビルド後も異なるクラスタに"
         "割り当てられる");
}

void build_test3() {
  prf::NodeManager nodeManager;

  prf::Node node1(0);
  prf::Node node2(1);
  prf::Node node3(2);

  node1.link_to(&node2);
  node2.link_to(&node3);

  nodeManager.register_node(&node1);
  nodeManager.register_node(&node2);
  nodeManager.register_node(&node3);

  nodeManager.build();

  const auto ranks = nodeManager.get_cluster_ranks();

  assert(ranks[node1.get_cluster_id()] < ranks[node2.get_cluster_id()] &&
         "親は子よりもランクの値が小さい");
  assert(ranks[node2.get_cluster_id()] < ranks[node3.get_cluster_id()] &&
         "親は子よりもランクの値が小さい");
}

void build_test4() {
  prf::NodeManager nodeManager;

  prf::Node node1(0);
  prf::Node node2(1);
  prf::Node node3(1);
  prf::Node node4(2);

  node1.link_to(&node2);
  node1.link_to(&node3);
  node2.link_to(&node4);
  node3.link_to(&node4);

  nodeManager.register_node(&node1);
  nodeManager.register_node(&node2);
  nodeManager.register_node(&node3);
  nodeManager.register_node(&node4);

  nodeManager.build();

  assert(node2.get_cluster_id() != node3.get_cluster_id() &&
         "依存関係の無いノードはビルド時に別クラスタへ再割り当てされる");
}

void build_test5() {
  prf::NodeManager nodeManager;

  prf::Node node1(0);
  prf::Node node2(0);
  prf::Node node3(0);

  node1.link_to(&node3);
  node2.link_to(&node3);

  nodeManager.register_node(&node1);
  nodeManager.register_node(&node2);
  nodeManager.register_node(&node3);

  nodeManager.build();

  assert(node1.get_cluster_id() == node2.get_cluster_id() &&
         "連結の判定は依存関係の向きに関わらず行なわれる");
}

int main() {
  build_test1();
  build_test2();
  build_test3();
  build_test4();
  build_test5();
}
