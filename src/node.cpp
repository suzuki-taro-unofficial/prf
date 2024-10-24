#include "node.hpp"
#include "union_find.hpp"
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
Rank Node::get_in_cluster_rank() { return in_cluster_rank; }

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
  std::map<Node *, u64> node2u64;

  for (Node *node : nodes) {
    node2u64[node] = 0;
  }
  u64 id = 0;
  for (auto &i : node2u64) {
    i.second = id;
    ++id;
  }

  UnionFind uf(id);

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

  std::map<u64, u64> unionfind_id2cluster_id;

  for (Node *node : nodes) {
    u64 unionfind_id = uf.get_parent(node2u64[node]);
    unionfind_id2cluster_id[unionfind_id] = 0;
  }

  {
    u64 cluster_id = 0;
    for (auto &i : unionfind_id2cluster_id) {
      i.second = cluster_id;
      ++cluster_id;
    }
  }

  for (Node *node : nodes) {
    u64 unionfind_id = uf.get_parent(node2u64[node]);
    u64 cluster_id = unionfind_id2cluster_id[unionfind_id];
    node->set_cluster_id(cluster_id);
  }
}

void NodeManager::build() {
  assert(not already_build && "ビルドは一度まで");
  already_build = true;

  split_cluster_by_dependence();

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

const std::vector<Rank> &NodeManager::get_cluster_ranks() {
  assert(already_build && "クラスタのランクを知るにはビルドをしてください");
  return cluster_ranks;
}
}; // namespace prf
