#pragma once

#include "rank.hpp"
#include <vector>

namespace prf {

// 依存関係を表現するオブジェクト
class Node {
private:
  // 所属しているクラスタのID
  ID cluster_id;

  // クラスター内の優先順位
  Rank in_cluster_rank;

  // このノードが依存しているノード
  std::vector<Node *> parents;
  // このノードに依存しているノード
  std::vector<Node *> childs;

public:
  Node(ID);

  ID get_cluster_id();
  void set_cluster_id(ID);
  Rank get_in_cluster_rank();

  const std::vector<Node *> &get_parents();
  const std::vector<Node *> &get_childs();

  // 別のノードを子ノードとする
  void link_to(Node *);
};

// ノードを管理するクラス
class NodeManager {
private:
  std::vector<Node *> nodes;
  // クラスターに割り当てるランク
  std::vector<Rank> cluster_ranks;
  bool already_build;

  // 依存関係に基づいてクラスタの再割り当てを行なう
  void split_cluster_by_dependence();

public:
  NodeManager();

  // マネージャにノードを登録する
  void register_node(Node *);
  // ノードの情報を元にグラフを構築する
  void build();

  const std::vector<Rank> &get_cluster_ranks();
};

// クラスタを管理するマネージャ
class ClusterManager {
private:
  // 現状のクラスタに割り当てるID
  static ID global_current_id;
  // 現在クラスタの中か否か
  // ネストしたときを想定して整数値で管理する
  static u64 current_clusters;

public:
  ID next_id();
  ID current_id();

  bool is_in_cluster();
  void enter_cluster();
  void exit_cluster();
};

extern ClusterManager clusterManager;

// クラスタの範囲を示すためのクラス
// このクラスのインスタンスが生成されてから消えるまでを一つのクラスタとする
class Cluster {
  Cluster();
  ~Cluster();
};

}; // namespace prf
