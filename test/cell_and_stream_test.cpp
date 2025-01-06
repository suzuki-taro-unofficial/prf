#include "cell.hpp"
#include "prf.hpp"
#include "stream.hpp"
#include "transaction.hpp"
#include <cassert>
#include <string>

// リソースの初期化をしてテストを実行後、バックグラウンドのスレッドを停止する
#define run_test(func)                                                         \
  do {                                                                         \
    prf::initialize();                                                         \
    info_log("テスト %s を実行します", #func);                                 \
    func();                                                                    \
    prf::stop_execution();                                                     \
  } while (false)

void test_1() {
  prf::StreamSink<int> s;
  prf::Cell<std::string> c =
      s.map([](int n) -> int { return n * n; })
          .map([](int n) -> std::string { return std::to_string(n); })
          .hold("START!");

  std::string acc = "";
  c.listen([&acc](std::string s) -> void { acc += s; });

  prf::build();
  assert(acc == "START!" && "holdプリミティブが正しく動作している");

  s.send(1);
  assert(acc == "START!1" && "holdプリミティブが正しく動作している");

  s.send(2);
  assert(acc == "START!14" && "holdプリミティブが正しく動作している");

  s.send(3);
  assert(acc == "START!149" && "holdプリミティブが正しく動作している");

  s.send(4);
  assert(acc == "START!14916" && "holdプリミティブが正しく動作している");
}

void test_2() {
  prf::StreamSink<int> s1;
  prf::CellSink<std::string> c1("ABC");
  prf::Stream<char> s2 = s1.snapshot(
      c1, [](int index, std::string s) -> char { return s[index]; });

  std::string acc = "";
  s2.listen([&acc](char c) -> void { acc += c; });

  prf::build();

  s1.send(1);
  assert(acc == "B" && "snapshotプリミティブが正しく動作している");
  s1.send(2);
  assert(acc == "BC" && "snapshotプリミティブが正しく動作している");
  s1.send(0);
  assert(acc == "BCA" && "snapshotプリミティブが正しく動作している");

  c1.send("XYZ");

  s1.send(0);
  assert(acc == "BCAX" && "snapshotプリミティブが正しく動作している");
  s1.send(1);
  assert(acc == "BCAXY" && "snapshotプリミティブが正しく動作している");
  s1.send(2);
  assert(acc == "BCAXYZ" && "snapshotプリミティブが正しく動作している");

  {
    prf::Transaction trans;
    s1.send(3);
    c1.send("QWERTY");
  }
  assert(acc == "BCAXYZR" && "snapshotプリミティブが正しく動作している");
}

void test_3() {
  prf::StreamSink<int> s;
  prf::CellSink<bool> c(true);

  int sum = 0;
  s.gate(c).listen([&sum](int n) -> void { sum += n; });

  prf::build();

  s.send(1);
  assert(sum == 1 && "gateが正しく動作している");

  s.send(2);
  assert(sum == 3 && "gateが正しく動作している");

  c.send(false);
  s.send(3);
  assert(sum == 3 && "gateが正しく動作している");

  c.send(true);
  s.send(4);
  assert(sum == 7 && "gateが正しく動作している");

  {
    prf::Transaction trans;
    c.send(false);
    s.send(5);
  }
  assert(sum == 7 && "gateが正しく動作している");

  {
    prf::Transaction trans;
    c.send(true);
    s.send(6);
  }
  assert(sum == 13 && "gateが正しく動作している");
}

void test_4() {
  prf::StreamSink<int> s1;
  prf::CellLoop<int> acc;
  prf::Stream<int> s2 =
      s1.snapshot(acc, [](int a, int b) -> int { return a + b; });
  acc.loop(s2.hold(0));

  int sum = 0;
  s2.listen([&sum](int n) -> void { sum += n; });

  prf::build();

  s1.send(1);
  assert(sum == 1 && "CellLoopが正しく動作している");

  s1.send(2);
  assert(sum == 4 && "CellLoopが正しく動作している");

  s1.send(3);
  assert(sum == 10 && "CellLoopが正しく動作している");

  s1.send(4);
  assert(sum == 20 && "CellLoopが正しく動作している");
}

int main() {
  run_test(test_1);
  run_test(test_2);
  run_test(test_3);
  run_test(test_4);
}
