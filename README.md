# prf
FRPを並列化します。

CMakeでビルドされるライブラリとして提供する予定です。

# ライブラリの動作確認をしたいとき
`Makefile` に動作確認のためのコマンドを列挙してるのでそれを使ってください

# このライブラリを参照するとき
1. CMakeLists.txtのadd_subdirectoryにこのプロジェクトルートを追加する
2. ROOT/src/CMakeLists.txtから適当に利用したいライブラリの名前を指定して利用する
