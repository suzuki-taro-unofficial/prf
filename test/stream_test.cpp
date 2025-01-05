#include "logger.hpp"
#include "prf.hpp"
#include "stream.hpp"
#include "thread.hpp"
#include <cassert>

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

int main() {
  test_1();

  prf::stop_execution();
}
