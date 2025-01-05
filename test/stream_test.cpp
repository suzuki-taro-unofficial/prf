#include "cluster.hpp"
#include "logger.hpp"
#include "prf.hpp"
#include "stream.hpp"
#include "thread.hpp"
#include <cassert>

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

  prf::Stream<int> s2 = s1.map(
      (std::function<int(std::shared_ptr<int>)>)[](std::shared_ptr<int> x)

          ->int { return *x * *x; });

  int sum = 0;

  s2.listen(
      (std::function<void(std::shared_ptr<int>)>)[&sum](std::shared_ptr<int> x)
          ->void { sum += *x; });

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

  prf::Stream<int> s2 = s1.map(
      (std::function<int(std::shared_ptr<int>)>)[](std::shared_ptr<int> x)

          ->int { return *x + 1; });

  prf::Stream<int> s3 = s2.map(
      (std::function<int(std::shared_ptr<int>)>)[](std::shared_ptr<int> x)

          ->int { return *x + 1; });

  prf::Stream<int> s4 = s3.map(
      (std::function<int(std::shared_ptr<int>)>)[](std::shared_ptr<int> x)

          ->int { return *x + 1; });

  int sum = 0;

  s4.listen(
      (std::function<void(std::shared_ptr<int>)>)[&sum](std::shared_ptr<int> x)
          ->void { sum += *x; });

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

  prf::Stream<int> s2 = s1.map(
      (std::function<int(std::shared_ptr<int>)>)[](std::shared_ptr<int> x)

          ->int { return *x + 1; });

  prf::Cluster cluster2;

  prf::Stream<int> s3 = s2.map(
      (std::function<int(std::shared_ptr<int>)>)[](std::shared_ptr<int> x)

          ->int { return *x + 1; });

  prf::Cluster cluster3;

  prf::Stream<int> s4 = s3.map(
      (std::function<int(std::shared_ptr<int>)>)[](std::shared_ptr<int> x)

          ->int { return *x + 1; });

  int sum = 0;

  s4.listen(
      (std::function<void(std::shared_ptr<int>)>)[&sum](std::shared_ptr<int> x)
          ->void { sum += *x; });

  prf::build();

  s1.send(1);
  assert(sum == 4 && "複数クラスタに跨ってもStreamSinkとmapが適切に動く");
  s1.send(2);
  assert(sum == 9 && "複数クラスタに跨ってもStreamSinkとmapが適切に動く");
  s1.send(3);
  assert(sum == 15 && "複数クラスタに跨ってもStreamSinkとmapが適切に動く");
}

int main() {
  run_test(test_1);
  run_test(test_2);
  run_test(test_3);
}
