/*
** string-bitops.c - Basic bit operations for String
**
** Port of CRuby [Feature #22118] "Introduce Basic Bit Operations into
** String" for FemtoRuby (mruby/c).  On the mruby VM, the same API is
** provided by the mruby-string-bitops mgem inside picoruby-mruby.
**
** This file holds the VM-independent bulk kernels.  They process one
** machine word at a time; the word type follows the pointer width, so
** 32-bit targets use 32-bit words and avoid emulated 64-bit
** arithmetic.
*/

#include <string.h>
#include <stdint.h>
#include <limits.h>

#if defined(UINTPTR_MAX) && UINTPTR_MAX > 0xFFFFFFFFul
typedef uint64_t bitop_word;
# define BITOP_WORD_SIZE 8
#else
typedef uint32_t bitop_word;
# define BITOP_WORD_SIZE 4
#endif
#define BITOP_WORD_BITS (BITOP_WORD_SIZE * 8)

#ifndef __has_builtin
# define __has_builtin(x) 0
#endif

#if defined(__GNUC__) || __has_builtin(__builtin_popcount)
static unsigned int
bitop_popcount(bitop_word word)
{
#if BITOP_WORD_SIZE == 8
  return (unsigned int)__builtin_popcountll((unsigned long long)word);
#elif UINT_MAX >= 0xFFFFFFFFul
  /* int holds a 32-bit word; avoids the 64-bit helper where long is 64-bit */
  return (unsigned int)__builtin_popcount((unsigned int)word);
#else
  return (unsigned int)__builtin_popcountl((unsigned long)word);
#endif
}
#else
static unsigned int
bitop_popcount(bitop_word word)
{
  /* Generic SWAR popcount; the constants adapt to the word width. */
  const bitop_word m1 = (bitop_word)~(bitop_word)0 / 3;    /* 0x55... */
  const bitop_word m2 = (bitop_word)~(bitop_word)0 / 5;    /* 0x33... */
  const bitop_word m4 = (bitop_word)~(bitop_word)0 / 17;   /* 0x0f... */
  const bitop_word h01 = (bitop_word)~(bitop_word)0 / 255; /* 0x01... */

  word -= (word >> 1) & m1;
  word = (word & m2) + ((word >> 2) & m2);
  word = (word + (word >> 4)) & m4;
  return (unsigned int)((word * h01) >> (BITOP_WORD_BITS - 8));
}
#endif

/*
 * memcpy-based word loops, safe for any alignment.  They are the
 * whole bulk path on non-GNU compilers, and the unaligned fallback
 * on GNU-compatible ones.  The trailing byte loop of each kernel
 * only handles the tail.
 */
#define BITOP_UNARY_MEMCPY_LOOP(dst, src, len, off, expr_word)               \
  {                                                                          \
    int aligned_end_ = (len) & ~(BITOP_WORD_SIZE - 1);                       \
    for (; (off) < aligned_end_; (off) += BITOP_WORD_SIZE) {                 \
      bitop_word w_;                                                         \
      memcpy(&w_, (src) + (off), BITOP_WORD_SIZE);                           \
      w_ = expr_word(w_);                                                    \
      memcpy((dst) + (off), &w_, BITOP_WORD_SIZE);                           \
    }                                                                        \
  }

#define BITOP_BINARY_MEMCPY_LOOP(dst, lhs, rhs, len, off, expr_word)         \
  {                                                                          \
    int aligned_end_ = (len) & ~(BITOP_WORD_SIZE - 1);                       \
    for (; (off) < aligned_end_; (off) += BITOP_WORD_SIZE) {                 \
      bitop_word l_, r_;                                                     \
      memcpy(&l_, (lhs) + (off), BITOP_WORD_SIZE);                           \
      memcpy(&r_, (rhs) + (off), BITOP_WORD_SIZE);                           \
      l_ = expr_word(l_, r_);                                                \
      memcpy((dst) + (off), &l_, BITOP_WORD_SIZE);                           \
    }                                                                        \
  }

/*
 * On GNU-compatible compilers, word-aligned buffers are processed
 * through a word pointer; the may_alias type keeps that free of
 * strict-aliasing issues.  Targets without unaligned load support
 * (e.g. Cortex-M0+) still get true word loads this way, which a bare
 * memcpy cannot guarantee.  Unaligned buffers fall back to the memcpy
 * word loops above.
 */
#if defined(__GNUC__)
typedef bitop_word __attribute__((__may_alias__)) bitop_word_alias;
# define BITOP_ALIGNED(ptrbits) (((ptrbits) & (BITOP_WORD_SIZE - 1)) == 0)

#define BITOP_DEFINE_UNARY_KERNEL(name, expr_word, expr_byte)                \
static void                                                                  \
name(uint8_t *dst, const uint8_t *src, int len)                              \
{                                                                            \
  int off = 0;                                                               \
  if (BITOP_ALIGNED((uintptr_t)dst | (uintptr_t)src)) {                      \
    bitop_word_alias *dw = (bitop_word_alias*)dst;                           \
    const bitop_word_alias *sw = (const bitop_word_alias*)src;               \
    int words = len / BITOP_WORD_SIZE;                                       \
    int i = 0;                                                               \
    for (; i + 4 <= words; i += 4) {                                         \
      bitop_word s0 = sw[i], s1 = sw[i+1], s2 = sw[i+2], s3 = sw[i+3];       \
      dw[i]   = expr_word(s0);                                               \
      dw[i+1] = expr_word(s1);                                               \
      dw[i+2] = expr_word(s2);                                               \
      dw[i+3] = expr_word(s3);                                               \
    }                                                                        \
    for (; i < words; i++) {                                                 \
      dw[i] = expr_word(sw[i]);                                              \
    }                                                                        \
    off = words * BITOP_WORD_SIZE;                                           \
  }                                                                          \
  else                                                                       \
    BITOP_UNARY_MEMCPY_LOOP(dst, src, len, off, expr_word)                   \
  for (; off < len; off++) {                                                 \
    dst[off] = expr_byte(src[off]);                                          \
  }                                                                          \
}

#define BITOP_DEFINE_BINARY_KERNEL(name, expr_word, expr_byte)               \
static void                                                                  \
name(uint8_t *dst, const uint8_t *lhs, const uint8_t *rhs, int len)          \
{                                                                            \
  int off = 0;                                                               \
  if (BITOP_ALIGNED((uintptr_t)dst | (uintptr_t)lhs | (uintptr_t)rhs)) {     \
    bitop_word_alias *dw = (bitop_word_alias*)dst;                           \
    const bitop_word_alias *lw = (const bitop_word_alias*)lhs;               \
    const bitop_word_alias *rw = (const bitop_word_alias*)rhs;               \
    int words = len / BITOP_WORD_SIZE;                                       \
    int i = 0;                                                               \
    for (; i + 4 <= words; i += 4) {                                         \
      bitop_word l0 = lw[i], l1 = lw[i+1], l2 = lw[i+2], l3 = lw[i+3];       \
      bitop_word r0 = rw[i], r1 = rw[i+1], r2 = rw[i+2], r3 = rw[i+3];       \
      dw[i]   = expr_word(l0, r0);                                           \
      dw[i+1] = expr_word(l1, r1);                                           \
      dw[i+2] = expr_word(l2, r2);                                           \
      dw[i+3] = expr_word(l3, r3);                                           \
    }                                                                        \
    for (; i < words; i++) {                                                 \
      dw[i] = expr_word(lw[i], rw[i]);                                       \
    }                                                                        \
    off = words * BITOP_WORD_SIZE;                                           \
  }                                                                          \
  else                                                                       \
    BITOP_BINARY_MEMCPY_LOOP(dst, lhs, rhs, len, off, expr_word)             \
  for (; off < len; off++) {                                                 \
    dst[off] = expr_byte(lhs[off], rhs[off]);                                \
  }                                                                          \
}

#else /* generic compilers: memcpy word loops only */

#define BITOP_DEFINE_UNARY_KERNEL(name, expr_word, expr_byte)                \
static void                                                                  \
name(uint8_t *dst, const uint8_t *src, int len)                              \
{                                                                            \
  int off = 0;                                                               \
  BITOP_UNARY_MEMCPY_LOOP(dst, src, len, off, expr_word)                     \
  for (; off < len; off++) {                                                 \
    dst[off] = expr_byte(src[off]);                                          \
  }                                                                          \
}

#define BITOP_DEFINE_BINARY_KERNEL(name, expr_word, expr_byte)               \
static void                                                                  \
name(uint8_t *dst, const uint8_t *lhs, const uint8_t *rhs, int len)          \
{                                                                            \
  int off = 0;                                                               \
  BITOP_BINARY_MEMCPY_LOOP(dst, lhs, rhs, len, off, expr_word)               \
  for (; off < len; off++) {                                                 \
    dst[off] = expr_byte(lhs[off], rhs[off]);                                \
  }                                                                          \
}

#endif

#define BITOP_NOT_WORD(x)    (~(x))
#define BITOP_NOT_BYTE(x)    ((uint8_t)~(x))
#define BITOP_AND_WORD(x, y) ((x) & (y))
#define BITOP_AND_BYTE(x, y) ((uint8_t)((x) & (y)))
#define BITOP_OR_WORD(x, y)  ((x) | (y))
#define BITOP_OR_BYTE(x, y)  ((uint8_t)((x) | (y)))
#define BITOP_XOR_WORD(x, y) ((x) ^ (y))
#define BITOP_XOR_BYTE(x, y) ((uint8_t)((x) ^ (y)))

BITOP_DEFINE_UNARY_KERNEL(bitop_not_kernel, BITOP_NOT_WORD, BITOP_NOT_BYTE)
BITOP_DEFINE_BINARY_KERNEL(bitop_and_kernel, BITOP_AND_WORD, BITOP_AND_BYTE)
BITOP_DEFINE_BINARY_KERNEL(bitop_or_kernel, BITOP_OR_WORD, BITOP_OR_BYTE)
BITOP_DEFINE_BINARY_KERNEL(bitop_xor_kernel, BITOP_XOR_WORD, BITOP_XOR_BYTE)

/*
 * The uint32_t count cannot overflow: it would need a 512MiB string,
 * far beyond MRBC_STRING_SIZE_T.  Whether the count also fits in the
 * VM integer type (it does not under MRBC_INT16) is checked by the
 * caller when boxing the return value.
 */
static uint32_t
bitop_count_bits(const uint8_t *ptr, int len)
{
  uint32_t count = 0;
  int off = 0;

#if defined(__GNUC__)
  if (BITOP_ALIGNED((uintptr_t)ptr)) {
    const bitop_word_alias *pw = (const bitop_word_alias*)ptr;
    int words = len / BITOP_WORD_SIZE;
    int i = 0;
    for (; i + 4 <= words; i += 4) {
      count += bitop_popcount(pw[i]);
      count += bitop_popcount(pw[i+1]);
      count += bitop_popcount(pw[i+2]);
      count += bitop_popcount(pw[i+3]);
    }
    for (; i < words; i++) {
      count += bitop_popcount(pw[i]);
    }
    off = words * BITOP_WORD_SIZE;
  }
  else
#endif
  {
    int aligned_end = len & ~(BITOP_WORD_SIZE - 1);
    for (; off < aligned_end; off += BITOP_WORD_SIZE) {
      bitop_word w;
      memcpy(&w, ptr + off, BITOP_WORD_SIZE);
      count += bitop_popcount(w);
    }
  }
  /* Pack the remaining bytes into one word and popcount it once. */
  if (off < len) {
    bitop_word w = 0;
    unsigned int shift = 0;
    for (; off < len; off++, shift += 8) {
      w |= (bitop_word)ptr[off] << shift;
    }
    count += bitop_popcount(w);
  }
  return count;
}

#if defined(PICORB_VM_MRUBY)

#error "picoruby-string-bitops does not support the mruby VM. Use mruby-string-bitops instead"

#elif defined(PICORB_VM_MRUBYC)

#include "mrubyc/string-bitops.c"

#endif
