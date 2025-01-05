#include "time_invariant_values.hpp"
#include "node.hpp"
#include "transaction.hpp"

namespace prf {

void TimeInvariantValues::register_listeners_update(Transaction *transaction) {
  for (auto tiv : this->listners) {
    transaction->register_update(tiv);
  }
}

void TimeInvariantValues::register_cleanup(Transaction *transaction) {
  transaction->register_cleanup(this);
}

TimeInvariantValues::TimeInvariantValues(ID cluster_id)
    : node(new Node(cluster_id)) {
  globalNodeManager.register_node(this->node);
}

void TimeInvariantValues::update(Transaction *transaction) {}

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
} // namespace prf
