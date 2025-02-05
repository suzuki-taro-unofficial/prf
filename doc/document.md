# PRF
このファイルはトランザクション間並列化されたFRPライブラリであるPRFのドキュメントです

具体的な使用例は `test` ディレクトリ以下にテストとして置いてあるので、そちらを参照してください。

## 用語
このファイル内では以下のような用語を使います

- 時変値:
  - 時間によって変わる値のこと
  - FRPにおけるStreamやCellを表す
- Sodium:
  - [sodium-cxx](https://github.com/SodiumFRP/sodium-cxx)のこと
  - 本文書では上記の実装との対比で説明します

## 仕様
> とりあえず清書はせずに箇条書きで書いてます

### 依存グラフのビルドについて
PRFでは、トランザクションを開始する前に明示的に依存グラフをビルドするという工程を挟む必要があります。
これはビルド処理以降には新しくグラフの形状に手を加えられないというPRFの都合から、ビルドされるタイミングを処理系とユーザーに認識させるためにあり、暗黙的にビルドされない理由になります。

ビルドするときは以下のようにします
```
prf::build();
```

ビルドをするときの注意点として以下のようなものがあります
 - ビルド実行前/中にトランザクションを発生させてはいけない
   - (Global)CellLoopの初期化がトランザクションを利用して行なわれるためです

### プログラムの終了について
恐らくPRFを適当に利用したプログラムは`main`関数の`return 0;`に到達しても終了できないです。
これは、PRFの裏でスレッドが動作しているからです。

スレッドを停止するためには `prf::stop_execution()` を呼び出すと停止できます。
この関数の注意点として、`prf::build()` を呼び出す前に使用してしまうと停止しなくなるので避けてください。

### 並列実行について
デフォルトではPRFは更新処理を逐次で行ないます。
もし、並列に更新処理をして欲しい場合は以下のようにしてください。

```
prf::use_parallel_execution = true; // buildより先にする必要あり
prf::build();
```

### クラスターについて
依存グラフの時変値をグループ分けする存在になります。

クラスターは、それを区切るためのクラスである `Cluster` の生存範囲で区切られることになります。
そして、時変値が生成される際は、現在のグローバルにあるクラスタがなんであるかを取得して、そこに所属するという仕組みです。

```
...
Stream A = ...;
...
// ↑ Cluster X
Cluster cluster1; // ここ以前と以降は別クラスタ
// ↓ Cluster Y
...
Stream B = A.map(f);
...
// ↑ Cluster Y
delete cluster2; // ここ以前と以降は別クラスタ
// ↓ Cluster Z
...
Stream C = B.map(g);
...
```

上記のようなコードを書いたとき、時変値`A`, `B`, `C`は別のクラスタに属することになり、それぞれクラスター`X`, `Y`, `Z`(仮称)に属しています。
> 時変値 `A` のようにClusterを明示的に呼び出す前でも暗黙的なクラスターに属しています

時変値の更新はクラスター単位で行なわれることになります。
例えば時変値 `B` が属するクラスター`Y`では、時変値Bについて以下のことを更新処理として行います。
- 時変値 `A` の現在の値を取得する
- 関数fに `A` の値を適用する
- 時変値 `B` の現在の値を適用した結果に更新する
以上のように、時変値の現在の値を決定するために行なう計算は、その時変値に属することを気をつけてください(内部の更新処理はPull BaseのFRPになります)

クラスターへの分割時の例外としてSink系列のものがあります。
StreamSinkやCellSinkがこれに該当しますが、これらの時変値はどのクラスターにも属することはありません。
これは、更新時の計算処理が重い時変値と同じクラスターに属した場合に、それらの計算を待たないと更新が終了しないという不便が発生するためです。

### トランザクションについて

#### トランザクションのブロッキング
トランザクションは、その生存期間中に行われたXXXSink系列の変更を纏めて、デストラクタの呼び出し時に更新を開始します。

また、トランザクションの更新処理をするスレッドの処理能力を越えて更新が発生するのを防ぐため、トランザクションの更新処理が終了するまではデストラクタの実行はブロッキングされます。

```
{
  Transaction trans1;
  A.send("HOGE")
}
↓のコードは↑のトランザクションの更新処理が終了するまで実行されない
{
  Transaction trans2;
  A.send("FUGA")
}
```

そのため、複数のトランザクションを発火させたい場合はスレッドに分けてください。

```
std::thread thread1([A]{
  Transaction trans1;
  A.send("HOGE")
});
std::thread thread2([A]{
  Transaction trans2;
  A.send("HOGE")
});

thread1.join();
thread2.join();
```

または、トランザクションに対して `get_join_handler()` を呼び出してください。

```
Transaction trans1;
A.send("HOGE")
JoinHandler handler1 = trans1.get_join_handler();


// get_join_handler()を呼び出した段階で現在のトランザクションは触れなくなる
// そのため、trans2はtrans1にネストしたように見えるが実際は別のトランザクションとなる
Transaction trans2;
A.send("FUGA")
JoinHandler handler2 = trans1.get_join_handler();


// 取得したHandlerは `join()` を呼び出すことで終了するまでブロッキングできる
// また、`join()` する順序は重要では無いので `handler2.join(); handler1.join();` でも正しく動作する
handler1.join();
handler2.join();
```

このときtrans1とtrans2がどの順序で実行されるのかを保証する必要がある場合は適宜condition_variableなどで順序の制御をしてください。
> 需要が高いならブロッキングしない & トランザクションの順序が自明なAPIも考えます

#### トランザクションの並列化について
> クラスターの更新順序についてルールがありますが、他の資料での説明を参照していただきたいです。
> 清書の段階で他の資料の内容と統合して書きます

クラスターという概念を追加したことにより、複数のトランザクションがクラスター単位で並列に更新されるようになります。
ただし、実際に並列実行されるか否かは保証しません。
並列に実行するか否かを判断するのは計算コストが高いため、ある程度の並列度合いが確保できた段階で、解析を中止した方が合理的な場合があるためです。
またその逆に、以下のようにFRPの意味論を守る範囲なら並列化は可能な限りされる場合もあります。

##### 完全に独立した依存グラフについて

以下のように時変値`A`,`B` と `C`,`D` が完全にグラフとして途切れているような場合は、それぞれが完全に独立して並列実行されます。
```
Cluster cluster1;
StreamSink A;
Cluster cluster2;
Stream B = A.map(f);
delete cluster2;
delete cluster1;

Cluster cluster1;
StreamSink C;
Cluster cluster2;
Stream D = C.map(f);
delete cluster2;
delete cluster1;


{
Transaction trans;
  // AとCの処理は完全に並列
  A.send("HOGE");
  C.send("FUGA");
}
```

#### listen処理について
FRPではFRPの中の値を外に取り出すためにlistenと呼ばれるものがあります。
```
StreamSink<int> A;
Stream B = A.map([](int x){ return x * x;});
B.listen([](int x) { std::cout << "Got Value: " << x << std::endl; });

A.sink(10); // "Got Value: 100" と出力される
```

これはトランザクションが終了時に事前に登録された関数を呼び出すことで実現されていますが、この呼び出し順序は必ずトランザクションの生成順序と一致します。

```
StreamSink<int> A;
A.listen([](int x){ std::cout << "Got " << x << std::endl; });
std::thread thread1([&](){ 
  Transaction trans1;
  A.send(1); 
});
std::thread thread2([&](){ 
  Transaction trans2;
  A.send(2); 
});
trans1の方がtrans2より先に実行されたなら出力が
"Got 1"
"Got 2"
逆なら
"Got 1"
"Got 2"
に必ずなる
```

これは、順序が意味論として重要な意味を持つFRPに置いて、その外への通信に対しても同じ性質を保つためです。

このルールは依存グラフが完全に独立しているような場合でも同様です。

### Loopについて
> 通常のLoopへ入ったクラスタ関連の仕様は別の資料を参照ください
#### GlobalCellLoopについて
GlobalCellLoopはCellLoopと違いクラスターを越えて利用できるように特殊な仕様を持っています。

仕様としては以下の通りです。
1. CellLoopと異なり、GlobalCellLoopは値の更新はされるが、それに起因して更新処理が開始されない
  - そのため、値を参照するためにはsnapshotやliftが必須である。
    - liftは扱いを間違えると、プログラムが動作しない状況を産み出すので、一般的な利用では snapshot してから hold する方が良いです。
  - それ以外を利用したときにはエラーが出るようにしたいが、C++の仕様上難しいことが分かったので妥協している。
2. 更新された値を参照できるのは、更新をしたトランザクションが終了した後に新たに発生したトランザクションだけである
  - 厳密には、GlobalCellLoopの更新をしたトランザクションの更新処理が終了した後、そのトランザクションのデストラクタが終了するまでのどこかで更新処理が登録される
3. GlobalCellLoopの値を参照した結果は、同一トランザクション内では同一であることが保証される

## その他
このライブラリは現在の実装では一度作成した時変値に割り当てられたメモリを開放する手段を提供していないです。
これはライブラリの用途上一度依存グラフを作成した後に再度作ることを想定していないため、これによってメモリリークが起きても良いだろうと考えているからです。
そのため、デバッグ目的で`-fsanitize=address`などをするとプログラムの終了時にエラーが出ます。
メモリを開放するように修正も可能ですが現在は他の方が優先順位が高いため放置してあります。
