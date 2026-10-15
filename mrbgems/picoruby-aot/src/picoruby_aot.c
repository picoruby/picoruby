#include <math.h>
#include <string.h>
#include "picoruby_aot.h"
#ifdef MRB_USE_BIGINT
#include <mruby/internal.h>
#endif

static void
aot_overflow(mrb_state *mrb)
{
  mrb_raise(mrb, E_RANGE_ERROR, "picoruby-aot: Integer overflow in compiled method");
}

aot_int
aot_unbox_int(mrb_state *mrb, mrb_value v)
{
  if (mrb_integer_p(v)) return (aot_int)mrb_integer(v);
#ifdef MRB_USE_BIGINT
  /* A Bignum that fits the 64-bit slot; RangeError past it. */
  if (mrb_bigint_p(v)) return mrb_bint_as_int64(mrb, v);
#endif
  mrb_raisef(mrb, E_TYPE_ERROR, "picoruby-aot: expected Integer, got %Y", v);
  return 0;
}

mrb_value
aot_box_int(mrb_state *mrb, aot_int i)
{
#if MRB_INT_BIT < 64
  if (i < (aot_int)MRB_INT_MIN || (aot_int)MRB_INT_MAX < i) {
#ifdef MRB_USE_BIGINT
    return mrb_bint_new_int64(mrb, i);
#else
    aot_overflow(mrb);
#endif
  }
#endif
  return mrb_int_value(mrb, (mrb_int)i);
}

int
aot_bint_cmp_int(mrb_state *mrb, mrb_value v, aot_int n)
{
#ifdef MRB_USE_BIGINT
  mrb_int c = mrb_bint_cmp(mrb, v, aot_box_int(mrb, n));
  return c < 0 ? -1 : (c > 0 ? 1 : 0);
#else
  (void)mrb; (void)v; (void)n;
  return 1;
#endif
}

mrb_float
aot_unbox_float(mrb_state *mrb, mrb_value v)
{
  if (mrb_float_p(v)) return mrb_float(v);
  mrb_raisef(mrb, E_TYPE_ERROR, "picoruby-aot: expected Float, got %Y", v);
  return 0.0;
}

mrb_bool
aot_unbox_bool(mrb_state *mrb, mrb_value v)
{
  if (mrb_true_p(v)) return TRUE;
  if (mrb_false_p(v)) return FALSE;
  mrb_raisef(mrb, E_TYPE_ERROR, "picoruby-aot: expected true or false, got %Y", v);
  return FALSE;
}

/* Checked 64-bit arithmetic: the compiler builtins where they exist, the
   same tests in plain C elsewhere. */
#if defined(__GNUC__) || defined(__clang__)
#define aot_add_overflow(a, b, r) __builtin_add_overflow((a), (b), (r))
#define aot_sub_overflow(a, b, r) __builtin_sub_overflow((a), (b), (r))
#define aot_mul_overflow(a, b, r) __builtin_mul_overflow((a), (b), (r))
#else
static inline int
aot_add_overflow(aot_int a, aot_int b, aot_int *r)
{
  uint64_t x = (uint64_t)a, y = (uint64_t)b, z = x + y;
  *r = (aot_int)z;
  return (int)(((x ^ z) & (y ^ z)) >> 63);
}

static inline int
aot_sub_overflow(aot_int a, aot_int b, aot_int *r)
{
  uint64_t x = (uint64_t)a, y = (uint64_t)b, z = x - y;
  *r = (aot_int)z;
  return (int)(((x ^ y) & (x ^ z)) >> 63);
}

static inline int
aot_mul_overflow(aot_int a, aot_int b, aot_int *r)
{
  if (a == 0 || b == 0) { *r = 0; return 0; }
  if (a > 0) {
    if (b > 0 ? a > AOT_INT_MAX / b : b < AOT_INT_MIN / a) return 1;
  }
  else {
    if (b > 0 ? a < AOT_INT_MIN / b : a < AOT_INT_MAX / b) return 1;
  }
  *r = a * b;
  return 0;
}
#endif

aot_int
aot_int_add(mrb_state *mrb, aot_int a, aot_int b)
{
  aot_int r;
  if (aot_add_overflow(a, b, &r)) aot_overflow(mrb);
  return r;
}

aot_int
aot_int_sub(mrb_state *mrb, aot_int a, aot_int b)
{
  aot_int r;
  if (aot_sub_overflow(a, b, &r)) aot_overflow(mrb);
  return r;
}

aot_int
aot_int_mul(mrb_state *mrb, aot_int a, aot_int b)
{
  aot_int r;
  if (aot_mul_overflow(a, b, &r)) aot_overflow(mrb);
  return r;
}

aot_int
aot_int_div(mrb_state *mrb, aot_int a, aot_int b)
{
  aot_int q;
  if (b == 0) mrb_raise(mrb, E_ZERODIV_ERROR, "divided by 0");
  if (a == AOT_INT_MIN && b == -1) aot_overflow(mrb);
  q = a / b;
  if ((a % b != 0) && ((a < 0) != (b < 0))) q--;
  return q;
}

aot_int
aot_int_mod(mrb_state *mrb, aot_int a, aot_int b)
{
  aot_int m;
  if (b == 0) mrb_raise(mrb, E_ZERODIV_ERROR, "divided by 0");
  if (b == -1) return 0;
  m = a % b;
  if (m != 0 && ((m < 0) != (b < 0))) m += b;
  return m;
}

aot_int
aot_int_neg(mrb_state *mrb, aot_int a)
{
  if (a == AOT_INT_MIN) aot_overflow(mrb);
  return -a;
}

aot_int
aot_int_lshift(mrb_state *mrb, aot_int a, aot_int b)
{
  if (b < 0) return aot_int_rshift(mrb, a, -b);
  if (b >= AOT_INT_BIT) {
    if (a == 0) return 0;
    aot_overflow(mrb);
  }
  if (a > 0) {
    if (a > (AOT_INT_MAX >> b)) aot_overflow(mrb);
  }
  else if (a < 0) {
    if (a < (AOT_INT_MIN >> b)) aot_overflow(mrb);
  }
  return (aot_int)((uint64_t)a << b);
}

aot_int
aot_int_rshift(mrb_state *mrb, aot_int a, aot_int b)
{
  if (b < 0) return aot_int_lshift(mrb, a, -b);
  if (b >= AOT_INT_BIT) return a < 0 ? -1 : 0;
  return a >> b;
}

aot_int
aot_int_pow(mrb_state *mrb, aot_int a, aot_int b)
{
  aot_int r = 1;
  if (b < 0) mrb_raise(mrb, E_RANGE_ERROR, "picoruby-aot: negative exponent in compiled method");
  while (b > 0) {
    if (b & 1) r = aot_int_mul(mrb, r, a);
    b >>= 1;
    if (b > 0) a = aot_int_mul(mrb, a, a);
  }
  return r;
}

mrb_float
aot_float_mod(mrb_float a, mrb_float b)
{
  mrb_float m = fmod(a, b);
  if (m != 0.0 && ((m < 0) != (b < 0))) m += b;
  return m;
}

aot_int
aot_float_to_i(mrb_state *mrb, mrb_float f)
{
  if (isnan(f) || isinf(f)) {
    mrb_raise(mrb, E_FLOATDOMAIN_ERROR, "picoruby-aot: Float out of Integer range");
  }
  /* 2^63 is exact as a double; AOT_INT_MAX rounds up to it. */
  if (f >= 9223372036854775808.0 || f < -9223372036854775808.0) aot_overflow(mrb);
  return (aot_int)f;
}

/* ---- builtins the guarded helpers stand in for ---------------------- */

/* The core methods recorded at gem init. A helper answers in place only
   while the receiver's method still resolves to the recorded one. */
static struct {
  mrb_func_t ary_lshift, ary_size, ary_length, ary_empty_p, ary_push, ary_pop,
             ary_shift, ary_first, ary_last, ary_clear;
  mrb_func_t hash_key_p, hash_size, hash_delete;
} aot_bi;

static mrb_func_t
aot_builtin_func(mrb_state *mrb, struct RClass *c, mrb_sym mid)
{
  mrb_method_t m = mrb_method_search_vm(mrb, &c, mid);
  if (MRB_METHOD_UNDEF_P(m) || !MRB_METHOD_CFUNC_P(m)) return NULL;
  return MRB_METHOD_CFUNC(m);
}

static void
aot_builtins_init(mrb_state *mrb)
{
  struct RClass *a = mrb->array_class;
  struct RClass *h = mrb->hash_class;
  aot_bi.ary_lshift = aot_builtin_func(mrb, a, MRB_OPSYM(lshift));
  aot_bi.ary_size = aot_builtin_func(mrb, a, MRB_SYM(size));
  aot_bi.ary_length = aot_builtin_func(mrb, a, MRB_SYM(length));
  aot_bi.ary_empty_p = aot_builtin_func(mrb, a, MRB_SYM_Q(empty));
  aot_bi.ary_push = aot_builtin_func(mrb, a, MRB_SYM(push));
  aot_bi.ary_pop = aot_builtin_func(mrb, a, MRB_SYM(pop));
  aot_bi.ary_shift = aot_builtin_func(mrb, a, MRB_SYM(shift));
  aot_bi.ary_first = aot_builtin_func(mrb, a, MRB_SYM(first));
  aot_bi.ary_last = aot_builtin_func(mrb, a, MRB_SYM(last));
  aot_bi.ary_clear = aot_builtin_func(mrb, a, MRB_SYM(clear));
  aot_bi.hash_key_p = aot_builtin_func(mrb, h, MRB_SYM_Q(key));
  aot_bi.hash_size = aot_builtin_func(mrb, h, MRB_SYM(size));
  aot_bi.hash_delete = aot_builtin_func(mrb, h, MRB_SYM(delete));
}

/* The receiver's method is still the recorded builtin (never when the
   builtin was not a C function at init, or has been replaced since). */
static inline mrb_bool
aot_builtin_p(mrb_state *mrb, mrb_value recv, mrb_sym mid, mrb_func_t f)
{
  return f != NULL && aot_method_is(mrb, recv, mid, f);
}

/* ---- guarded operators on boxed values ------------------------------ */

/* The VM fallbacks name their method by presym: a compile-time constant,
   no symbol table lookup on the slow path. */
static mrb_value
aot_send1(mrb_state *mrb, mrb_value a, mrb_sym op, mrb_value b)
{
  return mrb_funcall_argv(mrb, a, op, 1, &b);
}

#define AOT_BOTH_INT(a, b) (mrb_integer_p(a) && mrb_integer_p(b))
#define AOT_NUM_P(v) (mrb_integer_p(v) || mrb_float_p(v))
#define AOT_AS_F(v) (mrb_integer_p(v) ? (mrb_float)mrb_integer(v) : mrb_float(v))

mrb_value
aot_add(mrb_state *mrb, mrb_value a, mrb_value b)
{
  mrb_int r;
  if (AOT_BOTH_INT(a, b) && !mrb_int_add_overflow(mrb_integer(a), mrb_integer(b), &r)) {
    return mrb_int_value(mrb, r);
  }
  if (mrb_float_p(a) && AOT_NUM_P(b)) return mrb_float_value(mrb, mrb_float(a) + AOT_AS_F(b));
  if (mrb_string_p(a) && mrb_string_p(b)) return mrb_str_plus(mrb, a, b);
  return aot_send1(mrb, a, MRB_OPSYM(add), b);
}

mrb_value
aot_sub(mrb_state *mrb, mrb_value a, mrb_value b)
{
  mrb_int r;
  if (AOT_BOTH_INT(a, b) && !mrb_int_sub_overflow(mrb_integer(a), mrb_integer(b), &r)) {
    return mrb_int_value(mrb, r);
  }
  if (mrb_float_p(a) && AOT_NUM_P(b)) return mrb_float_value(mrb, mrb_float(a) - AOT_AS_F(b));
  return aot_send1(mrb, a, MRB_OPSYM(sub), b);
}

mrb_value
aot_mul(mrb_state *mrb, mrb_value a, mrb_value b)
{
  mrb_int r;
  if (AOT_BOTH_INT(a, b) && !mrb_int_mul_overflow(mrb_integer(a), mrb_integer(b), &r)) {
    return mrb_int_value(mrb, r);
  }
  if (mrb_float_p(a) && AOT_NUM_P(b)) return mrb_float_value(mrb, mrb_float(a) * AOT_AS_F(b));
  return aot_send1(mrb, a, MRB_OPSYM(mul), b);
}

mrb_value
aot_div(mrb_state *mrb, mrb_value a, mrb_value b)
{
  if (AOT_BOTH_INT(a, b) && mrb_integer(b) != 0 && !(mrb_integer(a) == MRB_INT_MIN && mrb_integer(b) == -1)) {
    return mrb_int_value(mrb, aot_int_div(mrb, mrb_integer(a), mrb_integer(b)));
  }
  if (mrb_float_p(a) && AOT_NUM_P(b)) return mrb_float_value(mrb, mrb_float(a) / AOT_AS_F(b));
  return aot_send1(mrb, a, MRB_OPSYM(div), b);
}

mrb_value
aot_mod(mrb_state *mrb, mrb_value a, mrb_value b)
{
  if (AOT_BOTH_INT(a, b) && mrb_integer(b) != 0) {
    return mrb_int_value(mrb, aot_int_mod(mrb, mrb_integer(a), mrb_integer(b)));
  }
  return aot_send1(mrb, a, MRB_OPSYM(mod), b);
}

#define AOT_CMP(name, op, sym)                                               \
  mrb_value                                                                  \
  name(mrb_state *mrb, mrb_value a, mrb_value b)                             \
  {                                                                          \
    if (AOT_BOTH_INT(a, b)) return mrb_bool_value(mrb_integer(a) op mrb_integer(b)); \
    if (AOT_NUM_P(a) && AOT_NUM_P(b)) return mrb_bool_value(AOT_AS_F(a) op AOT_AS_F(b)); \
    return aot_send1(mrb, a, sym, b);                                        \
  }

AOT_CMP(aot_lt, <, MRB_OPSYM(lt))
AOT_CMP(aot_le, <=, MRB_OPSYM(le))
AOT_CMP(aot_gt, >, MRB_OPSYM(gt))
AOT_CMP(aot_ge, >=, MRB_OPSYM(ge))

mrb_value
aot_eq(mrb_state *mrb, mrb_value a, mrb_value b)
{
  if (AOT_BOTH_INT(a, b)) return mrb_bool_value(mrb_integer(a) == mrb_integer(b));
  if (mrb_symbol_p(a) && mrb_symbol_p(b)) return mrb_bool_value(mrb_symbol(a) == mrb_symbol(b));
  if (mrb_nil_p(a) || mrb_true_p(a) || mrb_false_p(a)) return mrb_bool_value(mrb_obj_eq(mrb, a, b));
  if (mrb_string_p(a) && mrb_string_p(b)) return mrb_bool_value(mrb_str_equal(mrb, a, b));
  return aot_send1(mrb, a, MRB_OPSYM(eq), b);
}

mrb_value
aot_neq(mrb_state *mrb, mrb_value a, mrb_value b)
{
  if (AOT_BOTH_INT(a, b)) return mrb_bool_value(mrb_integer(a) != mrb_integer(b));
  if (mrb_symbol_p(a) && mrb_symbol_p(b)) return mrb_bool_value(mrb_symbol(a) != mrb_symbol(b));
  if (mrb_string_p(a) && mrb_string_p(b)) return mrb_bool_value(!mrb_str_equal(mrb, a, b));
  return aot_send1(mrb, a, MRB_OPSYM(neq), b);
}

#define AOT_BITOP(name, op, sym)                                             \
  mrb_value                                                                  \
  name(mrb_state *mrb, mrb_value a, mrb_value b)                             \
  {                                                                          \
    if (AOT_BOTH_INT(a, b)) return mrb_int_value(mrb, mrb_integer(a) op mrb_integer(b)); \
    return aot_send1(mrb, a, sym, b);                                        \
  }

AOT_BITOP(aot_band, &, MRB_OPSYM(and))
AOT_BITOP(aot_bor, |, MRB_OPSYM(or))
AOT_BITOP(aot_bxor, ^, MRB_OPSYM(xor))

mrb_value
aot_lshift(mrb_state *mrb, mrb_value a, mrb_value b)
{
  if (AOT_BOTH_INT(a, b)) {
    mrb_int x = mrb_integer(a), s = mrb_integer(b);
    if (s >= 0 && s < MRB_INT_BIT - 1) {
      if (x == 0) return a;
      if ((x > 0 && x <= (MRB_INT_MAX >> s)) || (x < 0 && x >= (MRB_INT_MIN >> s))) {
        return mrb_int_value(mrb, (mrb_int)((mrb_uint)x << s));
      }
    }
    else if (s < 0 && s > -(MRB_INT_BIT - 1)) {
      return mrb_int_value(mrb, x >> -s);
    }
  }
  if (mrb_string_p(a) && mrb_string_p(b)) return mrb_str_cat_str(mrb, a, b);
  if (mrb_array_p(a) && aot_builtin_p(mrb, a, MRB_OPSYM(lshift), aot_bi.ary_lshift)) {
    mrb_ary_push(mrb, a, b);
    return a;
  }
  return aot_send1(mrb, a, MRB_OPSYM(lshift), b);
}

mrb_value
aot_rshift(mrb_state *mrb, mrb_value a, mrb_value b)
{
  if (AOT_BOTH_INT(a, b)) {
    mrb_int x = mrb_integer(a), s = mrb_integer(b);
    if (s >= 0) return mrb_int_value(mrb, s >= MRB_INT_BIT ? (x < 0 ? -1 : 0) : (x >> s));
  }
  return aot_send1(mrb, a, MRB_OPSYM(rshift), b);
}

mrb_value
aot_neg(mrb_state *mrb, mrb_value a)
{
  if (mrb_integer_p(a) && mrb_integer(a) != MRB_INT_MIN) return mrb_int_value(mrb, -mrb_integer(a));
  if (mrb_float_p(a)) return mrb_float_value(mrb, -mrb_float(a));
  return mrb_funcall_argv(mrb, a, MRB_OPSYM(minus), 0, NULL);
}

/* ---- String ------------------------------------------------------- */

static mrb_value
aot_send0(mrb_state *mrb, mrb_value a, mrb_sym name)
{
  return mrb_funcall_argv(mrb, a, name, 0, NULL);
}

static mrb_value
aot_send2(mrb_state *mrb, mrb_value a, mrb_sym name, mrb_value b, mrb_value c)
{
  mrb_value argv[2];
  argv[0] = b;
  argv[1] = c;
  return mrb_funcall_argv(mrb, a, name, 2, argv);
}

/* A String whose character count is its byte count: ASCII only, or read
   as bytes. Any other String is counted by the VM. */
#define AOT_STR_BYTES_P(s) (mrb_string_p(s) && RSTR_SINGLE_BYTE_P(RSTRING(s)))

mrb_value
aot_str_size(mrb_state *mrb, mrb_value s)
{
  if (AOT_STR_BYTES_P(s)) return mrb_int_value(mrb, RSTRING_LEN(s));
  return aot_send0(mrb, s, MRB_SYM(size));
}

mrb_value
aot_str_length(mrb_state *mrb, mrb_value s)
{
  if (AOT_STR_BYTES_P(s)) return mrb_int_value(mrb, RSTRING_LEN(s));
  return aot_send0(mrb, s, MRB_SYM(length));
}

mrb_value
aot_str_bytesize(mrb_state *mrb, mrb_value s)
{
  if (mrb_string_p(s)) return mrb_int_value(mrb, RSTRING_LEN(s));
  return aot_send0(mrb, s, MRB_SYM(bytesize));
}

mrb_value
aot_str_empty_p(mrb_state *mrb, mrb_value s)
{
  if (mrb_string_p(s)) return mrb_bool_value(RSTRING_LEN(s) == 0);
  return aot_send0(mrb, s, MRB_SYM_Q(empty));
}

mrb_value
aot_str_getbyte(mrb_state *mrb, mrb_value s, mrb_value i)
{
  if (mrb_string_p(s) && mrb_integer_p(i)) {
    mrb_int pos = mrb_integer(i);
    mrb_int len = RSTRING_LEN(s);
    if (pos < 0) pos += len;
    if (pos < 0 || len <= pos) return mrb_nil_value();
    return mrb_int_value(mrb, (unsigned char)RSTRING_PTR(s)[pos]);
  }
  return aot_send1(mrb, s, MRB_SYM(getbyte), i);
}

/* `s[i]`: the one-character String at i, nil past the end. mrb_str_substr
   counts characters and answers "" at the very end, which `[]` reports as
   nil. */
mrb_value
aot_str_aref1(mrb_state *mrb, mrb_value s, mrb_value i)
{
  if (mrb_string_p(s) && mrb_integer_p(i)) {
    mrb_value r = mrb_str_substr(mrb, s, mrb_integer(i), 1);
    if (mrb_string_p(r) && RSTRING_LEN(r) == 0) return mrb_nil_value();
    return r;
  }
  return aot_send1(mrb, s, MRB_OPSYM(aref), i);
}

mrb_value
aot_str_aref2(mrb_state *mrb, mrb_value s, mrb_value beg, mrb_value len)
{
  if (mrb_string_p(s) && mrb_integer_p(beg) && mrb_integer_p(len)) {
    return mrb_str_substr(mrb, s, mrb_integer(beg), mrb_integer(len));
  }
  return aot_send2(mrb, s, MRB_OPSYM(aref), beg, len);
}

mrb_value
aot_str_byteslice(mrb_state *mrb, mrb_value s, mrb_value beg, mrb_value len)
{
  if (mrb_string_p(s) && mrb_integer_p(beg) && mrb_integer_p(len)) {
    mrb_int slen = RSTRING_LEN(s);
    mrb_int b = mrb_integer(beg);
    mrb_int l = mrb_integer(len);
    if (slen < b || l < 0) return mrb_nil_value();
    if (b < 0) {
      b += slen;
      if (b < 0) return mrb_nil_value();
    }
    if (l > slen - b) l = slen - b;
    if (l < 0) l = 0;
    return mrb_str_byte_subseq(mrb, s, b, l);
  }
  return aot_send2(mrb, s, MRB_SYM(byteslice), beg, len);
}

/* ---- Array and Hash ------------------------------------------------- */

#define AOT_ARY_IDX_P(v, slot) (mrb_array_p(v) && mrb_ary_ptr(v)->c == mrb->idx_class[slot])
#define AOT_HASH_IDX_P(v, slot) (mrb_hash_p(v) && mrb_hash_ptr(v)->c == mrb->idx_class[slot])

mrb_value
aot_ary_aref(mrb_state *mrb, mrb_value a, mrb_value i)
{
  if (AOT_ARY_IDX_P(a, MRB_IDX_OP_ARY_AREF) && mrb_integer_p(i)) {
    return aot_protect(mrb, mrb_ary_entry(a, mrb_integer(i)));
  }
  return aot_send1(mrb, a, MRB_OPSYM(aref), i);
}

mrb_value
aot_ary_aset(mrb_state *mrb, mrb_value a, mrb_value i, mrb_value v)
{
  if (AOT_ARY_IDX_P(a, MRB_IDX_OP_ARY_ASET) && mrb_integer_p(i)) {
    mrb_ary_set(mrb, a, mrb_integer(i), v);
    return v;
  }
  return aot_send2(mrb, a, MRB_OPSYM(aset), i, v);
}

#define AOT_ARY0(name, sym, field, expr)                                     \
  mrb_value                                                                  \
  name(mrb_state *mrb, mrb_value a)                                          \
  {                                                                          \
    if (mrb_array_p(a) && aot_builtin_p(mrb, a, sym, aot_bi.field)) return aot_protect(mrb, expr); \
    return aot_send0(mrb, a, sym);                                           \
  }

AOT_ARY0(aot_ary_size, MRB_SYM(size), ary_size, mrb_int_value(mrb, RARRAY_LEN(a)))
AOT_ARY0(aot_ary_length, MRB_SYM(length), ary_length, mrb_int_value(mrb, RARRAY_LEN(a)))
AOT_ARY0(aot_ary_empty_p, MRB_SYM_Q(empty), ary_empty_p, mrb_bool_value(RARRAY_LEN(a) == 0))
AOT_ARY0(aot_ary_pop, MRB_SYM(pop), ary_pop, mrb_ary_pop(mrb, a))
AOT_ARY0(aot_ary_shift, MRB_SYM(shift), ary_shift, mrb_ary_shift(mrb, a))
AOT_ARY0(aot_ary_first, MRB_SYM(first), ary_first, mrb_ary_entry(a, 0))
AOT_ARY0(aot_ary_last, MRB_SYM(last), ary_last, mrb_ary_entry(a, -1))
AOT_ARY0(aot_ary_clear, MRB_SYM(clear), ary_clear, mrb_ary_clear(mrb, a))

mrb_value
aot_ary_push(mrb_state *mrb, mrb_value a, mrb_value v)
{
  if (mrb_array_p(a) && aot_builtin_p(mrb, a, MRB_SYM(push), aot_bi.ary_push)) {
    mrb_ary_push(mrb, a, v);
    return a;
  }
  return aot_send1(mrb, a, MRB_SYM(push), v);
}

mrb_value
aot_hash_aref(mrb_state *mrb, mrb_value h, mrb_value k)
{
  if (AOT_HASH_IDX_P(h, MRB_IDX_OP_HASH_AREF)) return aot_protect(mrb, mrb_hash_get(mrb, h, k));
  return aot_send1(mrb, h, MRB_OPSYM(aref), k);
}

mrb_value
aot_hash_aset(mrb_state *mrb, mrb_value h, mrb_value k, mrb_value v)
{
  if (AOT_HASH_IDX_P(h, MRB_IDX_OP_HASH_ASET)) {
    mrb_hash_set(mrb, h, k, v);
    return v;
  }
  return aot_send2(mrb, h, MRB_OPSYM(aset), k, v);
}

mrb_value
aot_hash_key_p(mrb_state *mrb, mrb_value h, mrb_value k)
{
  if (mrb_hash_p(h) && aot_builtin_p(mrb, h, MRB_SYM_Q(key), aot_bi.hash_key_p)) {
    return mrb_bool_value(mrb_hash_key_p(mrb, h, k));
  }
  return aot_send1(mrb, h, MRB_SYM_Q(key), k);
}

mrb_value
aot_hash_size(mrb_state *mrb, mrb_value h)
{
  if (mrb_hash_p(h) && aot_builtin_p(mrb, h, MRB_SYM(size), aot_bi.hash_size)) {
    return mrb_int_value(mrb, mrb_hash_size(mrb, h));
  }
  return aot_send0(mrb, h, MRB_SYM(size));
}

mrb_value
aot_hash_delete(mrb_state *mrb, mrb_value h, mrb_value k)
{
  if (mrb_hash_p(h) && aot_builtin_p(mrb, h, MRB_SYM(delete), aot_bi.hash_delete)) {
    return aot_protect(mrb, mrb_hash_delete_key(mrb, h, k));
  }
  return aot_send1(mrb, h, MRB_SYM(delete), k);
}

/* ---- case/when and multiple assignment -------------------------------- */

mrb_value
aot_eqq(mrb_state *mrb, mrb_value pat, mrb_value v)
{
  switch (mrb_type(pat)) {
  case MRB_TT_CLASS:
  case MRB_TT_MODULE:
  case MRB_TT_SCLASS:
    return mrb_bool_value(mrb_obj_is_kind_of(mrb, v, mrb_class_ptr(pat)));
  case MRB_TT_INTEGER:
  case MRB_TT_FLOAT:
  case MRB_TT_SYMBOL:
  case MRB_TT_STRING:
  case MRB_TT_FALSE:
  case MRB_TT_TRUE:
    return aot_eq(mrb, pat, v);
  default:
    break;
  }
  return aot_send1(mrb, pat, MRB_OPSYM(eqq), v);
}

mrb_value
aot_masgn_get(mrb_state *mrb, mrb_value v, mrb_int i)
{
  if (mrb_array_p(v)) return aot_protect(mrb, mrb_ary_entry(v, i));
  return i == 0 ? v : mrb_nil_value();
}

/* ---- constants ------------------------------------------------------ */

static void
aot_path_intern(mrb_state *mrb, struct aot_path *p)
{
  if (p->syms[0] != 0) return;
  for (int i = 0; i < p->n; i++) p->syms[i] = mrb_intern_cstr(mrb, p->names[i]);
}

struct RClass *
aot_scope_init(mrb_state *mrb, struct aot_path *p)
{
  struct RClass *c = mrb->object_class;
  aot_path_intern(mrb, p);
  for (int i = 0; i < p->n; i++) {
    mrb_value v = mrb_const_get(mrb, mrb_obj_value(c), p->syms[i]);
    if (!mrb_class_p(v) && !mrb_module_p(v)) {
      mrb_raisef(mrb, E_TYPE_ERROR, "picoruby-aot: %s is not a class/module", p->names[i]);
    }
    c = mrb_class_ptr(v);
    p->klass[i] = c;
  }
  return c;
}

mrb_value
aot_const_get(mrb_state *mrb, mrb_value self, const struct aot_path *scope, mrb_sym name)
{
  /* The order of mrb_vm_const_get. First each lexical scope's own
     constants, innermost first: the classes aot_scope_init recorded at
     registration (the VM reads them off the proc's target classes). */
  struct RClass *inner = NULL;
  (void)self;
  if (scope) {
    for (int i = scope->n - 1; i >= 0; i--) {
      struct RClass *c = scope->klass[i];
      if (!c) continue;
      if (!inner) inner = c;
      if (mrb_const_defined_at(mrb, mrb_obj_value(c), name)) {
        return mrb_const_get(mrb, mrb_obj_value(c), name);
      }
    }
  }
  if (!inner) inner = mrb->object_class;
  /* Then the ancestors of the innermost scope, not of the receiver: a
     module included there appears in the chain as an ICLASS whose
     constants are the module's. A class the chain reaches Object through;
     a module does not, so Object is the last explicit step. */
  {
    struct RClass *c;
    for (c = inner->super; c; c = c->super) {
      if (MRB_FLAG_TEST(c, MRB_FL_CLASS_IS_PREPENDED)) continue;
      struct RClass *owner = (c->tt == MRB_TT_ICLASS) ? c->c : c;
      if (mrb_const_defined_at(mrb, mrb_obj_value(owner), name)) {
        return mrb_const_get(mrb, mrb_obj_value(owner), name);
      }
    }
    if (mrb_const_defined_at(mrb, mrb_obj_value(mrb->object_class), name)) {
      return mrb_const_get(mrb, mrb_obj_value(mrb->object_class), name);
    }
  }
  /* Nothing has it: let the innermost scope raise, as the VM's
     const_missing on that class does. */
  return mrb_const_get(mrb, mrb_obj_value(inner), name);
}

/* ---- a call with keywords ------------------------------------------- */

/* mruby has no C API that passes keywords: mrb_funcall_argv hands a
   trailing Hash as a positional, and OP_ENTER takes it as one when the
   frame's keyword flag is off. PicoRubyAOT.send_kw, a Ruby method of this
   gem, does `recv.__send__(mid, *args, **kw, &blk)` for us. */
mrb_value
aot_funcall_kw(mrb_state *mrb, mrb_value recv, mrb_sym mid, mrb_int argc, const mrb_value *argv, mrb_value kw, mrb_value blk)
{
  static struct RClass *mod = NULL;
  mrb_value args[5];
  if (mod == NULL) mod = mrb_module_get_id(mrb, mrb_intern_lit(mrb, "PicoRubyAOT"));
  args[0] = recv;
  args[1] = mrb_symbol_value(mid);
  args[2] = mrb_ary_new_from_values(mrb, argc, argv);
  args[3] = kw;
  args[4] = blk;
  return mrb_funcall_argv(mrb, mrb_obj_value(mod), mrb_intern_lit(mrb, "send_kw"), 5, args);
}

/* ---- blocks --------------------------------------------------------- */

/* The block a cfunc received is a Proc or nil (mrb_get_args "&"); the
   generated C hands it on as it is, so the two helpers see only those. */
mrb_value
aot_yield(mrb_state *mrb, mrb_value blk, mrb_int argc, const mrb_value *argv)
{
  if (mrb_nil_p(blk)) {
    mrb_raise(mrb, E_LOCALJUMP_ERROR, "unexpected yield");
  }
  return mrb_yield_argv(mrb, blk, argc, argv);
}

mrb_value
aot_block_arg(mrb_state *mrb, mrb_value v)
{
  if (mrb_nil_p(v) || mrb_proc_p(v)) return v;
  return mrb_type_convert(mrb, v, MRB_TT_PROC, mrb_intern_lit(mrb, "to_proc"));
}

void
aot_raise_obj(mrb_state *mrb, mrb_value klass, const char *msg)
{
  if (!mrb_class_p(klass)) {
    mrb_raise(mrb, E_TYPE_ERROR, "exception class/object expected");
  }
  mrb_raise(mrb, mrb_class_ptr(klass), msg);
}

void
mrb_picoruby_aot_gem_init(mrb_state *mrb)
{
  aot_builtins_init(mrb);
}

void
mrb_picoruby_aot_gem_final(mrb_state *mrb)
{
  (void)mrb;
}
