/* regex_engine.c -- the regex engine of the corelib (spec 10.5).
 *
 * A pattern compiles to a small program; the program runs on a Pike
 * VM (Thompson NFA simulation carrying capture positions). The
 * choice is the whole point: matching costs O(input x program) in
 * time and O(program) in memory, with no recursion anywhere, so a
 * regex can never blow the task stack or hold the cooperative
 * scheduler hostage the way a backtracking engine does on (a*)*b.
 * The price is the absence of backreferences, which this engine does
 * not pretend to have.
 *
 * Lookahead and lookbehind are the one place the VM calls itself: a
 * (?=...) or (?<=...) is an epsilon edge whose test runs the body as a
 * sub-match at the current position, on the same code and on a
 * scratch level of its own. The body of a lookbehind must have a
 * bounded length, so the sub-match scans back at most that many
 * characters; the body of a lookahead may run to the end of the
 * subject, which is where the linear-time guarantee bends: a pattern
 * with lookahead costs O(input x program) per position in the worst
 * case. Lookaround nests at most RX_MAX_LOOK_DEPTH deep, which bounds
 * the C recursion and the scratch.
 *
 * A subexpression call \g<n> has no stack to return through either:
 * the compiler writes group n out again where it is called. A group
 * that calls itself is unrolled RX_MAX_CALL_DEPTH levels deep (fewer
 * when the copies outgrow the program), and input nested deeper than
 * that does not match.
 *
 * A possessive repeat (X*+, X++, X?+) gives back nothing it matched.
 * Of one character X it is written as the greedy repeat and a (?!X)
 * after it, which is exact: X*+ is X*(?!X), and X?+ is X or (?!X).
 * As in Ruby, X{n,m}+ is not possessive but (?:X{n,m})+. An atomic
 * group (?>X) is (?:X) in the same way. Of anything longer it is the greedy repeat,
 * so a match that a possessive repeat would refuse by keeping what it
 * took is still found; with no backtracking, the possessive form has
 * no time to save here.
 *
 * Memory follows the pattern, not the limits. The compiler grows its
 * three work buffers (AST nodes, class ranges, code) from a few
 * hundred bytes as the pattern demands; the RX_MAX_* values below are
 * ceilings, not allocations. The compiler recurses only along group
 * nesting (capped at 64): concatenation and alternation are
 * right-leaning chains that gen() walks in a loop, and a counted
 * repeat patches its splits through a chain threaded in the code
 * itself, so no frame holds an array. The VM allocates nothing: the
 * caller hands regex_exec a scratch block sized by
 * regex_scratch_words, which a scan or gsub loop allocates once.
 * Every allocation the compiler makes is checked; a failure is the
 * compile error "out of memory".
 *
 * The program is a flat array of words that the host keeps with its
 * Regexp object. Nothing here holds a value across calls.
 *
 * Text is validated UTF-8 (spec 10.2): the VM steps one codepoint at
 * a time, so `.` and a class consume a whole character, and every
 * position it reports is a byte offset into the subject, which is
 * what byteslice wants.
 *
 * The host binds regex_compile_words and regex_exec (regex_engine.h).
 */
#include "regex_engine.h"
#include <string.h>
#include <stdio.h>
#include <stdbool.h>

/* The compiler reaches the host allocator through these three names,
 * which the host defines. The VM allocates nothing. */
void *regex_malloc(size_t n);
void *regex_realloc(void *p, size_t n);
void regex_free(void *p);

/* ---- limits ----
 * Ceilings on what one pattern may compile to. The work buffers start
 * at RX_INIT items and double on demand up to these; nothing is
 * allocated up front at the ceiling. */

#define RX_MAX_INST   2048   /* instructions after {n,m} expansion */
#define RX_MAX_GROUPS REGEX_MAX_GROUPS  /* capture groups including group 0 */
#if RX_MAX_GROUPS > 64
#error "REGEX_MAX_GROUPS is at most 64 (rx_gen.open is a 64-bit set)"
#endif
#define RX_MAX_REPEAT 256    /* the n in {n,m} */
#define RX_MAX_NODES  4096   /* AST nodes */
#define RX_MAX_CLASS  65536  /* words of class-range storage */
#define RX_MAX_NAMES  512    /* bytes of group-name storage */
#define RX_MAX_NAME_LEN 64   /* bytes in one group name */
#define RX_MAX_LOOK_DEPTH 4  /* lookaround inside lookaround */
#define RX_MAX_CALL_DEPTH 8  /* \g<n> calls written out inside each other */
#define RX_INIT       32     /* first size of a work buffer, in items */

/* an unpatched slot in a backpatch chain */
#define RX_NONE 0xffffffffu

/* past the last codepoint: an OP_CHAR of it never matches */
#define RX_NO_CHAR 0x110000u

/* ---- flags (the public bits, regex_engine.h) ---- */

#define RX_ICASE  REGEX_ICASE
#define RX_DOTALL REGEX_DOTALL
#define RX_EXTENDED REGEX_EXTENDED

int
regex_flag_bits(const char *flags, size_t len)
{
  int bits = 0;
  for (size_t i = 0; i < len; i++) {
    if (flags[i] == 'i') bits |= (int)RX_ICASE;
    else if (flags[i] == 'm') bits |= (int)RX_DOTALL;
    else if (flags[i] == 'x') bits |= (int)RX_EXTENDED;
    else return -1;
  }
  return bits;
}

/* ---- program layout ----
 * uint32 words. Header, then instructions (3 words each), then the
 * class table. A class entry is [negated, nranges, lo0, hi0, ...]. */

enum {
  H_NINST = 0,
  H_NSAVE,      /* 2 * groups */
  H_FLAGS,      /* the options the pattern was compiled with */
  H_CLASS_OFF,  /* word offset of the class table */
  H_NLOOK,      /* OP_LOOK instructions */
  H_LOOK_DEPTH, /* deepest lookaround nesting */
  H_ANCHORED,   /* 1 when every match starts with ^ */
  H_NAMES_OFF,  /* word offset of the group name table, 0 without names */
  H_WORDS       /* header size */
};

enum {
  OP_CHAR = 1,  /* x = codepoint, y = flags */
  OP_ANY,       /* any codepoint (newline per DOTALL in y) */
  OP_CLASS,     /* x = class table offset, y = flags */
  OP_SPLIT,     /* x preferred, y second */
  OP_JMP,       /* x */
  OP_SAVE,      /* x = slot */
  OP_BOL,
  OP_EOL,
  OP_WORDB,     /* \b */
  OP_NWORDB,    /* \B */
  OP_LOOK,      /* x = body pc, y = RX_LOOK_* bits | max length << 3 */
  OP_LMATCH,    /* the end of a lookaround body */
  OP_MATCH
};

/* the y word of OP_LOOK (and the x of N_LOOK) */
#define RX_LOOK_NEG    1u   /* (?!...) or (?<!...) */
#define RX_LOOK_BEHIND 2u   /* (?<=...) or (?<!...) */
#define RX_LOOK_CAPS   4u   /* the body has a capture group */
#define RX_LOOK_SHIFT  3    /* the max character length of a lookbehind */
#define RX_LEN_CAP     (1 << 20)

/* ---- UTF-8 ----
 * Subjects are validated, so decoding needs no checks. Patterns are
 * Strings too. */

static uint32_t
utf8_decode(const uint8_t *p, size_t len, int *width)
{
  uint8_t b = p[0];
  if (b < 0x80 || len < 2) { *width = 1; return b; }
  if (b < 0xe0 || len < 3) {
    *width = 2;
    return ((uint32_t)(b & 0x1f) << 6) | (p[1] & 0x3f);
  }
  if (b < 0xf0 || len < 4) {
    *width = 3;
    return ((uint32_t)(b & 0x0f) << 12) | ((uint32_t)(p[1] & 0x3f) << 6) |
           (p[2] & 0x3f);
  }
  *width = 4;
  return ((uint32_t)(b & 0x07) << 18) | ((uint32_t)(p[1] & 0x3f) << 12) |
         ((uint32_t)(p[2] & 0x3f) << 6) | (p[3] & 0x3f);
}

static inline uint32_t
fold(uint32_t c)
{
  return (c >= 'A' && c <= 'Z') ? c + 32 : c;
}

static inline uint32_t
unfold(uint32_t c)
{
  return (c >= 'a' && c <= 'z') ? c - 32 : c;
}

/* ================================================================
 * Parser: pattern -> AST
 * ================================================================ */

enum {
  N_EMPTY = 0,
  N_CHAR,   /* x = codepoint, y = flags */
  N_ANY,    /* y = flags */
  N_CLASS,  /* x = class offset, y = flags */
  N_CAT,    /* a, b */
  N_ALT,    /* a, b */
  N_REP,    /* a; x = min, y = max (-1 = unbounded), b = greedy */
  N_GROUP,  /* a; x = group index, 0 when the group does not capture;
               y = 1 when the group is named */
  N_BOL,
  N_EOL,
  N_WORDB,
  N_NWORDB,
  N_LOOK,   /* a; x = RX_LOOK_* bits | max length << RX_LOOK_SHIFT */
  N_CALL    /* \g<n>: x = group number, a = its N_GROUP node (the root
               for group 0) once resolve_calls() ran, -1 before; for
               \g<name> until then, y = the name's byte offset in the
               pattern and b its length, 0 for a number */
};

typedef struct {
  uint8_t t;
  int32_t a, b;
  int32_t x, y;
} rx_node;

typedef struct {
  const uint8_t *src;
  size_t len;
  size_t pos;
  rx_node *nodes;
  int nnodes, cap_nodes;
  uint32_t *cls;      /* class storage */
  int ncls, cap_cls;
  uint8_t *names;     /* group names: [group, length, bytes...] entries */
  int nnames, cap_names;  /* bytes used and held */
  int nnamed;         /* named groups */
  int ngroups;        /* including group 0 */
  int nlook;          /* lookarounds in the pattern */
  int ncalls;         /* \g<...> calls */
  int ncall_numbered; /* of them by number */
  int look_depth;     /* lookarounds open around the current position */
  int look_depth_max;
  int flags;          /* the options in force at the current position */
  const char *err;    /* NULL while fine */
  size_t err_at;
} rx_parser;

/* Make *buf hold at least need items of elem bytes each; *cap is its
 * current size in items. Doubles from RX_INIT up to max. Returns
 * false with *err set (once) when max is exceeded or the allocator
 * gives up. Every index into these buffers stays valid across a
 * grow, which is why the parser and gen() speak in indices. */
static bool
rx_grow(void **buf, int *cap, int need, size_t elem, int max,
        const char **err, const char *too_large)
{
  if (need <= *cap) return true;
  if (need > max) { if (!*err) *err = too_large; return false; }
  int ncap = *cap < RX_INIT ? RX_INIT : *cap;
  while (ncap < need) ncap *= 2;
  if (ncap > max) ncap = max;
  void *nb = regex_realloc(*buf, elem * (size_t)ncap);
  if (!nb) { if (!*err) *err = "out of memory"; return false; }
  *buf = nb;
  *cap = ncap;
  return true;
}

static void
perr(rx_parser *p, const char *msg)
{
  if (!p->err) { p->err = msg; p->err_at = p->pos; }
}

/* room for n more items in the parser's buffers */
static bool
nodes_room(rx_parser *p, int n)
{
  const char *e = NULL;
  if (rx_grow((void **)&p->nodes, &p->cap_nodes, p->nnodes + n,
              sizeof(rx_node), RX_MAX_NODES, &e, "pattern too large")) {
    return true;
  }
  perr(p, e);
  return false;
}

static bool
cls_room(rx_parser *p, int n)
{
  const char *e = NULL;
  if (rx_grow((void **)&p->cls, &p->cap_cls, p->ncls + n,
              sizeof(uint32_t), RX_MAX_CLASS, &e, "pattern too large")) {
    return true;
  }
  perr(p, e);
  return false;
}

static bool
names_room(rx_parser *p, int n)
{
  const char *e = NULL;
  if (rx_grow((void **)&p->names, &p->cap_names, p->nnames + n,
              sizeof(uint8_t), RX_MAX_NAMES, &e, "pattern too large")) {
    return true;
  }
  perr(p, e);
  return false;
}

static int
node_new(rx_parser *p, uint8_t t, int32_t a, int32_t b, int32_t x, int32_t y)
{
  if (p->err) return 0;
  if (!nodes_room(p, 1)) return 0;
  rx_node *n = &p->nodes[p->nnodes];
  n->t = t; n->a = a; n->b = b; n->x = x; n->y = y;
  return p->nnodes++;
}

static bool
at_end(rx_parser *p)
{
  return p->pos >= p->len;
}

/* A (?#...) comment is not part of the pattern, and in extended mode (x)
 * neither is whitespace, nor a # with the rest of its line, outside a
 * character class; `\ ` and `\#` stay literal. Called where an atom or
 * a quantifier may start, so a quantifier after a comment applies to
 * the atom before it, as in Ruby. */
static void
skip_extended(rx_parser *p)
{
  bool x = (p->flags & RX_EXTENDED) != 0;
  while (p->pos < p->len) {
    uint8_t c = p->src[p->pos];
    /* a (?#...) comment, in any mode; a backslash escapes the next byte */
    if (c == '(' && p->pos + 2 < p->len && p->src[p->pos + 1] == '?' &&
        p->src[p->pos + 2] == '#') {
      p->pos += 3;
      while (p->pos < p->len && p->src[p->pos] != ')') {
        if (p->src[p->pos] == '\\') p->pos++;
        p->pos++;
      }
      if (p->pos >= p->len) { perr(p, "end pattern in group"); return; }
      p->pos++;
    } else if (!x) {
      break;
    } else if (c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\f' || c == '\v') {
      p->pos++;
    } else if (c == '#') {
      while (p->pos < p->len && p->src[p->pos] != '\n') p->pos++;
    } else {
      break;
    }
  }
}

static uint32_t
peek(rx_parser *p)
{
  int w;
  return utf8_decode(p->src + p->pos, p->len - p->pos, &w);
}

static uint32_t
next(rx_parser *p)
{
  int w;
  uint32_t c = utf8_decode(p->src + p->pos, p->len - p->pos, &w);
  p->pos += (size_t)w;
  return c;
}

/* ---- classes ---- */

static int
class_begin(rx_parser *p, bool negated)
{
  if (p->err) return 0;
  if (!cls_room(p, 2)) return 0;
  int off = p->ncls;
  p->cls[p->ncls++] = negated ? 1u : 0u;
  p->cls[p->ncls++] = 0;
  return off;
}

static void
class_add(rx_parser *p, int off, uint32_t lo, uint32_t hi)
{
  if (p->err) return;
  if (!cls_room(p, 2)) return;
  p->cls[p->ncls++] = lo;
  p->cls[p->ncls++] = hi;
  p->cls[off + 1]++;
}

/* \d \w \s and their negations: ASCII definitions (Go's default). */
static bool
class_add_escape(rx_parser *p, int off, uint32_t e)
{
  switch (e) {
  case 'd': class_add(p, off, '0', '9'); return true;
  case 'w':
    class_add(p, off, 'a', 'z'); class_add(p, off, 'A', 'Z');
    class_add(p, off, '0', '9'); class_add(p, off, '_', '_');
    return true;
  case 's':
    class_add(p, off, ' ', ' '); class_add(p, off, '\t', '\r');
    return true;
  case 'h':
    class_add(p, off, '0', '9'); class_add(p, off, 'a', 'f'); class_add(p, off, 'A', 'F');
    return true;
  default: return false;
  }
}

static int
negated_escape_class(rx_parser *p, uint32_t e)
{
  int off = class_begin(p, true);
  class_add_escape(p, off, e);
  return off;
}

/* the escapes that stand for one literal codepoint */
static bool
literal_escape(uint32_t e, uint32_t *out)
{
  switch (e) {
  case 'n': *out = '\n'; return true;
  case 't': *out = '\t'; return true;
  case 'r': *out = '\r'; return true;
  case 'f': *out = '\f'; return true;
  case 'v': *out = '\v'; return true;
  case 'e': *out = 27; return true;
  case '0': *out = 0; return true;
  default:
    if (e < 0x80 && !((e >= 'a' && e <= 'z') || (e >= 'A' && e <= 'Z') ||
                      (e >= '0' && e <= '9'))) {
      *out = e;   /* an escaped punctuation character is itself */
      return true;
    }
    return false;
  }
}

static int
hex_value(uint8_t c)
{
  if (c >= '0' && c <= '9') return c - '0';
  if (c >= 'a' && c <= 'f') return c - 'a' + 10;
  if (c >= 'A' && c <= 'F') return c - 'A' + 10;
  return -1;
}

/* The codepoint of \xHH, \uHHHH or \u{H...} after the x or u has been
 * read: false when the digits are missing or the value is no codepoint. */
static bool
code_escape(rx_parser *p, uint32_t e, uint32_t *out)
{
  uint32_t v = 0;
  int n = 0;
  if (e == 'x') {
    while (n < 2 && !at_end(p) && hex_value(p->src[p->pos]) >= 0) {
      v = v * 16 + (uint32_t)hex_value(p->src[p->pos++]);
      n++;
    }
    if (n == 0) { perr(p, "invalid hex escape"); return false; }
  } else if (!at_end(p) && p->src[p->pos] == '{') {
    p->pos++;
    while (!at_end(p) && hex_value(p->src[p->pos]) >= 0) {
      v = v * 16 + (uint32_t)hex_value(p->src[p->pos++]);
      if (++n > 8) { perr(p, "invalid Unicode escape"); return false; }
    }
    if (n == 0 || at_end(p) || next(p) != '}') { perr(p, "invalid Unicode escape"); return false; }
  } else {
    while (n < 4 && !at_end(p) && hex_value(p->src[p->pos]) >= 0) {
      v = v * 16 + (uint32_t)hex_value(p->src[p->pos++]);
      n++;
    }
    if (n < 4) { perr(p, "invalid Unicode escape"); return false; }
  }
  if (v > 0x10FFFF) { perr(p, "invalid Unicode escape"); return false; }
  *out = v;
  return true;
}

/* one item inside [...]: a codepoint, or an escape naming a set */
static uint32_t
class_item(rx_parser *p, int off, bool *was_set)
{
  *was_set = false;
  uint32_t c = next(p);
  if (c != '\\') return c;
  if (at_end(p)) { perr(p, "trailing backslash"); return 0; }
  uint32_t e = next(p);
  uint32_t lit;
  if (class_add_escape(p, off, e)) { *was_set = true; return 0; }
  if (e == 'x' || e == 'u') return code_escape(p, e, &lit) ? lit : 0;
  if (literal_escape(e, &lit)) return lit;
  perr(p, "unsupported escape in class");
  return 0;
}

static int
parse_class(rx_parser *p)
{
  bool negated = false;
  if (!at_end(p) && peek(p) == '^') { next(p); negated = true; }
  int off = class_begin(p, negated);
  bool first = true;
  for (;;) {
    if (at_end(p)) { perr(p, "unterminated class"); return off; }
    uint32_t c = peek(p);
    if (c == ']' && !first) { next(p); break; }
    first = false;
    bool set;
    uint32_t lo = class_item(p, off, &set);
    if (p->err) return off;
    if (set) continue;
    /* a range? `-` is literal at either end of the class */
    if (!at_end(p) && peek(p) == '-' && p->pos + 1 < p->len &&
        p->src[p->pos + 1] != ']') {
      next(p);
      bool set2;
      uint32_t hi = class_item(p, off, &set2);
      if (p->err) return off;
      if (set2) { perr(p, "bad range in class"); return off; }
      if (hi < lo) { perr(p, "reversed range in class"); return off; }
      class_add(p, off, lo, hi);
    } else {
      class_add(p, off, lo, lo);
    }
  }
  return off;
}

/* ---- expressions ---- */

static int parse_alt(rx_parser *p, int depth);

static inline int32_t
sat_add(int32_t a, int32_t b)
{
  return a + b > RX_LEN_CAP ? RX_LEN_CAP : a + b;
}

/* The most characters node n can match, capped at RX_LEN_CAP; *unbounded
 * is set when a star or plus leaves no bound. Walks a CAT or ALT chain
 * in a loop like gen() and recurses only into the left children and
 * the bodies of groups and repeats. A lookbehind needs this bound: its
 * sub-match scans back that many characters. */
static int32_t
max_len(const rx_parser *p, int n, bool *unbounded)
{
  int32_t acc = 0, best = 0;
  bool alt = false;
  for (;;) {
    const rx_node *nd = &p->nodes[n];
    if (nd->t == N_CAT && !alt) {
      acc = sat_add(acc, max_len(p, nd->a, unbounded));
      n = nd->b;
      continue;
    }
    if (nd->t == N_ALT) {
      alt = true;
      int32_t l = max_len(p, nd->a, unbounded);
      if (l > best) best = l;
      n = nd->b;
      continue;
    }
    int32_t v;
    switch (nd->t) {
    case N_CHAR: case N_ANY: case N_CLASS: v = 1; break;
    case N_GROUP: v = max_len(p, nd->a, unbounded); break;
    case N_CAT: v = max_len(p, n, unbounded); break;   /* the last alternative */
    case N_CALL: *unbounded = true; v = 0; break;   /* may recurse */
    case N_REP:
      if (nd->y < 0) { *unbounded = true; v = 0; }
      else {
        int32_t one = max_len(p, nd->a, unbounded);
        v = one > RX_LEN_CAP / (nd->y ? nd->y : 1) ? RX_LEN_CAP : one * nd->y;
      }
      break;
    default: v = 0; break;   /* EMPTY, BOL, EOL, WORDB, NWORDB, LOOK */
    }
    if (alt && best > v) v = best;
    return sat_add(acc, v);
  }
}

/* Does every path through node n begin with ^? Then a search can
 * stop after its first position instead of trying each one. Walks
 * the chains like max_len. */
static bool
starts_with_bol(const rx_parser *p, int n)
{
  for (;;) {
    const rx_node *nd = &p->nodes[n];
    switch (nd->t) {
    case N_BOL: return true;
    case N_CAT: case N_GROUP: n = nd->a; continue;
    case N_ALT:
      if (!starts_with_bol(p, nd->a)) return false;
      n = nd->b;
      continue;
    case N_REP:
      if (nd->x < 1) return false;
      n = nd->a;
      continue;
    default: return false;
    }
  }
}

/* (?imx-imx:...) scopes options to its body and (?imx-imx) sets them
 * for the rest of the enclosing group, both after the `(?` was seen. */
static int
parse_options(rx_parser *p, int depth)
{
  int flags = p->flags;
  bool on = true;
  for (;;) {
    if (at_end(p)) { perr(p, "missing )"); return 0; }
    uint8_t c = p->src[p->pos];
    if (c == 'i') flags = on ? flags | RX_ICASE : flags & ~RX_ICASE;
    else if (c == 'm') flags = on ? flags | RX_DOTALL : flags & ~RX_DOTALL;
    else if (c == 'x') flags = on ? flags | RX_EXTENDED : flags & ~RX_EXTENDED;
    else if (c == '-') on = false;
    else if (c == ':') {
      p->pos++;
      int saved = p->flags;
      p->flags = flags;
      int inner = parse_alt(p, depth + 1);
      p->flags = saved;
      if (p->err) return 0;
      if (at_end(p) || next(p) != ')') { perr(p, "missing )"); return 0; }
      return inner;
    }
    else if (c == ')') {
      p->pos++;
      p->flags = flags;
      return node_new(p, N_EMPTY, 0, 0, 0, 0);
    }
    else { perr(p, "unsupported group syntax"); return 0; }
    p->pos++;
  }
}

/* (?=...), (?!...), (?<=...) and (?<!...) after the `(?` was seen;
 * look holds the RX_LOOK_NEG and RX_LOOK_BEHIND bits */
static int
parse_look(rx_parser *p, int depth, uint32_t look)
{
  if (++p->look_depth > RX_MAX_LOOK_DEPTH) {
    perr(p, "lookaround nested too deep");
    return 0;
  }
  if (p->look_depth > p->look_depth_max) p->look_depth_max = p->look_depth;
  int groups_before = p->ngroups;
  int saved = p->flags;
  int inner = parse_alt(p, depth + 1);
  p->flags = saved;
  if (p->err) return 0;
  if (at_end(p) || next(p) != ')') { perr(p, "missing )"); return 0; }
  p->look_depth--;
  p->nlook++;
  if (p->ngroups != groups_before) look |= RX_LOOK_CAPS;
  if (look & RX_LOOK_BEHIND) {
    bool unbounded = false;
    int32_t len = max_len(p, inner, &unbounded);
    if (unbounded) { perr(p, "lookbehind must have a bounded length"); return 0; }
    look |= (uint32_t)len << RX_LOOK_SHIFT;
  }
  return node_new(p, N_LOOK, inner, 0, (int32_t)look, 0);
}

/* The name of a (?<name>...) group, up to the closing byte close:
 * any bytes that do not start with a digit or a minus, as in CRuby. Appends an
 * entry [group, length, bytes...] to p->names and returns its offset,
 * with the group slot still to be filled in by the caller; -1 on an
 * error. */
static int
parse_group_name(rx_parser *p, uint8_t close)
{
  size_t start = p->pos;
  /* As in Onigmo, any byte but the closing one; only the first may not
     be a digit or a minus, as in Onigmo */
  if (p->pos < p->len) {
    uint8_t c = p->src[p->pos];
    if ((c >= '0' && c <= '9') || c == '-') {
      perr(p, "invalid group name");
      return -1;
    }
  }
  while (p->pos < p->len && p->src[p->pos] != close) p->pos++;
  size_t n = p->pos - start;
  if (n == 0 || n > RX_MAX_NAME_LEN) {
    p->pos = start;
    perr(p, n == 0 ? "invalid group name" : "group name too long");
    return -1;
  }
  if (at_end(p)) { perr(p, "unterminated group name"); return -1; }
  p->pos++;
  if (!names_room(p, 2 + (int)n)) return -1;
  int entry = p->nnames;
  p->names[p->nnames++] = 0;
  p->names[p->nnames++] = (uint8_t)n;
  memcpy(p->names + p->nnames, p->src + start, n);
  p->nnames += (int)n;
  return entry;
}

/* With a named group in the pattern, plain (...) groups do not
 * capture, as in CRuby: the named groups are renumbered 1..n in
 * order and every other group is left as a plain group. */
static void
renumber_groups(rx_parser *p)
{
  int map[RX_MAX_GROUPS];
  int next_idx = 1;
  memset(map, 0, sizeof map);
  for (int i = 0; i < p->nnames; i += 2 + p->names[i + 1]) {
    map[p->names[i]] = next_idx;
    p->names[i] = (uint8_t)next_idx++;
  }
  for (int i = 0; i < p->nnodes; i++) {
    rx_node *nd = &p->nodes[i];
    if (nd->t == N_GROUP) nd->x = map[nd->x];
  }
  p->ngroups = next_idx;
}

/* \g<n>, \g<-n>, \g<+n> (relative to the groups opened so far) and
 * \g<name>, or the same in quotes, after the `\g` was seen: a call of
 * group n, which gen() writes out in place. The group is looked up
 * once the whole pattern is parsed (resolve_calls), as it may come
 * later or enclose the call. A \g with neither is a g, as in CRuby. */
static int
parse_call(rx_parser *p)
{
  if (at_end(p) || (p->src[p->pos] != '<' && p->src[p->pos] != '\'')) {
    /* CRuby warns and takes a bare \g as a g */
    return node_new(p, N_CHAR, 0, 0, 'g', p->flags);
  }
  uint8_t close = p->src[p->pos] == '<' ? '>' : '\'';
  size_t start = ++p->pos;
  p->ncalls++;
  while (p->pos < p->len && p->src[p->pos] != close) p->pos++;
  if (at_end(p)) { perr(p, "unterminated group name"); return 0; }
  size_t n = p->pos - start;
  p->pos++;
  const uint8_t *s = p->src + start;
  size_t i = 0;
  int sign = 0;
  if (n > 0 && (s[0] == '-' || s[0] == '+')) { sign = s[0] == '-' ? -1 : 1; i = 1; }
  if (i < n && s[i] >= '0' && s[i] <= '9') {
    int num = 0;
    for (; i < n; i++) {
      if (s[i] < '0' || s[i] > '9') { perr(p, "invalid group name"); return 0; }
      num = num * 10 + (s[i] - '0');
      if (num > RX_MAX_GROUPS) { perr(p, "undefined group"); return 0; }
    }
    if (sign < 0) num = p->ngroups - num;
    else if (sign > 0) num = p->ngroups - 1 + num;
    if (num < 0 || (sign && num == 0)) { perr(p, "undefined group"); return 0; }
    p->ncall_numbered++;
    return node_new(p, N_CALL, -1, 0, num, 0);
  }
  if (sign || n == 0 || n > RX_MAX_NAME_LEN) { perr(p, "invalid group name"); return 0; }
  return node_new(p, N_CALL, -1, (int32_t)n, 0, (int32_t)start);
}

/* Point every call at the group it names, once all groups are known.
 * As in CRuby, a pattern with named groups calls them by name only. */
static void
resolve_calls(rx_parser *p, int root)
{
  for (int i = 0; i < p->nnodes && !p->err; i++) {
    rx_node *nd = &p->nodes[i];
    if (nd->t != N_CALL) continue;
    int idx = nd->x;
    if (nd->b > 0) {
      idx = -1;
      for (int e = 0; e < p->nnames; e += 2 + p->names[e + 1]) {
        if (p->names[e + 1] == nd->b &&
            memcmp(p->names + e + 2, p->src + nd->y, (size_t)nd->b) == 0) {
          idx = p->names[e];
          break;
        }
      }
      if (idx < 0) { perr(p, "undefined name reference"); return; }
    } else if (p->nnamed) {
      perr(p, "numbered backref/call is not allowed (use name)");
      return;
    }
    int target = -1;
    if (idx == 0) {
      target = root;
    } else {
      for (int j = 0; j < p->nnodes; j++) {
        if (p->nodes[j].t == N_GROUP && p->nodes[j].x == idx) { target = j; break; }
      }
    }
    if (target < 0) { perr(p, "undefined group"); return; }
    nd->x = idx;
    nd->a = target;
  }
}

static int
parse_atom(rx_parser *p, int depth)
{
  uint32_t c = next(p);
  switch (c) {
  case '(': {
    bool capture = true;
    int name_entry = -1;
    if (p->pos + 1 < p->len && p->src[p->pos] == '?') {
      uint8_t k = p->src[p->pos + 1];
      /* an atomic group (?>...) is a plain one (see the header) */
      if (k == ':' || k == '>') { p->pos += 2; capture = false; }
      else if (k == 'i' || k == 'm' || k == 'x' || k == '-') {
        p->pos += 1;
        return parse_options(p, depth);
      }
      else if (k == '=') { p->pos += 2; return parse_look(p, depth, 0); }
      else if (k == '!') { p->pos += 2; return parse_look(p, depth, RX_LOOK_NEG); }
      else if (k == '<' && p->pos + 2 < p->len &&
               (p->src[p->pos + 2] == '=' || p->src[p->pos + 2] == '!')) {
        uint32_t look = RX_LOOK_BEHIND;
        if (p->src[p->pos + 2] == '!') look |= RX_LOOK_NEG;
        p->pos += 3;
        return parse_look(p, depth, look);
      }
      else if (k == '<' || k == '\'') {
        p->pos += 2;
        name_entry = parse_group_name(p, k == '<' ? '>' : '\'');
        if (name_entry < 0) return 0;
      }
      else { perr(p, "unsupported group syntax"); return 0; }
    }
    int idx = 0;
    if (capture) {
      if (p->ngroups >= RX_MAX_GROUPS) { perr(p, "too many groups"); return 0; }
      idx = p->ngroups++;
      if (name_entry >= 0) {
        p->names[name_entry] = (uint8_t)idx;
        p->nnamed++;
      }
    }
    int saved = p->flags;
    int inner = parse_alt(p, depth + 1);
    p->flags = saved;
    if (p->err) return 0;
    if (at_end(p) || next(p) != ')') { perr(p, "missing )"); return 0; }
    return capture ? node_new(p, N_GROUP, inner, 0, idx, name_entry >= 0) : inner;
  }
  case ')': perr(p, "unmatched )"); return 0;
  case '[': return node_new(p, N_CLASS, 0, 0, parse_class(p), p->flags);
  case '.': return node_new(p, N_ANY, 0, 0, 0, p->flags);
  case '^': return node_new(p, N_BOL, 0, 0, 0, 0);
  case '$': return node_new(p, N_EOL, 0, 0, 0, 0);
  case '*': case '+': case '?': perr(p, "nothing to repeat"); return 0;
  case '{': return node_new(p, N_CHAR, 0, 0, '{', p->flags);
  case '\\': {
    if (at_end(p)) { perr(p, "trailing backslash"); return 0; }
    uint32_t e = next(p);
    uint32_t lit;
    switch (e) {
    case 'A': return node_new(p, N_BOL, 0, 0, 0, 0);
    case 'z': case 'Z': return node_new(p, N_EOL, 0, 0, 0, 0);
    case 'b': return node_new(p, N_WORDB, 0, 0, 0, 0);
    case 'B': return node_new(p, N_NWORDB, 0, 0, 0, 0);
    case 'd': case 'w': case 's': case 'h': {
      int off = class_begin(p, false);
      class_add_escape(p, off, e);
      return node_new(p, N_CLASS, 0, 0, off, p->flags);
    }
    case 'D': case 'W': case 'S': case 'H':
      return node_new(p, N_CLASS, 0, 0, negated_escape_class(p, e + 32), p->flags);
    case 'x': case 'u':
      if (!code_escape(p, e, &lit)) return 0;
      return node_new(p, N_CHAR, 0, 0, (int32_t)lit, p->flags);
    case 'g':
      return parse_call(p);
    default:
      if (literal_escape(e, &lit)) return node_new(p, N_CHAR, 0, 0, (int32_t)lit, p->flags);
      if (e >= '1' && e <= '9') perr(p, "backreferences are not supported");
      else perr(p, "unsupported escape");
      return 0;
    }
  }
  default:
    return node_new(p, N_CHAR, 0, 0, (int32_t)c, p->flags);
  }
}

/* {n}, {n,}, {n,m}: false when the brace does not open a counted
 * repetition (then `{` is a literal, as in Ruby) */
static bool
parse_braces(rx_parser *p, int *min, int *max)
{
  size_t save = p->pos;
  int n = 0, m = -1;
  bool any = false;
  while (!at_end(p) && p->src[p->pos] >= '0' && p->src[p->pos] <= '9') {
    n = n * 10 + (p->src[p->pos] - '0');
    if (n > RX_MAX_REPEAT) { perr(p, "repeat count too large"); return false; }
    p->pos++; any = true;
  }
  if (!any) { p->pos = save; return false; }
  if (!at_end(p) && p->src[p->pos] == ',') {
    p->pos++;
    if (!at_end(p) && p->src[p->pos] >= '0' && p->src[p->pos] <= '9') {
      m = 0;
      while (!at_end(p) && p->src[p->pos] >= '0' && p->src[p->pos] <= '9') {
        m = m * 10 + (p->src[p->pos] - '0');
        if (m > RX_MAX_REPEAT) { perr(p, "repeat count too large"); return false; }
        p->pos++;
      }
      if (m < n) { perr(p, "reversed repeat count"); return false; }
    }
  } else {
    m = n;
  }
  if (at_end(p) || p->src[p->pos] != '}') { p->pos = save; return false; }
  p->pos++;
  *min = n; *max = m;
  return true;
}

/* A copy of the one-character atom at index i (N_CHAR, N_ANY or N_CLASS,
 * which have no children), or -1 when it is anything else. */
static int
copy_char_atom(rx_parser *p, int i)
{
  rx_node n = p->nodes[i];
  if (n.t != N_CHAR && n.t != N_ANY && n.t != N_CLASS) return -1;
  return node_new(p, n.t, n.a, n.b, n.x, n.y);
}

/* X{min,max}+ of the one-character atom at index atom (see the header):
 * the greedy repeat, and where it can stop short of max, a (?!X) */
static int
possessive_char_rep(rx_parser *p, int atom, int min, int max)
{
  if (max == min) return node_new(p, N_REP, atom, 1, min, max);
  if (p->look_depth + 1 > RX_MAX_LOOK_DEPTH) {
    perr(p, "lookaround nested too deep");
    return 0;
  }
  int not_x = copy_char_atom(p, atom);
  int look = node_new(p, N_LOOK, not_x, 0, (int32_t)RX_LOOK_NEG, 0);
  if (p->err) return 0;
  p->nlook++;
  if (p->look_depth + 1 > p->look_depth_max) p->look_depth_max = p->look_depth + 1;
  if (max < 0) {
    int rep = node_new(p, N_REP, atom, 1, min, max);
    return node_new(p, N_CAT, rep, look, 0, 0);
  }
  int all = node_new(p, N_REP, atom, 1, max, max);
  int fewer = node_new(p, N_REP, copy_char_atom(p, atom), 1, min, max - 1);
  int stop = node_new(p, N_CAT, fewer, look, 0, 0);
  return node_new(p, N_ALT, all, stop, 0, 0);
}

static int
parse_repeat(rx_parser *p, int depth)
{
  int atom = parse_atom(p, depth);
  if (p->err) return 0;
  for (;;) {
    skip_extended(p);
    if (p->err) return 0;
    if (at_end(p)) return atom;
    uint32_t c = peek(p);
    int min, max;
    bool braces = false;
    if (c == '*') { next(p); min = 0; max = -1; }
    else if (c == '+') { next(p); min = 1; max = -1; }
    else if (c == '?') { next(p); min = 0; max = 1; }
    else if (c == '{') {
      next(p);
      braces = true;
      if (!parse_braces(p, &min, &max)) {
        if (p->err) return 0;
        /* a literal brace follows the atom: leave it for the caller */
        p->pos--;
        return atom;
      }
    }
    else return atom;
    rx_node *n = &p->nodes[atom];
    if (n->t == N_BOL || n->t == N_EOL || n->t == N_EMPTY) {
      perr(p, "nothing to repeat");
      return 0;
    }
    bool greedy = true;
    if (!at_end(p) && peek(p) == '?') { next(p); greedy = false; }
    /* X{n,m}+ is (?:X{n,m})+ in Ruby, a repeat of the repeat */
    else if (!braces && !at_end(p) && peek(p) == '+') {
      next(p);
      uint8_t t = n->t;
      if (t == N_CHAR || t == N_ANY || t == N_CLASS) {
        atom = possessive_char_rep(p, atom, min, max);
        if (p->err) return 0;
        continue;
      }
    }
    atom = node_new(p, N_REP, atom, greedy ? 1 : 0, min, max);
    if (p->err) return 0;
  }
}

/* Append item to a right-leaning chain of binary nodes of type t:
 * x1 t (x2 t (x3 t x4)). *root is the chain (or the lone first item)
 * and *tail the last t node, -1 while there is none. gen() walks
 * such a chain in a loop, so a long concatenation or alternation
 * costs no stack. Returns false on a parse error. */
static bool
chain_append(rx_parser *p, uint8_t t, int *root, int *tail, int item)
{
  if (*root < 0) {
    *root = item;
  } else if (*tail < 0) {
    *root = *tail = node_new(p, t, *root, item, 0, 0);
  } else {
    /* node_new may move p->nodes: read before, index after */
    int last = p->nodes[*tail].b;
    int c = node_new(p, t, last, item, 0, 0);
    if (p->err) return false;
    p->nodes[*tail].b = c;
    *tail = c;
  }
  return !p->err;
}

static int
parse_cat(rx_parser *p, int depth)
{
  int root = -1, tail = -1;
  for (;;) {
    skip_extended(p);
    if (at_end(p) || p->err) break;
    uint32_t c = peek(p);
    if (c == '|' || c == ')') break;
    int r = parse_repeat(p, depth);
    if (p->err) return 0;
    if (!chain_append(p, N_CAT, &root, &tail, r)) return 0;
  }
  return root < 0 ? node_new(p, N_EMPTY, 0, 0, 0, 0) : root;
}

static int
parse_alt(rx_parser *p, int depth)
{
  if (depth > 64) { perr(p, "groups nested too deep"); return 0; }
  int root = -1, tail = -1;
  int first = parse_cat(p, depth);
  if (p->err) return 0;
  chain_append(p, N_ALT, &root, &tail, first);
  while (!at_end(p) && !p->err && peek(p) == '|') {
    next(p);
    int right = parse_cat(p, depth);
    if (p->err) return 0;
    if (!chain_append(p, N_ALT, &root, &tail, right)) return 0;
  }
  return root;
}

/* ================================================================
 * Code generation: AST -> program words
 * ================================================================ */

typedef struct {
  uint32_t *code;   /* 3 words per instruction */
  int ninst, cap;   /* cap in instructions */
  const char *err;
  int call_depth;   /* \g<n> calls being written out around this point */
  int call_max;     /* how deep they may go */
  uint64_t open;    /* bit n: group n is being written out around this point */
} rx_gen;

static int
emit(rx_gen *g, uint32_t op, uint32_t x, uint32_t y)
{
  if (g->err) return 0;
  if (!rx_grow((void **)&g->code, &g->cap, g->ninst + 1, 3 * sizeof(uint32_t),
               RX_MAX_INST, &g->err, "program too large")) {
    return 0;
  }
  uint32_t *i = g->code + 3 * g->ninst;
  i[0] = op; i[1] = x; i[2] = y;
  return g->ninst++;
}

static void
patch(rx_gen *g, int at, int slot, int target)
{
  if (g->err) return;
  g->code[3 * at + slot] = (uint32_t)target;
}

static void gen(rx_gen *g, const rx_parser *p, int n);

static void
gen_star(rx_gen *g, const rx_parser *p, int body, bool greedy)
{
  int l1 = emit(g, OP_SPLIT, 0, 0);
  int l2 = g->ninst;
  gen(g, p, body);
  emit(g, OP_JMP, (uint32_t)l1, 0);
  int l3 = g->ninst;
  if (greedy) { patch(g, l1, 1, l2); patch(g, l1, 2, l3); }
  else        { patch(g, l1, 1, l3); patch(g, l1, 2, l2); }
}

/* The optional tail of {n,m}: nopt copies of body, each behind a
 * SPLIT that may skip to the end of the whole tail. The end is not
 * known until the last copy is out, so every SPLIT's second slot
 * holds the index of the SPLIT before it for now (RX_NONE first),
 * and a walk down that chain patches all of them at the end. No
 * frame array, whatever RX_MAX_REPEAT is. */
static void
gen_optional(rx_gen *g, const rx_parser *p, int body, int nopt, bool greedy)
{
  uint32_t chain = RX_NONE;
  for (int i = 0; i < nopt; i++) {
    chain = (uint32_t)emit(g, OP_SPLIT, 0, chain);
    gen(g, p, body);
  }
  if (g->err) return;
  int end = g->ninst;
  while (chain != RX_NONE) {
    int at = (int)chain;
    chain = g->code[3 * at + 2];
    int next_inst = at + 1;
    if (greedy) { patch(g, at, 1, next_inst); patch(g, at, 2, end); }
    else        { patch(g, at, 1, end); patch(g, at, 2, next_inst); }
  }
}

/* Emit the code for node n. Recursion follows only the left child
 * of a chain node and the body of a group or a repeat, all bounded
 * by group nesting. The right child of N_CAT and N_ALT is taken by
 * looping, since parse_cat and parse_alt build right-leaning chains.
 * The JMP that ends each alternative on the spine must reach the end
 * of the whole chain, which is known only when the loop leaves: the
 * pending JMPs are threaded through their own target slot the way
 * gen_optional threads its SPLITs. */
static void
gen(rx_gen *g, const rx_parser *p, int n)
{
  uint32_t jmps = RX_NONE;
  for (;;) {
    if (g->err) return;
    const rx_node *nd = &p->nodes[n];
    if (nd->t == N_CAT) {
      gen(g, p, nd->a);
      n = nd->b;
      continue;
    }
    if (nd->t == N_ALT) {
      int split = emit(g, OP_SPLIT, 0, 0);
      int l1 = g->ninst;
      gen(g, p, nd->a);
      jmps = (uint32_t)emit(g, OP_JMP, jmps, 0);
      patch(g, split, 1, l1);
      patch(g, split, 2, g->ninst);
      n = nd->b;
      continue;
    }
    switch (nd->t) {
    case N_EMPTY: break;
    case N_CHAR:  emit(g, OP_CHAR, (uint32_t)nd->x, (uint32_t)nd->y); break;
    case N_ANY:   emit(g, OP_ANY, 0, (uint32_t)nd->y); break;
    case N_CLASS: emit(g, OP_CLASS, (uint32_t)nd->x, (uint32_t)nd->y); break;
    case N_BOL:   emit(g, OP_BOL, 0, 0); break;
    case N_EOL:   emit(g, OP_EOL, 0, 0); break;
    case N_WORDB: emit(g, OP_WORDB, 0, 0); break;
    case N_NWORDB: emit(g, OP_NWORDB, 0, 0); break;
    case N_GROUP:
      /* A copy of the group inside itself (a recursive \g<n>) ends
       * before the enclosing one, whose span CRuby reports: the copy
       * leaves the group's slots alone. */
      if (nd->x == 0 || (g->open & (1ull << nd->x))) { gen(g, p, nd->a); break; }
      g->open |= 1ull << nd->x;
      emit(g, OP_SAVE, (uint32_t)(2 * nd->x), 0);
      gen(g, p, nd->a);
      emit(g, OP_SAVE, (uint32_t)(2 * nd->x + 1), 0);
      g->open &= ~(1ull << nd->x);
      break;
    case N_REP: {
      bool greedy = nd->b != 0;
      for (int i = 0; i < nd->x; i++) gen(g, p, nd->a);
      if (nd->y < 0) gen_star(g, p, nd->a, greedy);
      else gen_optional(g, p, nd->a, nd->y - nd->x, greedy);
      break;
    }
    case N_CALL:
      /* A Pike VM has no stack to return through, so a call is the
       * group written out again here. A recursive one is unrolled
       * call_max deep; the innermost copy matches nothing. */
      if (g->call_depth >= g->call_max) {
        emit(g, OP_CHAR, RX_NO_CHAR, 0);
        break;
      }
      g->call_depth++;
      gen(g, p, nd->a);
      g->call_depth--;
      break;
    case N_LOOK: {
      /* LOOK runs the body as a sub-match and falls through to the
       * JMP, which steps over the body; the body ends in LMATCH */
      int look = emit(g, OP_LOOK, 0, (uint32_t)nd->x);
      int over = emit(g, OP_JMP, 0, 0);
      patch(g, look, 1, g->ninst);
      gen(g, p, nd->a);
      emit(g, OP_LMATCH, 0, 0);
      patch(g, over, 1, g->ninst);
      break;
    }
    }
    break;
  }
  if (g->err) return;
  int end = g->ninst;
  while (jmps != RX_NONE) {
    int at = (int)jmps;
    jmps = g->code[3 * at + 1];
    patch(g, at, 1, end);
  }
}

/* ---- the whole compile: pattern + flags -> program words ---- */

uint32_t *
regex_compile_words(const uint8_t *src, size_t len, int flags,
                       size_t *nwords, const char **err, size_t *err_at)
{
  /* the work buffers start empty and grow with the pattern (rx_grow) */
  rx_parser p;
  memset(&p, 0, sizeof p);
  p.src = src; p.len = len; p.flags = flags;
  p.ngroups = 1;
  *err = NULL;
  *err_at = 0;
  *nwords = 0;

  int root = parse_alt(&p, 0);
  if (!p.err && !at_end(&p)) perr(&p, "unmatched )");
  if (!p.err && p.nnamed) renumber_groups(&p);
  if (!p.err) resolve_calls(&p, root);

  rx_gen g;
  memset(&g, 0, sizeof g);
  uint32_t *prog = NULL;
  if (!p.err) {
    /* Each level of recursion a call is unrolled to copies the group
     * again; when that outgrows the program, settle for fewer. */
    for (g.call_max = RX_MAX_CALL_DEPTH; ; g.call_max--) {
      emit(&g, OP_SAVE, 0, 0);
      gen(&g, &p, root);
      emit(&g, OP_SAVE, 1, 0);
      emit(&g, OP_MATCH, 0, 0);
      if (!g.err || !p.ncalls || g.call_max <= 1 ||
          strcmp(g.err, "program too large") != 0) {
        break;
      }
      g.err = NULL;
      g.ninst = 0;
      g.call_depth = 0;
      g.open = 0;
    }
    if (g.err) { *err = g.err; *err_at = 0; }
  } else {
    *err = p.err; *err_at = p.err_at;
  }

  /* the name table: a count, then [group, length, name bytes] per
   * named group with the bytes padded to whole words */
  size_t name_words = 0;
  if (p.nnamed) {
    name_words = 1;
    for (int i = 0; i < p.nnames; i += 2 + p.names[i + 1]) {
      name_words += 2 + ((size_t)p.names[i + 1] + 3) / 4;
    }
  }
  size_t words = H_WORDS + 3 * (size_t)g.ninst + (size_t)p.ncls + name_words;
  if (!*err) {
    prog = regex_malloc(words * sizeof(uint32_t));
    if (!prog) { *err = "out of memory"; *err_at = 0; }
  }
  if (prog) {
    prog[H_NINST] = (uint32_t)g.ninst;
    prog[H_NSAVE] = (uint32_t)(2 * p.ngroups);
    prog[H_FLAGS] = (uint32_t)flags;
    prog[H_CLASS_OFF] = (uint32_t)(H_WORDS + 3 * g.ninst);
    prog[H_NLOOK] = (uint32_t)p.nlook;
    prog[H_LOOK_DEPTH] = (uint32_t)p.look_depth_max;
    prog[H_ANCHORED] = starts_with_bol(&p, root) ? 1u : 0u;
    prog[H_NAMES_OFF] = 0;
    memcpy(prog + H_WORDS, g.code, 3 * (size_t)g.ninst * sizeof(uint32_t));
    /* a pattern without a class never allocated p.cls */
    if (p.ncls) {
      memcpy(prog + prog[H_CLASS_OFF], p.cls, (size_t)p.ncls * sizeof(uint32_t));
    }
    if (name_words) {
      uint32_t *t = prog + words - name_words;
      prog[H_NAMES_OFF] = (uint32_t)(words - name_words);
      *t++ = (uint32_t)p.nnamed;
      for (int i = 0; i < p.nnames; i += 2 + p.names[i + 1]) {
        size_t n = p.names[i + 1];
        *t++ = p.names[i];
        *t++ = (uint32_t)n;
        memset(t, 0, ((n + 3) / 4) * sizeof(uint32_t));
        memcpy(t, p.names + i + 2, n);
        t += (n + 3) / 4;
      }
    }
    *nwords = words;
  }
  regex_free(g.code);
  regex_free(p.nodes);
  regex_free(p.cls);
  regex_free(p.names);
  return prog;
}

const uint8_t *
regex_group_name(const uint32_t *prog, int group, size_t *len)
{
  uint32_t off = prog[H_NAMES_OFF];
  if (off == 0) return NULL;
  const uint32_t *t = prog + off;
  uint32_t n = *t++;
  for (uint32_t i = 0; i < n; i++) {
    uint32_t g = t[0], l = t[1];
    if ((int)g == group) {
      *len = l;
      return (const uint8_t *)(t + 2);
    }
    t += 2 + (l + 3) / 4;
  }
  return NULL;
}

/* ================================================================
 * The Pike VM. It allocates nothing: the caller supplies a scratch
 * block of regex_scratch_words(prog) words, which one loop over
 * many matches allocates once. The block holds one level per
 * lookaround nesting depth: the main run uses level 0, the sub-match
 * for a lookaround met at level d uses level d + 1.
 * ================================================================ */

typedef struct {
  int n;            /* threads in the list */
  uint32_t *pc;     /* pc per thread */
  int32_t *caps;    /* nsave slots per thread */
  uint32_t *mark;   /* per instruction: the generation that added it */
  uint32_t gen;
} rx_list;

/* what every level of a run shares */
typedef struct {
  const uint32_t *code;
  const uint32_t *cls;
  int ninst;
  int nsave;
  int nlook;
  const uint8_t *s;
  size_t len;
  uint32_t *scratch;
  size_t level_words;   /* scratch words per level */
} rx_shared;

/* one level: the main run or one lookaround sub-match */
typedef struct {
  const rx_shared *sh;
  int depth;
  rx_list a, b;
  int32_t *work;    /* the caps array addthread mutates */
  int32_t *lout;    /* the caps a lookaround sub-match reports */
  uint32_t *stack;  /* explicit follow stack: 2 words per entry */
  uint32_t gen;
  size_t end_at;    /* RX_ANY_END, or the only position LMATCH accepts */
} rx_vm;

#define RX_ANY_END ((size_t)-1)

static bool
class_has(const uint32_t *c, uint32_t ch)
{
  uint32_t n = c[1];
  const uint32_t *r = c + 2;
  for (uint32_t i = 0; i < n; i++) {
    if (ch >= r[2 * i] && ch <= r[2 * i + 1]) return true;
  }
  return false;
}

static bool
class_match(const rx_shared *sh, uint32_t off, uint32_t flags, uint32_t ch)
{
  const uint32_t *c = sh->cls + off;
  bool in = class_has(c, ch);
  if (!in && (flags & RX_ICASE)) {
    uint32_t f = fold(ch), u = unfold(ch);
    in = (f != ch && class_has(c, f)) || (u != ch && class_has(c, u));
  }
  return c[0] ? !in : in;
}

/* \w as a byte: ASCII only, like the class, so a multibyte character
 * is never a word character and its continuation bytes never are */
static inline bool
word_byte(uint8_t b)
{
  return (b >= 'a' && b <= 'z') || (b >= 'A' && b <= 'Z') ||
         (b >= '0' && b <= '9') || b == '_';
}

/* \b: a word character on exactly one side of pos */
static bool
at_word_boundary(const rx_shared *sh, size_t pos)
{
  bool before = pos > 0 && word_byte(sh->s[pos - 1]);
  bool after = pos < sh->len && word_byte(sh->s[pos]);
  return before != after;
}

/* the byte offset n characters before pos, or 0 */
static size_t
back_chars(const uint8_t *s, size_t pos, uint32_t n)
{
  while (n-- && pos > 0) {
    pos--;
    while (pos > 0 && (s[pos] & 0xc0) == 0x80) pos--;
  }
  return pos;
}

static bool run(const rx_shared *sh, int depth, uint32_t pc0, size_t start,
                size_t end_at, bool anchored, const int32_t *init,
                int32_t *out, bool first_only);

/* The test of a LOOK instruction at pos: run the body as a sub-match
 * on the next scratch level. A lookahead is anchored at pos and may
 * end anywhere; a lookbehind starts up to its bounded length before
 * pos and must end exactly at pos. The sub-match starts from this
 * thread's captures, and a positive body with groups reports the
 * ones it set in vm->lout. */
static bool
look_test(rx_vm *vm, const uint32_t *i, size_t pos)
{
  const rx_shared *sh = vm->sh;
  uint32_t body = i[1], y = i[2];
  bool neg = (y & RX_LOOK_NEG) != 0;
  bool first_only = neg || !(y & RX_LOOK_CAPS);
  bool hit;
  if (y & RX_LOOK_BEHIND) {
    size_t from = back_chars(sh->s, pos, y >> RX_LOOK_SHIFT);
    hit = run(sh, vm->depth + 1, body, from, pos, false, vm->work, vm->lout,
              first_only);
  } else {
    hit = run(sh, vm->depth + 1, body, pos, RX_ANY_END, true, vm->work,
              vm->lout, first_only);
  }
  return hit != neg;
}

/* Follow the epsilon edges from pc, adding every consuming
 * instruction reached to the list, in priority order. An explicit
 * stack replaces the textbook recursion: an entry is either
 * (pc, RX_EXPLORE) = explore pc, or (slot, old) = restore a capture
 * slot on the way back out. RX_EXPLORE is neither a position nor
 * the -1 an unset slot restores to. Each instruction is entered at
 * most once per list generation, so the stack is bounded by
 * 2 * ninst entries plus nsave for every LOOK, whose body may set
 * any group. */
#define RX_EXPLORE 0xfffffffeu

static void
addthread(rx_vm *vm, rx_list *l, uint32_t pc0, size_t pos)
{
  const rx_shared *sh = vm->sh;
  uint32_t *sp = vm->stack;
  *sp++ = pc0; *sp++ = RX_EXPLORE;
  while (sp > vm->stack) {
    uint32_t y = *--sp;
    uint32_t x = *--sp;
    if (y != RX_EXPLORE) { vm->work[x] = (int32_t)y; continue; }
    uint32_t pc = x;
    if (l->mark[pc] == l->gen) continue;
    l->mark[pc] = l->gen;
    const uint32_t *i = sh->code + 3 * pc;
    switch (i[0]) {
    case OP_JMP:
      *sp++ = i[1]; *sp++ = RX_EXPLORE;
      break;
    case OP_SPLIT:
      /* push second first so the preferred branch is explored first */
      *sp++ = i[2]; *sp++ = RX_EXPLORE;
      *sp++ = i[1]; *sp++ = RX_EXPLORE;
      break;
    case OP_SAVE:
      *sp++ = i[1]; *sp++ = (uint32_t)vm->work[i[1]];
      vm->work[i[1]] = (int32_t)pos;
      *sp++ = pc + 1; *sp++ = RX_EXPLORE;
      break;
    case OP_BOL:
      if (pos == 0) { *sp++ = pc + 1; *sp++ = RX_EXPLORE; }
      break;
    case OP_EOL:
      if (pos == sh->len) { *sp++ = pc + 1; *sp++ = RX_EXPLORE; }
      break;
    case OP_WORDB:
      if (at_word_boundary(sh, pos)) { *sp++ = pc + 1; *sp++ = RX_EXPLORE; }
      break;
    case OP_NWORDB:
      if (!at_word_boundary(sh, pos)) { *sp++ = pc + 1; *sp++ = RX_EXPLORE; }
      break;
    case OP_LOOK:
      if (!look_test(vm, i, pos)) break;
      if (!(i[2] & RX_LOOK_NEG) && (i[2] & RX_LOOK_CAPS)) {
        /* take the body's captures, restorable like a SAVE */
        for (int k = 0; k < sh->nsave; k++) {
          if (vm->lout[k] == vm->work[k]) continue;
          *sp++ = (uint32_t)k; *sp++ = (uint32_t)vm->work[k];
          vm->work[k] = vm->lout[k];
        }
      }
      *sp++ = pc + 1; *sp++ = RX_EXPLORE;
      break;
    default:
      l->pc[l->n] = pc;
      memcpy(l->caps + (size_t)l->n * (size_t)sh->nsave, vm->work,
             (size_t)sh->nsave * sizeof(int32_t));
      l->n++;
      break;
    }
  }
}

/* Carve a thread list out of the scratch block: pc, caps and mark
 * are ninst, ninst * nsave and ninst words. mark must start below
 * every generation the run will use, so it is zeroed here. */
static uint32_t *
list_init(rx_list *l, uint32_t *scratch, int ninst, int nsave)
{
  l->n = 0;
  l->pc = scratch;
  l->caps = (int32_t *)(scratch + ninst);
  l->mark = scratch + ninst + ninst * nsave;
  memset(l->mark, 0, sizeof(uint32_t) * (size_t)ninst);
  l->gen = 0;
  return l->mark + ninst;
}

/* the scratch one level needs: two thread lists, the work caps, the
 * caps a lookaround reports and the follow stack (2 words per entry,
 * 2 * ninst + 2 + nlook * nsave entries) */
static size_t
level_words(size_t ninst, size_t nsave, size_t nlook)
{
  return 2 * (ninst + ninst * nsave + ninst) + 2 * nsave +
         2 * (2 * ninst + 2 + nlook * nsave);
}

/* one level per lookaround nesting depth, plus the main run */
size_t
regex_scratch_words(const uint32_t *prog)
{
  return level_words(prog[H_NINST], prog[H_NSAVE], prog[H_NLOOK]) *
         (1 + (size_t)prog[H_LOOK_DEPTH]);
}

/* One run of the program from pc0 on scratch level depth. Anchored,
 * threads start only at start; otherwise a fresh thread starts at
 * every position until a match is found, which makes the match
 * leftmost. MATCH and LMATCH end a thread; with end_at set, LMATCH
 * counts only at that position, and the run stops there. init is the
 * nsave capture slots a fresh thread starts from; out receives the
 * winning thread's. first_only stops at the first thread to finish. */
static bool
run(const rx_shared *sh, int depth, uint32_t pc0, size_t start, size_t end_at,
    bool anchored, const int32_t *init, int32_t *out, bool first_only)
{
  rx_vm vm;
  vm.sh = sh;
  vm.depth = depth;
  vm.end_at = end_at;
  uint32_t *sp = sh->scratch + (size_t)depth * sh->level_words;
  sp = list_init(&vm.a, sp, sh->ninst, sh->nsave);
  sp = list_init(&vm.b, sp, sh->ninst, sh->nsave);
  vm.work = (int32_t *)sp;
  sp += sh->nsave;
  vm.lout = (int32_t *)sp;
  sp += sh->nsave;
  vm.stack = sp;
  vm.gen = 0;

  const uint8_t *s = sh->s;
  size_t len = sh->len;
  rx_list *clist = &vm.a, *nlist = &vm.b;
  bool matched = false;
  size_t pos = start;
  clist->gen = ++vm.gen;
  clist->n = 0;

  for (;;) {
    if (!matched && (!anchored || pos == start)) {
      /* a fresh attempt at this position, below every thread that
       * started earlier: that is what makes the match leftmost */
      memcpy(vm.work, init, (size_t)sh->nsave * sizeof(int32_t));
      addthread(&vm, clist, pc0, pos);
    }
    /* no thread is alive: done, unless a later start may still pass
     * an assertion that failed here, such as $ or a lookbehind */
    if (clist->n == 0 && (matched || anchored || pos >= len)) break;

    uint32_t c = 0;
    int w = 0;
    if (pos < len) c = utf8_decode(s + pos, len - pos, &w);

    nlist->gen = ++vm.gen;
    nlist->n = 0;
    for (int t = 0; t < clist->n; t++) {
      uint32_t pc = clist->pc[t];
      const uint32_t *i = sh->code + 3 * pc;
      const int32_t *caps = clist->caps + (size_t)t * (size_t)sh->nsave;
      bool step = false;
      switch (i[0]) {
      case OP_CHAR:
        step = pos < len && (c == i[1] ||
                             ((i[2] & RX_ICASE) && fold(c) == fold(i[1])));
        break;
      case OP_ANY:
        step = pos < len && ((i[2] & RX_DOTALL) || c != '\n');
        break;
      case OP_CLASS:
        step = pos < len && class_match(sh, i[1], i[2], c);
        break;
      case OP_LMATCH:
        if (end_at != RX_ANY_END && pos != end_at) break;
        /* fall through */
      case OP_MATCH:
        memcpy(out, caps, (size_t)sh->nsave * sizeof(int32_t));
        matched = true;
        /* lower-priority threads can only produce worse matches */
        t = clist->n;
        break;
      default:
        break;
      }
      if (matched && first_only) break;
      if (step) {
        memcpy(vm.work, caps, (size_t)sh->nsave * sizeof(int32_t));
        addthread(&vm, nlist, pc + 1, pos + (size_t)w);
      }
    }
    if (matched && first_only) break;
    rx_list *tmp = clist; clist = nlist; nlist = tmp;
    /* at the end of the subject, or of a lookbehind's window, the
     * step above ran with no character: only a finish could fire */
    if (pos >= len || pos >= end_at) break;
    pos += (size_t)w;
  }

  return matched;
}

/* Leftmost-first search from byte offset start. On a match, out[]
 * (nsave slots) holds the capture positions, -1 where a group did
 * not take part. first_only stops at the first thread that reaches
 * MATCH -- enough for is_match, where nobody reads the positions.
 * scratch is regex_scratch_words(prog) words the caller owns; its
 * content on entry does not matter. */
bool
regex_exec(const uint32_t *prog, const uint8_t *s, size_t len, size_t start,
              int32_t *out, bool first_only, uint32_t *scratch)
{
  rx_shared sh;
  sh.code = prog + H_WORDS;
  sh.cls = prog + prog[H_CLASS_OFF];
  sh.ninst = (int)prog[H_NINST];
  sh.nsave = (int)prog[H_NSAVE];
  sh.nlook = (int)prog[H_NLOOK];
  sh.s = s;
  sh.len = len;
  sh.scratch = scratch;
  sh.level_words = level_words(prog[H_NINST], prog[H_NSAVE], prog[H_NLOOK]);
  /* every thread of the main run starts with no group set */
  int32_t init[2 * RX_MAX_GROUPS];
  for (int k = 0; k < sh.nsave; k++) init[k] = -1;
  return run(&sh, 0, 0, start, RX_ANY_END, prog[H_ANCHORED] != 0, init, out,
             first_only);
}
