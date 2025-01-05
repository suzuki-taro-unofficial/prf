#include "node.hpp"
#include "rank.hpp"

#include <cassert>

void build_test1() {
  prf::NodeManager nodeManager;

  prf::Node A(1);
  prf::Node B(1);

  nodeManager.register_node(&A);
  nodeManager.register_node(&B);

  nodeManager.build();

  assert(A.get_cluster_id() != B.get_cluster_id() &&
         "依存関係の無いノードはビルド時に別クラスタへ再割り当てされる");
}

void build_test2() {
  prf::NodeManager nodeManager;
  prf::Node A(1);
  prf::Node B(2);

  // A -(cluster)-> B;

  A.link_to(&B);

  nodeManager.register_node(&A);
  nodeManager.register_node(&B);

  nodeManager.build();

  assert(A.get_cluster_id() != B.get_cluster_id() &&
         "依存関係の有るノードでもクラスタが異なればビルド後も異なるクラスタに"
         "割り当てられる");
}

void build_test3() {
  prf::NodeManager nodeManager;

  prf::Node A(1);
  prf::Node B(2);
  prf::Node C(3);

  A.link_to(&B);
  B.link_to(&C);

  // A -(cluster)-> B -(cluster)-> C

  nodeManager.register_node(&A);
  nodeManager.register_node(&B);
  nodeManager.register_node(&C);

  nodeManager.build();

  const auto ranks = nodeManager.get_cluster_ranks();

  assert(ranks[A.get_cluster_id()] < ranks[B.get_cluster_id()] &&
         "親クラスタは子クラスタよりもランクの値が小さい");
  assert(ranks[B.get_cluster_id()] < ranks[C.get_cluster_id()] &&
         "親クラスタは子クラスタよりもランクの値が小さい");
}

void build_test4() {
  prf::NodeManager nodeManager;

  prf::Node A(1);
  prf::Node B(2);
  prf::Node C(2);
  prf::Node D(3);

  // A -(cluster)-> B -(cluster)-> D
  // A -(cluster)-> C -(cluster)-> D

  A.link_to(&B);
  A.link_to(&C);
  B.link_to(&D);
  C.link_to(&D);

  nodeManager.register_node(&A);
  nodeManager.register_node(&B);
  nodeManager.register_node(&C);
  nodeManager.register_node(&D);

  nodeManager.build();

  assert(B.get_cluster_id() != C.get_cluster_id() &&
         "依存関係の無いノードはビルド時に別クラスタへ再割り当てされる");
}

void build_test5() {
  prf::NodeManager nodeManager;

  prf::Node A(1);
  prf::Node B(1);
  prf::Node C(1);

  // A -> C
  // B -> C

  A.link_to(&C);
  B.link_to(&C);

  nodeManager.register_node(&A);
  nodeManager.register_node(&B);
  nodeManager.register_node(&C);

  nodeManager.build();

  assert(A.get_cluster_id() == B.get_cluster_id() &&
         "連結の判定は依存関係の向きに関わらず行なわれる");
}

void build_test6() {
  prf::NodeManager nodeManager;
  prf::Node A(1);
  prf::Node B(1);
  prf::Node C(1);

  // A -> B -> C

  A.link_to(&B);
  B.link_to(&C);

  nodeManager.register_node(&A);
  nodeManager.register_node(&B);
  nodeManager.register_node(&C);

  nodeManager.build();

  assert(A.get_in_cluster_rank() == prf::Rank(0) &&
         "誰にも依存していないノードのランクは0");
  assert(B.get_in_cluster_rank() == prf::Rank(1) &&
         "ランク0のノードにだけ依存しているノードのランクは1");
  assert(C.get_in_cluster_rank() == prf::Rank(2) &&
         "ランク1のノードにだけ依存しているノードのランクは2");
  assert(A.get_in_cluster_rank() < B.get_in_cluster_rank() &&
         "依存関係に基づいてランクが昇順になるように割り当てられる");
  assert(B.get_in_cluster_rank() < C.get_in_cluster_rank() &&
         "依存関係に基づいてランクが昇順になるように割り当てられる");
}

void build_test7() {
  prf::NodeManager nodeManager;
  prf::Node A(1);
  prf::Node B(1);
  prf::Node C(1);
  prf::Node D(1);

  // A -> B -> D
  // A -> C -> D

  A.link_to(&B);
  A.link_to(&C);
  B.link_to(&D);
  C.link_to(&D);

  nodeManager.register_node(&A);
  nodeManager.register_node(&B);
  nodeManager.register_node(&C);
  nodeManager.register_node(&D);

  nodeManager.build();

  assert(A.get_in_cluster_rank() == prf::Rank(0) &&
         "誰にも依存していないノードのランクは0");
  assert(B.get_in_cluster_rank() == prf::Rank(1) &&
         "ランク0のノードにだけ依存しているノードのランクは1");
  assert(C.get_in_cluster_rank() == prf::Rank(1) &&
         "ランク0のノードにだけ依存しているノードのランクは1");
  assert(D.get_in_cluster_rank() == prf::Rank(2) &&
         "ランク1のノードにだけ依存しているノードのランクは2");
}

void build_test8() {
  prf::NodeManager nodeManager;
  prf::Node A(1);
  prf::Node B(1);
  prf::Node C(1);

  // A -> B -> C
  // A -> C

  A.link_to(&B);
  A.link_to(&C);
  B.link_to(&C);

  nodeManager.register_node(&A);
  nodeManager.register_node(&B);
  nodeManager.register_node(&C);

  nodeManager.build();

  assert(A.get_in_cluster_rank() == prf::Rank(0) &&
         "誰にも依存していないノードのランクは0");
  assert(B.get_in_cluster_rank() == prf::Rank(1) &&
         "ランク0のノードにだけ依存しているノードのランクは1");
  assert(C.get_in_cluster_rank() == prf::Rank(2) &&
         "ランク0と1のノードにだけ依存しているノードのランクは2");
}

void build_test9() {
  prf::NodeManager nodeManager;
  prf::Node A(1);
  prf::Node B(2);

  // A -(cluster)-> B

  nodeManager.register_node(&A);
  nodeManager.register_node(&B);

  A.link_to(&B);

  nodeManager.build();

  assert(A.get_in_cluster_rank() == prf::Rank(0) &&
         "誰にも依存していないノードのランクは0");
  assert(B.get_in_cluster_rank() == prf::Rank(0) &&
         "依存しているノードが異なるクラスタである場合は依存関係が無いとして処"
         "理される");
}

void build_test10() {
  prf::NodeManager nodeManager;
  prf::Node A(1);
  prf::Node B(1);
  prf::Node C(1);
  prf::Node D(1);
  prf::Node E(1);

  // A -(loop)-> B
  // C -(loop)-> D -(loop)-> E

  nodeManager.register_node(&A);
  nodeManager.register_node(&B);
  nodeManager.register_node(&C);
  nodeManager.register_node(&D);
  nodeManager.register_node(&E);

  A.loop_child_to(&B);

  C.loop_child_to(&D);
  D.loop_child_to(&E);

  nodeManager.build();

  assert(A.get_cluster_id() == B.get_cluster_id() &&
         "Loopを利用すると必ず同じクラスタに属する");

  assert(C.get_cluster_id() == E.get_cluster_id() &&
         "Loopを利用すると必ず同じクラスタに属する");
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
