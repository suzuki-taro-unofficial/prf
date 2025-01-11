#include "prf/rank.hpp"

namespace prf {
Rank::Rank() : value(0) {}
Rank::Rank(u64 value) : value(value) {}

bool Rank::ensure_after(Rank &other) {
  if (*this < other) {
    return false;
  } else {
    other.value = value + 1;
    return true;
  }
}
bool operator<(const Rank &x, const Rank &y) { return x.value < y.value; }
bool operator>(const Rank &x, const Rank &y) { return x.value > y.value; }
bool operator==(const Rank &x, const Rank &y) { return x.value == y.value; }
} // namespace prf
