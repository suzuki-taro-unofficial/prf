#pragma once

#include "types.hpp"
#include <vector>

namespace prf {
class UnionFind {
private:
  std::vector<u64> parents;

public:
  const u64 size;

  UnionFind(u64);

  u64 get_parent(u64);
  void merge(u64, u64);
  bool is_same(u64, u64);
};
} // namespace prf
