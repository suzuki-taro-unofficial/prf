#pragma once

#include "types.hpp"

namespace prf {
// クラスタを管理するマネージャ
class ClusterManager {
private:
  // 現状のクラスタに割り当てるID
  static ID global_current_id;
  // 現在クラスタの中か否か
  // ネストしたときを想定して整数値で管理する
  static u64 current_clusters;

  ID next_id();

public:
  ID current_id();

  bool is_in_cluster();
  void enter_cluster();
  void exit_cluster();
};

extern ClusterManager clusterManager;

// クラスタの範囲を示すためのクラス
// このクラスのインスタンスが生成されてから消えるまでを一つのクラスタとする
class Cluster {
  Cluster();
  ~Cluster();
};
} // namespace prf
