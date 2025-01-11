#include "prf/time_invariant_values.hpp"
#include "prf/logger.hpp"
#include "prf/node.hpp"
#include "prf/transaction.hpp"

namespace prf {

void TimeInvariantValues::register_listeners_update(InnerTransaction *transaction) {
  for (auto tiv : this->listners) {
    transaction->register_update(tiv);
  }
}

void TimeInvariantValues::register_cleanup(InnerTransaction *transaction) {
  transaction->register_cleanup(this);
}

TimeInvariantValues::TimeInvariantValues(ID cluster_id)
    : node(new Node(cluster_id)) {
  NodeManager::globalNodeManager->register_node(this->node);
}

void TimeInvariantValues::update(InnerTransaction *transaction) {}

void TimeInvariantValues::refresh(ID transaction_id) {}

ID TimeInvariantValues::get_cluster_id() { return node->get_cluster_id(); }

void TimeInvariantValues::listen(TimeInvariantValues *to) {
  to->node->link_to(this->node);
  to->listners.push_back(this);
}

void TimeInvariantValues::listen_over_loop(TimeInvariantValues *to) {
  to->node->loop_child_to(this->node);
  to->listners.push_back(this);
}

void TimeInvariantValues::child_to(TimeInvariantValues *to) {
  to->node->link_to(this->node);
}

void TimeInvariantValues::global_listen(TimeInvariantValues *to) {
  to->listners.push_back(this);
}

void TimeInvariantValues::finalize(InnerTransaction *transaction) {}
} // namespace prf
