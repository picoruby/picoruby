#include <stdio.h>
#include <stdint.h>

#define LOGLEVEL_FATAL 0
#define LOGLEVEL_ERROR 1
#define LOGLEVEL_WARN  2
#define LOGLEVEL_INFO  3
#define LOGLEVEL_DEBUG 4

/* GLOBAL */
int loglevel;

#ifdef DEBUG_BUILD

  #define DEBUG(fmt, ...)                 \
    do {                                  \
      if (loglevel >= LOGLEVEL_DEBUG) {   \
        printf("\033[34;1m[DEBUG] ");     \
        DEBUG_PRINTF(fmt, ##__VA_ARGS__); \
        printf("\033[m\n");                 \
      }                                   \
    } while (0)
  #define INFO(fmt, ...)                  \
    do {                                  \
      if (loglevel >= LOGLEVEL_INFO) {    \
        printf("[INFO]  ");      \
        DEBUG_PRINTF(fmt, ##__VA_ARGS__); \
        printf("\033[m\n");                 \
      }                                   \
    } while (0)
  #define WARN(fmt, ...)                  \
    do {                                  \
      if (loglevel >= LOGLEVEL_WARN) {    \
        printf("\033[35;1m[WARN]  ");     \
        DEBUG_PRINTF(fmt, ##__VA_ARGS__); \
        printf("\033[m\n");                 \
      }                                   \
    } while (0)
  #define ERROR(fmt, ...)                 \
    do {                                  \
      if (loglevel >= LOGLEVEL_ERROR) {   \
        printf("\033[31;1m[ERROR] ");     \
        DEBUG_PRINTF(fmt, ##__VA_ARGS__); \
        printf("\033[m\n");                 \
      }                                   \
    } while (0)
  #define FATAL(fmt, ...)                 \
    do {                                  \
      if (loglevel >= LOGLEVEL_FATAL) {   \
        printf("\033[37;41;1m[FATAL] ");     \
        DEBUG_PRINTF(fmt, ##__VA_ARGS__); \
        printf("\033[m\n");                 \
      }                                   \
    } while (0)
  #define DEBUG_PRINTF(fmt, ...)                  \
    do {                                          \
      printf("file : %s, line : %d, func : %s,\n  ", \
            __FILE__,  __LINE__,  __func__);      \
      printf(fmt, ##__VA_ARGS__);                 \
    } while (0)
#else
  #define DEBUG(fmt, ...) /* omit */
  #define INFO(fmt, ...)  /* omit */
  #define WARN(fmt, ...)  (fprintf(stdout, "[WARN]  "),        \
                           fprintf(stdout, fmt, ##__VA_ARGS__),\
                           fprintf(stdout, "\n"))
  #define ERROR(fmt, ...) (fprintf(stderr, "[ERROR] "),        \
                           fprintf(stderr, fmt, ##__VA_ARGS__),\
                           fprintf(stderr, "\n"))
  #define FATAL(fmt, ...) (fprintf(stderr, "[FATAL] "),        \
                           fprintf(stderr, fmt, ##__VA_ARGS__),\
                           fprintf(stderr, "\n"))
#endif /* DEBUG_BUILD */
