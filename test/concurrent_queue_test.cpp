#include "prf/concurrent_queue.hpp"
#include <cassert>
#include <thread>

void test_1() {
  prf::ConcurrentQueue<int> vs;

  int sum = 0;

  std::thread t1([&]() {
    vs.push(1);
    vs.push(2);
    vs.push(3);
  });

  std::thread t2([&]() {
    sum += *vs.pop();
    sum += *vs.pop();
    sum += *vs.pop();
  });

  t1.join();
  t2.join();

  assert(sum == 6 && "ConcurrentQueueで適切にデータの輸送ができてきる");
}

int main() { test_1(); }
