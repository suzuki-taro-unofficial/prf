#pragma once

#include "rank.hpp"
#include "types.hpp"
#include <vector>

namespace prf {

// 依存関係を表現するオブジェクト
class Node {
private:
  // 所属しているクラスタのID
  // ビルド時にクラスタIDを再度割り当てる
  // 同じクラスタIDであっても連結成分でない場合は異なるクラスタIDになる
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
  Rank &get_in_cluster_rank();

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
  // クラスタにランクを割り当てる
  void generate_cluster_ranks();
  // クラスタ内のランクを割り当てる
  void generate_in_cluster_ranks();

public:
  NodeManager();

  // マネージャにノードを登録する
  void register_node(Node *);
  // ノードの情報を元にグラフを構築する
  void build();

  const std::vector<Rank> &get_cluster_ranks();
};
}; // namespace prf
