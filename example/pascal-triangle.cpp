#include "prf/cluster.hpp"
#include "prf/prf.hpp"
#include "prf/thread.hpp"
#include <cassert>
#include <chrono>
#include <ctime>
#include <iostream>
#include <prf/stream.hpp>
#include <string>
#include <vector>

const size_t HEIGHT = 10;

void heavy_calcurate() {
  std::chrono::system_clock::time_point start =
      std::chrono::system_clock::now();
  while (true) {
    std::chrono::system_clock::time_point current =
        std::chrono::system_clock::now();
    // 0.1秒経過するまでCPUを浪費させる
    auto dur =
        std::chrono::duration_cast<std::chrono::milliseconds>(current - start);
    if (dur.count() >= 100) {
      break;
    }
  }
}

int main(int argc, char **argv) {
  prf::StreamSink<int> root;
  std::vector<prf::Stream<int>> ss;

  ss.push_back(root.map([](int n) -> int {
    heavy_calcurate();
    return n;
  }));

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

  prf::use_parallel_execution = true;
  prf::build();

  clock_t start = clock();
  root.send(1);
  clock_t end = clock();
  assert((1 << (HEIGHT - 1)) == sum);

  // 実行時間を出力
  std::cout << (end - start) * 1.0 / CLOCKS_PER_SEC << std::endl;

  prf::stop_execution();
}
