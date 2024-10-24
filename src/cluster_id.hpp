#pragma once

#include "types.hpp"

namespace prf {
class ClusterId {

  ClusterId(u64 value) : value(value) {}

public:
  u64 value;
};
}; // namespace prf
