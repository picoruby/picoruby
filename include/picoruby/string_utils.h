#ifndef PICORUBY_STRING_UTILS_H
#define PICORUBY_STRING_UTILS_H

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Check if a string is valid UTF-8
 * @param str String to validate (must be null-terminated)
 * @return true if valid UTF-8, false otherwise
 */
static inline bool
picorb_is_valid_utf8(const unsigned char *str)
{
  while (*str) {
    if (*str < 0x80) {
      /* ASCII */
      if (*str < 0x20 && *str != 0x0A && *str != 0x0D && *str != 0x09) {
        return false;
      }
      str++;
    }
    else if ((*str & 0xE0) == 0xC0) {
      /* 2-byte character */
      if (!(str[1] && (str[1] & 0xC0) == 0x80)) return false;
      str += 2;
    }
    else if ((*str & 0xF0) == 0xE0) {
      /* 3-byte character */
      if (!(str[1] && str[2] &&
            (str[1] & 0xC0) == 0x80 &&
            (str[2] & 0xC0) == 0x80)) return false;
      str += 3;
    }
    else if ((*str & 0xF8) == 0xF0) {
      /* 4-byte character */
      if (!(str[1] && str[2] && str[3] &&
            (str[1] & 0xC0) == 0x80 &&
            (str[2] & 0xC0) == 0x80 &&
            (str[3] & 0xC0) == 0x80)) return false;
      str += 4;
    }
    else {
      return false;
    }
  }
  return true;
}

#ifdef __cplusplus
}
#endif

#endif /* PICORUBY_STRING_UTILS_H */
