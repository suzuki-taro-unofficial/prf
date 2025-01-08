#include "cluster.hpp"
#include "prf.hpp"
#include "stream.hpp"
#include "transaction.hpp"
#include <atomic>
#include <chrono>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

// リソースの初期化をしてテストを実行後、バックグラウンドのスレッドを停止する
#define run_test(func)                                                         \
  do {                                                                         \
    prf::initialize();                                                         \
    info_log("テスト %s を実行します", #func);                                 \
    func();                                                                    \
    prf::stop_execution();                                                     \
  } while (false)

void test_1() {
  std::string sum = "";
  prf::StreamSink<int> s;
  std::mutex mtx;
  mtx.lock();
  s.map([&mtx](int n) -> char {
     mtx.lock();
     mtx.unlock();
     std::this_thread::sleep_for(std::chrono::milliseconds(10));
     return (char)n;
   }).listen([&sum](char c) -> void { sum += c; });

  prf::build();

  std::vector<prf::JoinHandler> handlers;

  {
    prf::Transaction trans1;
    s.send((int)'A');
    handlers.push_back(trans1.get_join_handler());

    prf::Transaction trans2;
    s.send((int)'B');
    handlers.push_back(trans2.get_join_handler());

    prf::Transaction trans3;
    s.send((int)'C');
    handlers.push_back(trans3.get_join_handler());
  }
  sum += "Z";
  mtx.unlock();
  // JoinHandlerのjoinを呼び出す順番は重要では無い
  for (auto handler = handlers.rbegin(); handler != handlers.rend();
       ++handler) {
    handler->join();
  }

  assert(sum == "ZABC" && "JoinHandlerが正しく動作する");
}

void test_2() {
  prf::StreamSink<int> s;
  prf::build();

  prf::Transaction trans;
  prf::JoinHandler handler1 = trans.get_join_handler();

  // JoinHandlerは std::move() を利用しないと移動できない
  prf::JoinHandler handler2 = std::move(handler1);

  std::vector<prf::JoinHandler> handlers;

  // vector等で扱いときも同様
  handlers.push_back(std::move(handler2));
}

void test_3() {
  std::atomic_int n(0);

  prf::StreamSink<int> s;
  int sum = 0;

  {
    prf::Cluster cluster;
    s.map([&n](int x) -> int {
       if (x == 1) {
         n.fetch_add(1);
         while (n.load() != 3) {
         }
       }
       return x;
     }).listen([&sum](int x) -> void { sum += x; });
  }

  {
    prf::Cluster cluster;
    s.map([&n](int x) -> int {
       if (x == 2) {
         n.fetch_add(1);
         while (n.load() != 3) {
         }
       }
       return x;
     }).listen([&sum](int x) -> void { sum += x; });
  }

  {
    prf::Cluster cluster;
    s.map([&n](int x) -> int {
       if (x == 3) {
         n.fetch_add(1);
         while (n.load() != 3) {
         }
       }
       return x;
     }).listen([&sum](int x) -> void { sum += x; });
  }

  prf::use_parallel_execution = true;
  prf::build();

  std::vector<prf::JoinHandler> handlers;

  {
    prf::Transaction trans;
    s.send(1);
    handlers.push_back(trans.get_join_handler());
  }
  {
    prf::Transaction trans;
    s.send(2);
    handlers.push_back(trans.get_join_handler());
  }
  {
    prf::Transaction trans;
    s.send(3);
    handlers.push_back(trans.get_join_handler());
  }

  for (auto &handler : handlers) {
    handler.join();
  }

  assert(sum == 18 && "並列に更新されている");

  prf::use_parallel_execution = false;
}

int main() {
  run_test(test_1);
  run_test(test_2);
  run_test(test_3);
}
