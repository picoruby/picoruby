/* picoruby-regexp: Regexp and MatchData on a Pike VM regex engine.
 *
 * The engine (lib/regex_engine.c, a Pike VM) compiles a pattern to a
 * position-independent uint32_t program and matches in O(input x
 * program) time with no recursion and no allocation. This file holds
 * what both VM bindings share: option letters, UTF-8 offset
 * conversion and the inspect/to_s spelling. The VM binding included
 * at the end supplies regex_malloc, regex_realloc and regex_free,
 * which the engine's compiler calls, and owns the objects.
 *
 * Offsets: the engine reports byte offsets; Ruby's MatchData#begin,
 * #end, Regexp#=~ and the pos argument of match are character
 * offsets. The bindings convert at the boundary with the two helpers
 * below, so a UTF-8 subject behaves as in CRuby.
 */
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <stdbool.h>

#include "regex_engine.h"

/* Ruby option bits (Regexp::IGNORECASE and friends) */
#define PICORB_RX_IGNORECASE 1
#define PICORB_RX_EXTENDED   2
#define PICORB_RX_MULTILINE  4

/* Option letters -> Ruby option bits. Returns -1 on a letter that is
 * not an option. 'x' is accepted and kept in the options, but the
 * engine has no extended mode: the pattern is taken as written. */
static int
picorb_rx_options_from_letters(const char *s, size_t len)
{
  int options = 0;
  for (size_t i = 0; i < len; i++) {
    switch (s[i]) {
    case 'i': options |= PICORB_RX_IGNORECASE; break;
    case 'x': options |= PICORB_RX_EXTENDED; break;
    case 'm': options |= PICORB_RX_MULTILINE; break;
    default: return -1;
    }
  }
  return options;
}

/* Ruby option bits -> engine flags. Ruby's m is the engine's DOTALL. */
static int
picorb_rx_engine_flags(int options)
{
  int flags = 0;
  if (options & PICORB_RX_IGNORECASE) flags |= (int)REGEX_ICASE;
  if (options & PICORB_RX_MULTILINE) flags |= (int)REGEX_DOTALL;
  return flags;
}

/* The letters of the options that are on, in CRuby's order "mix".
 * out holds at least 4 bytes; returns the length. */
static int
picorb_rx_letters(int options, char *out)
{
  int n = 0;
  if (options & PICORB_RX_MULTILINE) out[n++] = 'm';
  if (options & PICORB_RX_IGNORECASE) out[n++] = 'i';
  if (options & PICORB_RX_EXTENDED) out[n++] = 'x';
  out[n] = '\0';
  return n;
}

/* The prefix of Regexp#to_s: "(?mix:" with the options that are off
 * behind a '-', as CRuby spells it: "(?i-mx:". out holds at least 8
 * bytes; returns the length. */
static int
picorb_rx_to_s_prefix(int options, char *out)
{
  int n = 0;
  out[n++] = '(';
  out[n++] = '?';
  n += picorb_rx_letters(options, out + n);
  int off = (~options) & (PICORB_RX_MULTILINE | PICORB_RX_IGNORECASE | PICORB_RX_EXTENDED);
  if (off) {
    out[n++] = '-';
    n += picorb_rx_letters(off, out + n);
  }
  out[n++] = ':';
  out[n] = '\0';
  return n;
}

/* A UTF-8 continuation byte is 10xxxxxx. */
static bool
picorb_utf8_cont(uint8_t b)
{
  return (b & 0xC0) == 0x80;
}

/* Character offset -> byte offset. cpos == number of characters gives
 * len. Returns -1 when cpos is past the end. */
static long
picorb_utf8_char_to_byte(const uint8_t *s, size_t len, long cpos)
{
  if (cpos < 0) return -1;
  size_t b = 0;
  long c = 0;
  while (c < cpos && b < len) {
    b++;
    while (b < len && picorb_utf8_cont(s[b])) b++;
    c++;
  }
  if (c < cpos) return -1;
  return (long)b;
}

/* Byte offset -> character offset: the characters that start before
 * boff. boff is at a character boundary when it came from the engine. */
static long
picorb_utf8_byte_to_char(const uint8_t *s, size_t len, long boff)
{
  if (boff < 0) return -1;
  if ((size_t)boff > len) boff = (long)len;
  long c = 0;
  for (long b = 0; b < boff; b++) {
    if (!picorb_utf8_cont(s[b])) c++;
  }
  return c;
}

/* Number of characters in s */
static long
picorb_utf8_length(const uint8_t *s, size_t len)
{
  return picorb_utf8_byte_to_char(s, len, (long)len);
}

#if defined(PICORB_VM_MRUBY)
#include "mruby/regexp.c"
#elif defined(PICORB_VM_MRUBYC)
#include "mrubyc/regexp.c"
#endif
