#include <cstdlib>
#include <stdio.h>

#ifdef SHOW_INFO_LOG
#define info_log(...) LOG_MACRO("INFO", __VA_ARGS__)
#else
#define info_log(...)                                                          \
  do {                                                                         \
  } while (false)
#endif

#ifdef SHOW_WARN_LOG
#define warn_log(...) LOG_MACRO("WARN", __VA_ARGS__)
#else
#define warn_log(...)                                                          \
  do {                                                                         \
  } while (false)
#endif

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
    std::exit(1);                                                              \
  } while (false)
