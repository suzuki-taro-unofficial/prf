#include "cell.hpp"
#include "prf.hpp"

// リソースの初期化をしてテストを実行後、バックグラウンドのスレッドを停止する
#define run_test(func)                                                         \
  do {                                                                         \
    prf::initialize();                                                         \
    info_log("テスト %s を実行します", #func);                                 \
    func();                                                                    \
    prf::stop_execution();                                                     \
  } while (false)

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

int main() {
  run_test(test_1);
  run_test(test_2);
}
