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

/* Bytes of the character that starts at s[i]: at least 1, never past len */
static size_t
picorb_utf8_charlen(const uint8_t *s, size_t len, size_t i)
{
  size_t n = 1;
  while (i + n < len && picorb_utf8_cont(s[i + n])) n++;
  return n;
}

/* ---- the loops behind sub, gsub, scan and split ----
 * The VM bindings cannot share String or Array construction, so the
 * loops below take a callback: emit appends bytes to the result the
 * binding is building, on_match hands over one match, on_piece one
 * byte range. What the loops share is the part that is easy to get
 * wrong: where the next search starts after an empty match, and how
 * pieces and limits fall out of split. */

typedef void (*picorb_rx_emit)(void *ctx, const uint8_t *p, size_t n);
typedef void (*picorb_rx_on_match)(void *ctx, const int32_t *caps, int nsave);
typedef void (*picorb_rx_on_piece)(void *ctx, size_t b, size_t e);

/* RX_MAX_GROUPS in the engine is 16, so nsave is at most 32 */
#define PICORB_RX_MAX_NSAVE 32

/* Regexp.escape: a backslash before every byte the engine reads as
 * syntax, and \n \t \r \f \v spelled out. A space stays a space. */
static void
picorb_rx_escape(const uint8_t *s, size_t len, picorb_rx_emit emit, void *ctx)
{
  static const char syntax[] = ".*+?()[]{}|^$\\/#-";
  for (size_t i = 0; i < len; i++) {
    uint8_t c = s[i];
    const char *spelled = NULL;
    switch (c) {
    case '\n': spelled = "\\n"; break;
    case '\t': spelled = "\\t"; break;
    case '\r': spelled = "\\r"; break;
    case '\f': spelled = "\\f"; break;
    case '\v': spelled = "\\v"; break;
    default: break;
    }
    if (spelled) {
      emit(ctx, (const uint8_t *)spelled, 2);
    } else if (c && strchr(syntax, (char)c)) {
      emit(ctx, (const uint8_t *)"\\", 1);
      emit(ctx, s + i, 1);
    } else {
      emit(ctx, s + i, 1);
    }
  }
}

/* Expand a replacement template against one match: \0 and \& are
 * the whole match, \1..\9 a group (nothing when it did not take part
 * or does not exist), \\ a backslash. Any other \x stays as written. */
static void
picorb_rx_expand(const uint8_t *repl, size_t rlen, const uint8_t *s,
                 const int32_t *caps, int nsave, picorb_rx_emit emit, void *ctx)
{
  size_t run = 0;
  for (size_t i = 0; i < rlen; i++) {
    if (repl[i] != '\\' || i + 1 >= rlen) continue;
    uint8_t c = repl[i + 1];
    int group = -1;
    if (c == '0' || c == '&') group = 0;
    else if ('1' <= c && c <= '9') group = c - '0';
    else if (c != '\\') continue;
    emit(ctx, repl + run, i - run);
    if (group < 0) {
      emit(ctx, (const uint8_t *)"\\", 1);
    } else if (2 * group + 1 < nsave && caps[2 * group] >= 0) {
      emit(ctx, s + caps[2 * group], (size_t)(caps[2 * group + 1] - caps[2 * group]));
    }
    i++;
    run = i + 1;
  }
  emit(ctx, repl + run, rlen - run);
}

/* sub (global false) or gsub (global true) with a template. Emits
 * the whole result and returns the number of matches; with 0 the
 * caller keeps the subject as it was. After an empty match one
 * character is copied and the search resumes behind it. */
static int
picorb_rx_sub(const uint32_t *prog, uint32_t *scratch, const uint8_t *s, size_t len,
              const uint8_t *repl, size_t rlen, bool global,
              picorb_rx_emit emit, void *ctx)
{
  int32_t caps[PICORB_RX_MAX_NSAVE];
  int nsave = (int)prog[1];
  size_t pos = 0, last = 0;
  int count = 0;
  while (pos <= len) {
    if (!regex_exec(prog, s, len, pos, caps, false, scratch)) break;
    size_t b = (size_t)caps[0], e = (size_t)caps[1];
    count++;
    emit(ctx, s + last, b - last);
    picorb_rx_expand(repl, rlen, s, caps, nsave, emit, ctx);
    if (e == b) {
      if (b >= len) { last = len; break; }
      size_t w = picorb_utf8_charlen(s, len, b);
      emit(ctx, s + b, w);
      pos = last = b + w;
    } else {
      pos = last = e;
    }
    if (!global) break;
  }
  if (count) emit(ctx, s + last, len - last);
  return count;
}

/* scan: every match in order, the same stepping as gsub */
static int
picorb_rx_scan(const uint32_t *prog, uint32_t *scratch, const uint8_t *s, size_t len,
               picorb_rx_on_match on_match, void *ctx)
{
  int32_t caps[PICORB_RX_MAX_NSAVE];
  int nsave = (int)prog[1];
  size_t pos = 0;
  int count = 0;
  while (pos <= len) {
    if (!regex_exec(prog, s, len, pos, caps, false, scratch)) break;
    size_t b = (size_t)caps[0], e = (size_t)caps[1];
    count++;
    on_match(ctx, caps, nsave);
    if (e == b) {
      if (b >= len) break;
      pos = b + picorb_utf8_charlen(s, len, b);
    } else {
      pos = e;
    }
  }
  return count;
}

/* split: the pieces between matches, then the groups of each match
 * that took part, as CRuby does. An empty subject gives no piece. An
 * empty match splits between characters and never at the ends. With
 * limit > 0 at most limit pieces come out, the last one holding the
 * rest; the caller drops trailing empty pieces when limit is 0. */
static void
picorb_rx_split(const uint32_t *prog, uint32_t *scratch, const uint8_t *s, size_t len,
                long limit, picorb_rx_on_piece on_piece, void *ctx)
{
  int32_t caps[PICORB_RX_MAX_NSAVE];
  int nsave = (int)prog[1];
  size_t pos = 0, beg = 0;
  long count = 0;
  if (len == 0) return;
  while (pos <= len) {
    if (limit > 0 && count >= limit - 1) break;
    if (!regex_exec(prog, s, len, pos, caps, false, scratch)) break;
    size_t b = (size_t)caps[0], e = (size_t)caps[1];
    if (e == b) {
      if (b >= len) break;
      if (b == beg) {
        pos = b + picorb_utf8_charlen(s, len, b);
        continue;
      }
      on_piece(ctx, beg, b);
      count++;
      beg = b;
      pos = b + picorb_utf8_charlen(s, len, b);
    } else {
      on_piece(ctx, beg, b);
      count++;
      for (int g = 1; 2 * g + 1 < nsave; g++) {
        if (caps[2 * g] >= 0) on_piece(ctx, (size_t)caps[2 * g], (size_t)caps[2 * g + 1]);
      }
      pos = beg = e;
    }
  }
  on_piece(ctx, beg, len);
}

#if defined(PICORB_VM_MRUBY)
#include "mruby/regexp.c"
#elif defined(PICORB_VM_MRUBYC)
#include "mrubyc/regexp.c"
#endif
