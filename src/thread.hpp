#pragma once

#include <atomic>

namespace prf {

/**
 * バックグラウンドで動いているスレッドを停止するか否か
 */
extern std::atomic_bool stop_the_threads;

/**
 * stop_the_threadsの値をtrueに変えてPlannerとExecutorを停止させる
 */
void stop_execution();
} // namespace prf
