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

} // namespace prf
