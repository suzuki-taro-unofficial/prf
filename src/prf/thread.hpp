#pragma once

#include <atomic>

namespace prf {
// バックグラウンドにあるスレッドを停止するための機能を列挙する場所

/**
 * バックグラウンドで動いているスレッドを停止するか否か
 */
extern std::atomic_bool stop_the_threads;

/**
 * 停止待ちしているスレッドの数
 */
extern std::atomic_int8_t wait_threads;

/**
 * stop_the_threadsの値をtrueに変えてPlannerとExecutorを停止させる
 * PlannerとExecutorが既に停止しているときに呼び出すと、この関数は永久にブロックするので注意
 */
void stop_execution();
} // namespace prf
