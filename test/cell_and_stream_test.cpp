#include "cell.hpp"
#include "prf.hpp"
#include "stream.hpp"
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

int main() { run_test(test_1); }
