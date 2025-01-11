#include "prf/cell.hpp"
#include "prf/cluster.hpp"
#include "prf/prf.hpp"
#include "prf/stream.hpp"
#include "string"
#include "test_utils.hpp"
#include "prf/transaction.hpp"
#include <cassert>

void test_1() {
  prf::CellSink<int> c1(3);
  prf::Cell<int> c2 = c1.map([](int n) -> int { return n * n; });

  int sum = 0;
  c2.listen([&sum](int n) -> void { sum += n; });

  prf::build();

  assert(sum == 9 && "Cellの初期化が正しく行なわれた");
  c1.send(1);
  assert(sum == 10 && "CellSinkとmapが正しく動作した");
  c1.send(2);
  assert(sum == 14 && "CellSinkとmapが正しく動作した");
  c1.send(3);
  assert(sum == 23 && "CellSinkとmapが正しく動作した");
}
void test_2() {
  prf::CellSink<int> c1(10);
  prf::CellLoop<int> c2;
  prf::Cell<int> c3 = c1.map([](int n) -> int { return n * n; });
  c2.loop(c3);
  prf::Cell<int> c4 = c2.map([](int n) -> int { return n + 1; });

  int sum = 0;
  c4.listen([&sum](int n) -> void { sum += n; });

  prf::build();
  assert(sum == 101 && "CellLoopが正しく動作している");

  c1.send(1);
  assert(sum == 103 && "CellLoopが正しく動作している");

  c1.send(2);
  assert(sum == 108 && "CellLoopが正しく動作している");

  c1.send(3);
  assert(sum == 118 && "CellLoopが正しく動作している");
}

void test_3() {
  prf::CellSink<int> c1(0);
  prf::CellSink<std::string> c2("ABC");
  prf::Cell<char> c3 = c1.lift(
      c2, [](int index, std::string str) -> char { return str[index]; });

  std::string acc = "";
  c3.listen([&acc](char c) -> void { acc += c; });

  prf::build();
  assert(acc == "A" && "liftプリミティブが正しく動作している");

  c1.send(2);
  assert(acc == "AC" && "liftプリミティブが正しく動作している");

  c2.send("XYZ");
  assert(acc == "ACZ" && "liftプリミティブが正しく動作している");

  c2.send("QWERTY");
  assert(acc == "ACZE" && "liftプリミティブが正しく動作している");

  c1.send(4);
  assert(acc == "ACZET" && "liftプリミティブが正しく動作している");

  {
    prf::Transaction trans;
    c1.send(2);
    c2.send("DVORAK");
  }
  assert(acc == "ACZETO" && "liftプリミティブが正しく動作している");
}

void test_4() {
  prf::GlobalCellLoop<int> cg;

  // GlobalCellLoopはクラスターを越えても動く
  prf::Cluster *cluster = new prf::Cluster();
  prf::StreamSink<int> s1;
  prf::Stream<int> s2 =
      s1.snapshot(cg, [](int n, int m) -> int { return n + m; });
  cg.loop(s2.hold(0));
  delete cluster;

  prf::build();

  int sum = 0;
  s2.listen([&sum](int n) -> void { sum += n; });

  s1.send(1);
  assert(sum == 1 && "GlobalCellLoopが正しく動作している");

  s1.send(2);
  assert(sum == 4 && "GlobalCellLoopが正しく動作している");

  s1.send(3);
  assert(sum == 10 && "GlobalCellLoopが正しく動作している");
}

int main() {
  run_test(test_1);
  run_test(test_2);
  run_test(test_3);
  run_test(test_4);
}
