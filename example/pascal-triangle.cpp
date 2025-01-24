#include "prf/cluster.hpp"
#include "prf/prf.hpp"
#include "prf/thread.hpp"
#include <cassert>
#include <ctime>
#include <iostream>
#include <prf/stream.hpp>
#include <string>
#include <vector>

const size_t HEIGHT = 10;

void heavy_calcurate() {
  clock_t start = clock();
  while (true) {
    clock_t current = clock();
    // 0.1秒経過するまでCPUを浪費させる
    if ((current - start) * 1.0 / CLOCKS_PER_SEC > 0.1) {
      break;
    }
  }
}

int main(int argc, char **argv) {
  prf::StreamSink<int> root;
  std::vector<prf::Stream<int>> ss;

  ss.push_back(root);

  for (size_t i = 2; i <= HEIGHT; ++i) {
    std::vector<prf::Stream<int>> nexts;
    for (size_t j = 0; j < i; ++j) {
      if (j == 0) {
        prf::Cluster cluster;
        nexts.push_back(ss[0].map([](int n) -> int {
          heavy_calcurate();
          return n;
        }));
      } else if (j + 1 == i) {
        prf::Cluster cluster;
        nexts.push_back(ss[j - 1].map([](int n) -> int {
          heavy_calcurate();
          return n;
        }));
      } else {
        prf::Cluster cluster;
        nexts.push_back(ss[j - 1].merge(ss[j], [](int n, int m) -> int {
          heavy_calcurate();
          return n + m;
        }));
      }
    }
    ss = nexts;
  }

  int sum = 0;

  for (auto &s : ss) {
    s.listen([&sum](int n) -> void { sum += n; });
  }

  if (argc >= 2) {
    std::string argv0(argv[1]);
    if (argv0 == "yes") {
      // 並列実行モードにする
      prf::use_parallel_execution = true;
    }
  }

  prf::build();

  clock_t start = clock();
  root.send(1);
  clock_t end = clock();
  assert((1 << (HEIGHT - 1)) == sum);

  // 実行時間を出力
  std::cout << (end - start) * 1.0 / CLOCKS_PER_SEC << std::endl;

  prf::stop_execution();
}
