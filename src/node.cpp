#include "node.hpp"
#include "union_find.hpp"
#include "utils.hpp"
#include <algorithm>
#include <cassert>
#include <map>
#include <set>
#include <vector>

namespace prf {
// Node
Node::Node(ID cluster_id) : cluster_id(cluster_id) {}

ID Node::get_cluster_id() { return cluster_id; }
void Node::set_cluster_id(ID id) { cluster_id = id; };
Rank &Node::get_in_cluster_rank() { return in_cluster_rank; }

const std::vector<Node *> &Node::get_parents() { return parents; }
const std::vector<Node *> &Node::get_childs() { return childs; }

void Node::link_to(Node *other) {
  childs.push_back(other);
  other->parents.push_back(this);
}

// NodeManager
NodeManager::NodeManager() : nodes(), cluster_ranks(), already_build(false) {}

void NodeManager::register_node(Node *node) { nodes.push_back(node); }

void NodeManager::split_cluster_by_dependence() {
  std::map<Node *, u64> node2u64 = numbering(nodes);
  ID max_id = node2u64.size();

  UnionFind uf(max_id);

  for (Node *parent : nodes) {
    u64 parent_cluster_id = parent->get_cluster_id();
    for (Node *child : parent->get_childs()) {
      u64 child_cluster_id = child->get_cluster_id();
      // クラスタを明示的に切っているなら、そこでは依存が生まれない。
      if (parent_cluster_id != child_cluster_id) {
        continue;
      }

      uf.merge(node2u64[parent], node2u64[child]);
    }
  }

  std::vector<u64> unionfind_ids;
  for (auto i : node2u64) {
    unionfind_ids.push_back(i.second);
  }

  std::map<u64, u64> unionfind_id2cluster_id = numbering(unionfind_ids);

  for (Node *node : nodes) {
    u64 unionfind_id = uf.get_parent(node2u64[node]);
    u64 cluster_id = unionfind_id2cluster_id[unionfind_id];
    node->set_cluster_id(cluster_id);
  }
}

void NodeManager::generate_cluster_ranks() {
  ID max_id = 0;
  for (const auto node : nodes) {
    max_id = std::max(max_id, node->get_cluster_id());
  }

  cluster_ranks.assign(max_id + 1, Rank());

  std::vector<std::set<ID>> cluster_childs(max_id + 1);
  std::vector<std::set<ID>> cluster_parents(max_id + 1);

  for (const auto node : nodes) {
    const ID this_id = node->get_cluster_id();
    for (const auto parent : node->get_parents()) {
      const ID parent_id = parent->get_cluster_id();
      cluster_childs[parent_id].insert(this_id);
      cluster_parents[this_id].insert(parent_id);
    }
  }

  std::vector<ID> updates;

  for (ID i = 0; i < cluster_childs.size(); ++i) {
    if (cluster_parents[i].empty()) {
      updates.push_back(i);
    }
  }

  while (not updates.empty()) {
    const ID updating_id = updates.back();
    updates.pop_back();

    for (const ID child_id : cluster_childs[updating_id]) {
      cluster_ranks[updating_id].ensure_after(cluster_ranks[child_id]);
    }

    for (const ID child_id : cluster_childs[updating_id]) {
      cluster_parents[child_id].erase(updating_id);
      if (cluster_parents[child_id].empty()) {
        updates.push_back(child_id);
      }
    }
  }
}

void NodeManager::generate_in_cluster_ranks() {
  std::map<Node *, u64> node_to_u64 = numbering(nodes);
  std::map<u64, Node *> u64_to_node = transpose(node_to_u64);

  std::vector<std::set<u64>> parents(nodes.size());
  std::vector<std::set<u64>> childs(nodes.size());

  for (Node *parent : nodes) {
    for (Node *child : parent->get_childs()) {
      // クラスタIDが異なる場合は連結でないとみなす
      if (parent->get_cluster_id() != child->get_cluster_id()) {
        continue;
      }

      u64 parent_id = node_to_u64[parent];
      u64 child_id = node_to_u64[child];

      childs[parent_id].insert(child_id);
      parents[child_id].insert(parent_id);
    }
  }

  std::vector<u64> updates;
  for (Node *node : nodes) {
    u64 node_id = node_to_u64[node];
    if (parents[node_id].empty()) {
      updates.push_back(node_id);
    }
  }

  while (not updates.empty()) {
    u64 updating_id = updates.back();
    updates.pop_back();

    for (u64 child_id : childs[updating_id]) {
      Node *updating_node = u64_to_node[updating_id];
      Node *child_node = u64_to_node[child_id];
      updating_node->get_in_cluster_rank().ensure_after(
          child_node->get_in_cluster_rank());
    }

    for (u64 child_id : childs[updating_id]) {
      parents[child_id].erase(updating_id);
      if (parents[child_id].empty()) {
        updates.push_back(child_id);
      }
    }
  }
}

void NodeManager::build() {
  assert(not already_build && "ビルドは一度まで");
  already_build = true;

  split_cluster_by_dependence();
  generate_cluster_ranks();
  generate_in_cluster_ranks();
}

const std::vector<Rank> &NodeManager::get_cluster_ranks() {
  assert(already_build && "クラスタのランクを知るにはビルドをしてください");
  return cluster_ranks;
}
}; // namespace prf
