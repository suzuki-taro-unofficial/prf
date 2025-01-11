#include "cassert"
#include "prf/rank.hpp"

void ensure_after_test1() {
  prf::Rank rank1;
  prf::Rank rank2;

  rank1.value = 1;
  rank2.value = 2;

  assert(not rank1.ensure_after(rank2) &&
         "すでにランクが昇順である場合は更新を行なわない");
  // 値が変わってない
  assert(rank2.value == 2 && "falseを返したときは値が変わらない");
}

void ensure_after_test2() {

  prf::Rank rank1;
  prf::Rank rank2;

  rank1.value = 2;
  rank2.value = 2;

  assert(rank1.ensure_after(rank2) && "ランクが同じである場合は更新を行う");
  assert(rank1.value < rank2.value && "ランクが昇順に更新されている");
}

void ensure_after_test3() {
  prf::Rank rank1;
  prf::Rank rank2;

  rank1.value = 2;
  rank2.value = 1;

  assert(rank1.ensure_after(rank2) && "ランクが降順である場合は更新を行う");
  assert(rank1.value < rank2.value && "ランクが昇順に更新されている");
}

int main() {
  ensure_after_test1();
  ensure_after_test2();
  ensure_after_test3();
  return 0;
}
