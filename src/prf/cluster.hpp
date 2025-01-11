#pragma once

#include "prf/types.hpp"

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

  /**
   * Sink系列が属するClusterのIDである
   * ユーザが手動で生成したTransactionは、このIDを参照することとする
   * このIDは割り当て前と後で値が変わらない
   */
  static const ID UNMANAGED_CLUSTER_ID;
};

extern ClusterManager clusterManager;

// クラスタの範囲を示すためのクラス
// このクラスのインスタンスが生成されてから消えるまでを一つのクラスタとする
class Cluster {
private:
  /**
   * 既にこのクラスタは終了しているか
   */
  bool closed;

public:
  /**
   * 本来とは違うタイミングでクラスターを終了する
   */
  void close();

  Cluster();
  ~Cluster();
};
} // namespace prf
