#include "union_find.hpp"

namespace prf {
UnionFind::UnionFind(u64 size) : size(size), parents(size) {
  for (u64 i = 0; i < size; ++i) {
    parents[i] = i;
  }
}

u64 UnionFind::get_parent(u64 p) {
  if (parents[p] == p) {
    return p;
  } else {
    return parents[p] = get_parent(parents[p]);
  }
}

void UnionFind::merge(u64 p, u64 q) {
  u64 pp = get_parent(p);
  u64 qp = get_parent(q);
  if (pp == qp) {
    return;
  }
  parents[pp] = parents[qp];
}

bool UnionFind::is_same(u64 p, u64 q) { return get_parent(p) == get_parent(q); }

} // namespace prf
