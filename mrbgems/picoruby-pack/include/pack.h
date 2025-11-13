#ifndef PACK_DEFINED_H_
#define PACK_DEFINED_H_

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Pack format directives */
#define PACK_DIR_C      'C'  /* Unsigned 8-bit integer */
#define PACK_DIR_c      'c'  /* Signed 8-bit integer */
#define PACK_DIR_S      'S'  /* Unsigned 16-bit integer, native endian */
#define PACK_DIR_s      's'  /* Signed 16-bit integer, native endian */
#define PACK_DIR_L      'L'  /* Unsigned 32-bit integer, native endian */
#define PACK_DIR_l      'l'  /* Signed 32-bit integer, native endian */
#define PACK_DIR_n      'n'  /* Unsigned 16-bit integer, network (big-endian) byte order */
#define PACK_DIR_N      'N'  /* Unsigned 32-bit integer, network (big-endian) byte order */
#define PACK_DIR_v      'v'  /* Unsigned 16-bit integer, VAX (little-endian) byte order */
#define PACK_DIR_V      'V'  /* Unsigned 32-bit integer, VAX (little-endian) byte order */
#define PACK_DIR_A      'A'  /* ASCII string (space padded) */
#define PACK_DIR_a      'a'  /* Binary string (null padded) */
#define PACK_DIR_STAR   '*'  /* Use all remaining elements */

/* Error codes */
typedef enum {
  PACK_SUCCESS = 0,
  PACK_ERROR_INVALID_FORMAT = -1,
  PACK_ERROR_INSUFFICIENT_ARGS = -2,
  PACK_ERROR_MEMORY = -3,
  PACK_ERROR_TYPE = -4,
} pack_error_t;

#ifdef __cplusplus
}
#endif

#endif /* PACK_DEFINED_H_ */
