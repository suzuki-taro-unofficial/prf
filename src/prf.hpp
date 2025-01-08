#pragma once

#include "node.hpp"
#include "rank.hpp"
#include "types.hpp"

namespace prf {
/**
 * FRPの実行の準備を終える
 */
void build();

/**
 * 実行のために確保したリソースを実行前に戻す
 */
void initialize();

/**
 * 並列動作するか否か
 * build関数の実行前にセットしてください
 */
extern volatile bool use_parallel_execution;

} // namespace prf
