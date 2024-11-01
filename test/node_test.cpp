#include "node.hpp"
#include "rank.hpp"

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

void build_test6() {
  prf::NodeManager nodeManager;
  prf::Node node1(0);
  prf::Node node2(0);
  prf::Node node3(0);

  node1.link_to(&node2);
  node2.link_to(&node3);

  nodeManager.register_node(&node1);
  nodeManager.register_node(&node2);
  nodeManager.register_node(&node3);

  nodeManager.build();

  assert(node1.get_in_cluster_rank() == prf::Rank(0) &&
         "誰にも依存していないノードのランクは0");
  assert(node2.get_in_cluster_rank() == prf::Rank(1) &&
         "ランク0のノードにだけ依存しているノードのランクは1");
  assert(node3.get_in_cluster_rank() == prf::Rank(2) &&
         "ランク1のノードにだけ依存しているノードのランクは2");
  assert(node1.get_in_cluster_rank() < node2.get_in_cluster_rank() &&
         "依存関係に基づいてランクが昇順になるように割り当てられる");
  assert(node2.get_in_cluster_rank() < node3.get_in_cluster_rank() &&
         "依存関係に基づいてランクが昇順になるように割り当てられる");
}

void build_test7() {
  prf::NodeManager nodeManager;
  prf::Node node1(0);
  prf::Node node2(0);
  prf::Node node3(0);
  prf::Node node4(0);

  node1.link_to(&node2);
  node1.link_to(&node3);
  node2.link_to(&node4);
  node3.link_to(&node4);

  nodeManager.register_node(&node1);
  nodeManager.register_node(&node2);
  nodeManager.register_node(&node3);
  nodeManager.register_node(&node4);

  nodeManager.build();

  assert(node1.get_in_cluster_rank() == prf::Rank(0) &&
         "誰にも依存していないノードのランクは0");
  assert(node2.get_in_cluster_rank() == prf::Rank(1) &&
         "ランク0のノードにだけ依存しているノードのランクは1");
  assert(node3.get_in_cluster_rank() == prf::Rank(1) &&
         "ランク0のノードにだけ依存しているノードのランクは1");
  assert(node4.get_in_cluster_rank() == prf::Rank(2) &&
         "ランク1のノードにだけ依存しているノードのランクは2");
}

void build_test8() {
  prf::NodeManager nodeManager;
  prf::Node node1(0);
  prf::Node node2(0);
  prf::Node node3(0);

  node1.link_to(&node2);
  node1.link_to(&node3);
  node2.link_to(&node3);

  nodeManager.register_node(&node1);
  nodeManager.register_node(&node2);
  nodeManager.register_node(&node3);

  nodeManager.build();

  assert(node1.get_in_cluster_rank() == prf::Rank(0) &&
         "誰にも依存していないノードのランクは0");
  assert(node2.get_in_cluster_rank() == prf::Rank(1) &&
         "ランク0のノードにだけ依存しているノードのランクは1");
  assert(node3.get_in_cluster_rank() == prf::Rank(2) &&
         "ランク0と1のノードにだけ依存しているノードのランクは2");
}

void build_test9() {
  prf::NodeManager nodeManager;
  prf::Node node1(0);
  prf::Node node2(1);

  node1.link_to(&node2);

  nodeManager.build();

  assert(node1.get_in_cluster_rank() == prf::Rank(0) &&
         "誰にも依存していないノードのランクは0");
  assert(node2.get_in_cluster_rank() == prf::Rank(0) &&
         "依存しているノードが異なるクラスタである場合は依存関係が無いとして処"
         "理される");
}

void build_test10() {
  prf::NodeManager nodeManager;
  prf::Node node1(0);
  prf::Node node2(0);
  prf::Node node3(0);
  prf::Node node4(0);
  prf::Node node5(0);

  node1.same_cluster_to(&node2);

  node3.same_cluster_to(&node4);
  node4.same_cluster_to(&node5);

  nodeManager.build();

  assert(node1.get_cluster_id() == node2.get_cluster_id() &&
         "同じクラスタに属すると明示したものはビルド後に同一クラスタに属する");

  assert(node3.get_cluster_id() == node5.get_cluster_id() &&
         "同じクラスタに属すると明示したものはビルド後に同一クラスタに属する");
}

int main() {
  build_test1();
  build_test2();
  build_test3();
  build_test4();
  build_test5();
  build_test6();
  build_test7();
  build_test8();
  build_test9();
  build_test10();
}
