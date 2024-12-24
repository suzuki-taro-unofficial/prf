#include <stdio.h>

#define info_log(...) LOG_MACRO("INFO", __VA_ARGS__)
#define warn_log(...) LOG_MACRO("WARN", __VA_ARGS__)
#define error_log(...) LOG_MACRO("ERROR", __VA_ARGS__)
#define failure_log(...) LOG_AND_EXIT_MACRO("FAILURE", __VA_ARGS__)

/**
 * stderrにsprintfを経由してログを出すマクロ
 * C++17だとこういう用途に便利な方法が無いので残念だがマクロを使う
 */
#define LOG_MACRO(kind, ...)                                                   \
  {                                                                            \
    char buf[10000];                                                           \
    sprintf(buf, __VA_ARGS__);                                                 \
    fprintf(stderr, "%s:%s:%s:%d: %s\n", kind, __FILE__, __FUNCTION__,         \
            __LINE__, buf);                                                    \
  }

/**
 * ログを残してプログラムの実行を終了する
 */
#define LOG_AND_EXIT_MACRO(kind, ...)                                          \
  do {                                                                         \
    LOG_MACRO(kind, __VA_ARGS__);                                              \
    exit(1);                                                                   \
  } while (false)
