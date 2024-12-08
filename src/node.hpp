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

  // このノードに依存しているノード
  std::vector<Node *> childs;

  // このノードにLoopで依存しているノード
  std::vector<Node *> loop_childs;
  // このノードと同一クラスタに属するノード
  // Loopなどの依存関係は無いが同一クラスタに属するべきものを入れる
  std::vector<Node *> same_clusters;

public:
  Node(ID);

  ID get_cluster_id();
  void set_cluster_id(ID);
  Rank &get_in_cluster_rank();

  const std::vector<Node *> &get_childs();
  const std::vector<Node *> &get_loop_childs();
  const std::vector<Node *> &get_same_clusters();

  // 別のノードを子ノードとする
  void link_to(Node *);

  // 別のノードをLoopでの子ノードとする
  void loop_child_to(Node *);
};

// ノードを管理するクラス
class NodeManager {
private:
  std::vector<Node *> nodes;
  // クラスターに割り当てるランク
  std::vector<Rank> cluster_ranks;
  bool already_build;

  // ノード間の関係性に基づいてクラスタの再割り当てを行なう
  void split_cluster_by_associates();
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
