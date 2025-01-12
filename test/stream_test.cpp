#include "prf/cluster.hpp"
#include "prf/logger.hpp"
#include "prf/prf.hpp"
#include "prf/stream.hpp"
#include "prf/transaction.hpp"
#include "test_utils.hpp"
#include <cassert>
#include <string>

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

void test_5() {
  prf::StreamSink<int> s1;
  prf::StreamLoop<int> s2;
  prf::Stream<int> s3 = s1.map([](int n) -> int { return n * n; });
  s2.loop(s3);
  prf::Stream<int> s4 = s2.map([](int n) -> int { return n + 1; });
  int sum = 0;
  s4.listen([&sum](int n) -> void { sum += n; });

  prf::build();

  s1.send(1);
  assert(sum == 2 && "StreamLoopが正しく動作している");

  s1.send(2);
  assert(sum == 7 && "StreamLoopが正しく動作している");

  s1.send(3);
  assert(sum == 17 && "StreamLoopが正しく動作している");
}

void test_6() {
  prf::StreamSink<int> s1;

  prf::Stream<int> s2 = s1.map([](int n) -> int { return n + 1; });
  prf::Stream<int> s3 = s1.map([](int n) -> int { return n * n; });

  prf::Stream<int> s4 = s2.merge(s3, [](int n, int m) -> int { return n + m; });
  int sum1 = 0;
  s4.listen([&sum1](int n) -> void { sum1 += n; });

  prf::Stream<int> s5 = s2.or_else(s3);
  int sum2 = 0;
  s5.listen([&sum2](int n) -> void { sum2 += n; });

  prf::build();

  s1.send(1);
  assert(sum1 == 3 && "両方が発火しているときにmergeが正しく動いている");
  assert(sum2 == 2 && "両方が発火しているときにorElseが正しく動いている");

  s1.send(2);
  assert(sum1 == 10 && "両方が発火しているときにmergeが正しく動いている");
  assert(sum2 == 5 && "両方が発火しているときにorElseが正しく動いている");

  s1.send(3);
  assert(sum1 == 23 && "両方が発火しているときにmergeが正しく動いている");
  assert(sum2 == 9 && "両方が発火しているときにorElseが正しく動いている");
}

void test_7() {
  prf::StreamSink<int> s1;
  prf::StreamSink<int> s2;

  prf::Stream<int> s3 = s1.merge(s2, [](int n, int m) -> int { return n + m; });
  int sum1 = 0;
  s3.listen([&sum1](int n) -> void { sum1 += n; });

  prf::Stream<int> s4 = s1.or_else(s2);
  int sum2 = 0;
  s4.listen([&sum2](int n) -> void { sum2 += n; });

  prf::build();

  {
    prf::Transaction trans;
    s1.send(10);
  }
  assert(sum1 == 10 && "片方が発火しているときにmergeが正しく動いている");
  assert(sum2 == 10 && "片方が発火しているときにorElseが正しく動いている");

  {
    prf::Transaction trans;
    s2.send(20);
  }
  assert(sum1 == 30 && "片方が発火しているときにmergeが正しく動いている");
  assert(sum2 == 30 && "片方が発火しているときにorElseが正しく動いている");

  {
    prf::Transaction trans;
    s1.send(10);
    s2.send(20);
  }
  assert(sum1 == 60 && "両方が発火しているときにmergeが正しく動いている");
  assert(sum2 == 40 && "両方が発火しているときにorElseが正しく動いている");
}

void test_8() {
  prf::StreamSink<int> s;

  int acc = 0;
  s.filter([](int n) -> bool {
     return n % 2 == 1;
   }).listen([&acc](int n) -> void { acc += n; });

  prf::build();

  s.send(1);
  assert(acc == 1 && "filterが正しく動作している");

  s.send(2);
  assert(acc == 1 && "filterが正しく動作している");

  s.send(3);
  assert(acc == 4 && "filterが正しく動作している");

  s.send(4);
  assert(acc == 4 && "filterが正しく動作している");

  s.send(5);
  assert(acc == 9 && "filterが正しく動作している");
}

void test_9() {
  prf::StreamSink<std::optional<int>> s;

  int sum = 0;

  prf::filterOptional(s).listen([&sum](int n) -> void { sum += n; });

  prf::build();

  s.send(std::nullopt);
  assert(sum == 0 && "filterOptionalが正しく動作している");

  s.send(1);
  assert(sum == 1 && "filterOptionalが正しく動作している");

  s.send(std::nullopt);
  assert(sum == 1 && "filterOptionalが正しく動作している");

  s.send(2);
  assert(sum == 3 && "filterOptionalが正しく動作している");

  s.send(std::nullopt);
  assert(sum == 3 && "filterOptionalが正しく動作している");

  s.send(3);
  assert(sum == 6 && "filterOptionalが正しく動作している");
}

void test_10() {
  prf::StreamSink<std::string> s1;
  prf::StreamSink<float> s2;

  prf::Cluster cluster;

  prf::Stream<bool> s3 = s1.map_to(true).or_else(s2.map_to(false));

  int sum = 0;

  s3.listen([&sum](bool x) -> void {
    if (x) {
      sum += 1;
    }
  });

  prf::build();

  {
    prf::Transaction trans;
    s1.send("HOGE");
  }
  assert(sum == 1 && "map_toとor_elseが正しく動作している");
  {
    prf::Transaction trans;
    s2.send(1.2);
  }
  assert(sum == 1 && "map_toとor_elseが正しく動作している");
  {
    prf::Transaction trans;
    s1.send("HOGE");
    s2.send(1.2);
  }
  assert(sum == 2 && "map_toとor_elseが正しく動作している");
}

int main() {
  run_test(test_1);
  run_test(test_2);
  run_test(test_3);
  run_test(test_4);
  run_test(test_5);
  run_test(test_6);
  run_test(test_7);
  run_test(test_8);
  run_test(test_9);
  run_test(test_10);
}
