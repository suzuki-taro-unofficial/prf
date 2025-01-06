#include "cluster.hpp"
#include "logger.hpp"
#include "prf.hpp"
#include "stream.hpp"
#include "thread.hpp"
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
  prf::StreamSink<int> s1;

  auto s2 = s1.map([](int x) -> int { return x * x; });

  int sum = 0;

  s2.listen([&sum](int x) -> void { sum += x; });

  prf::build();

  s1.send(1);
  assert(sum == 1 && "StreamSinkとmapが適切に動く");
  s1.send(2);
  assert(sum == 5 && "StreamSinkとmapが適切に動く");
  s1.send(3);
  assert(sum == 14 && "StreamSinkとmapが適切に動く");
}

void test_2() {
  prf::StreamSink<int> s1;

  prf::Stream<int> s2 = s1.map([](int x) -> int { return x + 1; });

  prf::Stream<int> s3 = s2.map([](int x) -> int { return x + 1; });

  prf::Stream<int> s4 = s3.map([](int x) -> int { return x + 1; });

  int sum = 0;

  s4.listen([&sum](int x) -> void { sum += x; });

  prf::build();

  s1.send(1);
  assert(sum == 4 && "単一クラウタの中でStreamSinkとmapが適切に動く");
  s1.send(2);
  assert(sum == 9 && "単一クラウタの中でStreamSinkとmapが適切に動く");
  s1.send(3);
  assert(sum == 15 && "単一クラウタの中でStreamSinkとmapが適切に動く");
}

void test_3() {
  prf::StreamSink<int> s1;

  prf::Cluster cluster1;

  prf::Stream<int> s2 = s1.map([](int x) -> int { return x + 1; });

  prf::Cluster cluster2;

  prf::Stream<int> s3 = s2.map([](int x) -> int { return x + 1; });

  prf::Cluster cluster3;

  prf::Stream<int> s4 = s3.map([](int x) -> int { return x + 1; });

  int sum = 0;

  s4.listen([&sum](int x) -> void { sum += x; });

  prf::build();

  s1.send(1);
  assert(sum == 4 && "複数クラスタに跨ってもStreamSinkとmapが適切に動く");
  s1.send(2);
  assert(sum == 9 && "複数クラスタに跨ってもStreamSinkとmapが適切に動く");
  s1.send(3);
  assert(sum == 15 && "複数クラスタに跨ってもStreamSinkとmapが適切に動く");
}

void test_4() {
  prf::StreamSink<int> s;

  prf::Stream<std::string> s1 =
      s.map([](int x) -> std::string { return std::to_string(x); });
  prf::Stream<std::string> s2 =
      s.map([](const int x) -> std::string { return std::to_string(x); });
  prf::Stream<std::string> s3 =
      s.map([](const int &x) -> std::string { return std::to_string(x); });
  prf::Stream<std::string> s4 =
      s.map([](int &x) -> std::string { return std::to_string(x); });

  std::string r1 = "";
  std::string r2 = "";
  std::string r3 = "";
  std::string r4 = "";

  s1.listen([&r1](std::string x) { r1 += x; });
  s1.listen([&r2](std::string x) { r2 += x; });
  s1.listen([&r3](std::string x) { r3 += x; });
  s1.listen([&r4](std::string x) { r4 += x; });

  prf::build();

  s.send(1);
  s.send(2);
  s.send(4);
  s.send(8);
  s.send(16);

  assert(r1 == "124816" && "mapで引数をTで受け取れる");
  assert(r2 == "124816" && "mapで引数をconst Tで受け取れる");
  assert(r3 == "124816" && "mapで引数をconst T&で受け取れる");
  assert(r4 == "124816" && "mapで引数をT&で受け取れる");
}

int main() {
  run_test(test_1);
  run_test(test_2);
  run_test(test_3);
  run_test(test_4);
}
