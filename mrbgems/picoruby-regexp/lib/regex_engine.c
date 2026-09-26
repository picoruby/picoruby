/* regex_engine.c -- the regex engine of the corelib (spec 10.5).
 *
 * A pattern compiles to a small program; the program runs on a Pike
 * VM (Thompson NFA simulation carrying capture positions). The
 * choice is the whole point: matching costs O(input x program) in
 * time and O(program) in memory, with no recursion anywhere, so a
 * regex can never blow the task stack or hold the cooperative
 * scheduler hostage the way a backtracking engine does on (a*)*b.
 * The price is the absence of backreferences and lookaround, which
 * this engine does not pretend to have.
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
#define RX_MAX_GROUPS 16     /* capture groups including group 0 */
#define RX_MAX_REPEAT 256    /* the n in {n,m} */
#define RX_MAX_NODES  4096   /* AST nodes */
#define RX_MAX_CLASS  2048   /* words of class-range storage */
#define RX_INIT       32     /* first size of a work buffer, in items */

/* an unpatched slot in a backpatch chain */
#define RX_NONE 0xffffffffu

/* ---- flags (the public bits, regex_engine.h) ---- */

#define RX_ICASE  REGEX_ICASE
#define RX_DOTALL REGEX_DOTALL

int
regex_flag_bits(const char *flags, size_t len)
{
  int bits = 0;
  for (size_t i = 0; i < len; i++) {
    if (flags[i] == 'i') bits |= (int)RX_ICASE;
    else if (flags[i] == 'm') bits |= (int)RX_DOTALL;
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
  H_FLAGS,
  H_CLASS_OFF,  /* word offset of the class table */
  H_WORDS       /* header size */
};

enum {
  OP_CHAR = 1,  /* x = codepoint */
  OP_ANY,       /* any codepoint (newline per DOTALL) */
  OP_CLASS,     /* x = class table offset */
  OP_SPLIT,     /* x preferred, y second */
  OP_JMP,       /* x */
  OP_SAVE,      /* x = slot */
  OP_BOL,
  OP_EOL,
  OP_MATCH
};

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
  N_CHAR,   /* x = codepoint */
  N_ANY,
  N_CLASS,  /* x = class offset */
  N_CAT,    /* a, b */
  N_ALT,    /* a, b */
  N_REP,    /* a; x = min, y = max (-1 = unbounded), b = greedy */
  N_GROUP,  /* a; x = group index */
  N_BOL,
  N_EOL
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
  int ngroups;        /* including group 0 */
  int flags;
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

static int
parse_atom(rx_parser *p, int depth)
{
  uint32_t c = next(p);
  switch (c) {
  case '(': {
    bool capture = true;
    if (p->pos + 1 < p->len && p->src[p->pos] == '?') {
      if (p->src[p->pos + 1] == ':') { p->pos += 2; capture = false; }
      else { perr(p, "unsupported group syntax"); return 0; }
    }
    int idx = 0;
    if (capture) {
      if (p->ngroups >= RX_MAX_GROUPS) { perr(p, "too many groups"); return 0; }
      idx = p->ngroups++;
    }
    int inner = parse_alt(p, depth + 1);
    if (p->err) return 0;
    if (at_end(p) || next(p) != ')') { perr(p, "missing )"); return 0; }
    return capture ? node_new(p, N_GROUP, inner, 0, idx, 0) : inner;
  }
  case ')': perr(p, "unmatched )"); return 0;
  case '[': return node_new(p, N_CLASS, 0, 0, parse_class(p), 0);
  case '.': return node_new(p, N_ANY, 0, 0, 0, 0);
  case '^': return node_new(p, N_BOL, 0, 0, 0, 0);
  case '$': return node_new(p, N_EOL, 0, 0, 0, 0);
  case '*': case '+': case '?': perr(p, "nothing to repeat"); return 0;
  case '{': return node_new(p, N_CHAR, 0, 0, '{', 0);
  case '\\': {
    if (at_end(p)) { perr(p, "trailing backslash"); return 0; }
    uint32_t e = next(p);
    uint32_t lit;
    switch (e) {
    case 'A': return node_new(p, N_BOL, 0, 0, 0, 0);
    case 'z': case 'Z': return node_new(p, N_EOL, 0, 0, 0, 0);
    case 'd': case 'w': case 's': {
      int off = class_begin(p, false);
      class_add_escape(p, off, e);
      return node_new(p, N_CLASS, 0, 0, off, 0);
    }
    case 'D': case 'W': case 'S':
      return node_new(p, N_CLASS, 0, 0, negated_escape_class(p, e + 32), 0);
    default:
      if (literal_escape(e, &lit)) return node_new(p, N_CHAR, 0, 0, (int32_t)lit, 0);
      if (e == 'b' || e == 'B') perr(p, "word boundaries are not supported");
      else if (e >= '1' && e <= '9') perr(p, "backreferences are not supported");
      else perr(p, "unsupported escape");
      return 0;
    }
  }
  default:
    return node_new(p, N_CHAR, 0, 0, (int32_t)c, 0);
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

static int
parse_repeat(rx_parser *p, int depth)
{
  int atom = parse_atom(p, depth);
  if (p->err) return 0;
  for (;;) {
    if (at_end(p)) return atom;
    uint32_t c = peek(p);
    int min, max;
    if (c == '*') { next(p); min = 0; max = -1; }
    else if (c == '+') { next(p); min = 1; max = -1; }
    else if (c == '?') { next(p); min = 0; max = 1; }
    else if (c == '{') {
      next(p);
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
    if (n->t == N_REP) { perr(p, "nested repetition"); return 0; }
    bool greedy = true;
    if (!at_end(p) && peek(p) == '?') { next(p); greedy = false; }
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
  while (!at_end(p) && !p->err) {
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
    case N_CHAR:  emit(g, OP_CHAR, (uint32_t)nd->x, 0); break;
    case N_ANY:   emit(g, OP_ANY, 0, 0); break;
    case N_CLASS: emit(g, OP_CLASS, (uint32_t)nd->x, 0); break;
    case N_BOL:   emit(g, OP_BOL, 0, 0); break;
    case N_EOL:   emit(g, OP_EOL, 0, 0); break;
    case N_GROUP:
      emit(g, OP_SAVE, (uint32_t)(2 * nd->x), 0);
      gen(g, p, nd->a);
      emit(g, OP_SAVE, (uint32_t)(2 * nd->x + 1), 0);
      break;
    case N_REP: {
      bool greedy = nd->b != 0;
      for (int i = 0; i < nd->x; i++) gen(g, p, nd->a);
      if (nd->y < 0) gen_star(g, p, nd->a, greedy);
      else gen_optional(g, p, nd->a, nd->y - nd->x, greedy);
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

  rx_gen g;
  memset(&g, 0, sizeof g);
  uint32_t *prog = NULL;
  if (!p.err) {
    emit(&g, OP_SAVE, 0, 0);
    gen(&g, &p, root);
    emit(&g, OP_SAVE, 1, 0);
    emit(&g, OP_MATCH, 0, 0);
    if (g.err) { *err = g.err; *err_at = 0; }
  } else {
    *err = p.err; *err_at = p.err_at;
  }

  size_t words = H_WORDS + 3 * (size_t)g.ninst + (size_t)p.ncls;
  if (!*err) {
    prog = regex_malloc(words * sizeof(uint32_t));
    if (!prog) { *err = "out of memory"; *err_at = 0; }
  }
  if (prog) {
    prog[H_NINST] = (uint32_t)g.ninst;
    prog[H_NSAVE] = (uint32_t)(2 * p.ngroups);
    prog[H_FLAGS] = (uint32_t)flags;
    prog[H_CLASS_OFF] = (uint32_t)(H_WORDS + 3 * g.ninst);
    memcpy(prog + H_WORDS, g.code, 3 * (size_t)g.ninst * sizeof(uint32_t));
    /* a pattern without a class never allocated p.cls */
    if (p.ncls) {
      memcpy(prog + prog[H_CLASS_OFF], p.cls, (size_t)p.ncls * sizeof(uint32_t));
    }
    *nwords = words;
  }
  regex_free(g.code);
  regex_free(p.nodes);
  regex_free(p.cls);
  return prog;
}

/* ================================================================
 * The Pike VM. It allocates nothing: the caller supplies a scratch
 * block of regex_scratch_words(prog) words, which one loop over
 * many matches allocates once.
 * ================================================================ */

typedef struct {
  int n;            /* threads in the list */
  uint32_t *pc;     /* pc per thread */
  int32_t *caps;    /* nsave slots per thread */
  uint32_t *mark;   /* per instruction: the generation that added it */
  uint32_t gen;
} rx_list;

typedef struct {
  const uint32_t *prog;
  const uint32_t *code;
  const uint32_t *cls;
  int ninst;
  int nsave;
  uint32_t flags;
  const uint8_t *s;
  size_t len;
  rx_list a, b;
  int32_t *work;    /* the caps array addthread mutates */
  uint32_t *stack;  /* explicit follow stack: 2 words per entry */
  uint32_t gen;
} rx_vm;

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
class_match(const rx_vm *vm, uint32_t off, uint32_t ch)
{
  const uint32_t *c = vm->cls + off;
  bool in = class_has(c, ch);
  if (!in && (vm->flags & RX_ICASE)) {
    uint32_t f = fold(ch), u = unfold(ch);
    in = (f != ch && class_has(c, f)) || (u != ch && class_has(c, u));
  }
  return c[0] ? !in : in;
}

/* Follow the epsilon edges from pc, adding every consuming
 * instruction reached to the list, in priority order. An explicit
 * stack replaces the textbook recursion: an entry is either
 * (pc, RX_EXPLORE) = explore pc, or (slot, old) = restore a capture
 * slot on the way back out. RX_EXPLORE is neither a position nor
 * the -1 an unset slot restores to. Each instruction is entered at
 * most once per list generation, so the stack is bounded by
 * 2 * ninst. */
#define RX_EXPLORE 0xfffffffeu

static void
addthread(rx_vm *vm, rx_list *l, uint32_t pc0, size_t pos)
{
  uint32_t *sp = vm->stack;
  *sp++ = pc0; *sp++ = RX_EXPLORE;
  while (sp > vm->stack) {
    uint32_t y = *--sp;
    uint32_t x = *--sp;
    if (y != RX_EXPLORE) { vm->work[x] = (int32_t)y; continue; }
    uint32_t pc = x;
    if (l->mark[pc] == l->gen) continue;
    l->mark[pc] = l->gen;
    const uint32_t *i = vm->code + 3 * pc;
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
      if (pos == vm->len) { *sp++ = pc + 1; *sp++ = RX_EXPLORE; }
      break;
    default:
      l->pc[l->n] = pc;
      memcpy(l->caps + (size_t)l->n * (size_t)vm->nsave, vm->work,
             (size_t)vm->nsave * sizeof(int32_t));
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

/* the scratch a run over prog needs: two thread lists, the work caps
 * and the follow stack (2 words per entry, 2 * ninst + 2 entries) */
size_t
regex_scratch_words(const uint32_t *prog)
{
  size_t ninst = prog[H_NINST], nsave = prog[H_NSAVE];
  return 2 * (ninst + ninst * nsave + ninst) + nsave + 2 * (2 * ninst + 2);
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
  rx_vm vm;
  memset(&vm, 0, sizeof vm);
  vm.prog = prog;
  vm.ninst = (int)prog[H_NINST];
  vm.nsave = (int)prog[H_NSAVE];
  vm.flags = prog[H_FLAGS];
  vm.code = prog + H_WORDS;
  vm.cls = prog + prog[H_CLASS_OFF];
  vm.s = s;
  vm.len = len;
  uint32_t *sp = scratch;
  sp = list_init(&vm.a, sp, vm.ninst, vm.nsave);
  sp = list_init(&vm.b, sp, vm.ninst, vm.nsave);
  vm.work = (int32_t *)sp;
  sp += vm.nsave;
  vm.stack = sp;
  vm.gen = 0;

  rx_list *clist = &vm.a, *nlist = &vm.b;
  bool matched = false;
  size_t pos = start;
  clist->gen = ++vm.gen;
  clist->n = 0;

  for (;;) {
    if (!matched) {
      /* a fresh attempt at this position, below every thread that
       * started earlier: that is what makes the match leftmost */
      for (int k = 0; k < vm.nsave; k++) vm.work[k] = -1;
      addthread(&vm, clist, 0, pos);
    }
    if (clist->n == 0) break;

    uint32_t c = 0;
    int w = 0;
    if (pos < len) c = utf8_decode(s + pos, len - pos, &w);

    nlist->gen = ++vm.gen;
    nlist->n = 0;
    for (int t = 0; t < clist->n; t++) {
      uint32_t pc = clist->pc[t];
      const uint32_t *i = vm.code + 3 * pc;
      const int32_t *caps = clist->caps + (size_t)t * (size_t)vm.nsave;
      bool step = false;
      switch (i[0]) {
      case OP_CHAR:
        step = pos < len && (c == i[1] ||
                             ((vm.flags & RX_ICASE) && fold(c) == fold(i[1])));
        break;
      case OP_ANY:
        step = pos < len && ((vm.flags & RX_DOTALL) || c != '\n');
        break;
      case OP_CLASS:
        step = pos < len && class_match(&vm, i[1], c);
        break;
      case OP_MATCH:
        memcpy(out, caps, (size_t)vm.nsave * sizeof(int32_t));
        matched = true;
        /* lower-priority threads can only produce worse matches */
        t = clist->n;
        break;
      default:
        break;
      }
      if (matched && first_only) break;
      if (step) {
        memcpy(vm.work, caps, (size_t)vm.nsave * sizeof(int32_t));
        addthread(&vm, nlist, pc + 1, pos + (size_t)w);
      }
    }
    if (matched && first_only) break;
    rx_list *tmp = clist; clist = nlist; nlist = tmp;
    if (pos >= len) {
      /* the final step ran with no character: only MATCH could fire */
      if (clist->n == 0 || matched) break;
      /* threads still alive want a character that is not there */
      break;
    }
    pos += (size_t)w;
  }

  return matched;
}
