#include "prf.hpp"

// リソースの初期化をしてテストを実行後、バックグラウンドのスレッドを停止してフラグを初期化する
#define run_test(func)                                                         \
  do {                                                                         \
    prf::initialize();                                                         \
    info_log("テスト %s を実行します", #func);                                 \
    func();                                                                    \
    prf::stop_execution();                                                     \
    prf::use_parallel_execution = false;                                       \
  } while (false)
