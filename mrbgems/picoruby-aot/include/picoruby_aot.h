/*
 * Runtime helpers for C that picoruby-aot generates from Ruby methods.
 *
 * The generated code keeps Integer, Float and bool locals unboxed and
 * boxes them into mrb_value only at the boundaries: method entry and
 * return, instance variables, and calls it hands to the VM through
 * mrb_funcall_argv. Every unboxing of a value the VM produced is checked:
 * a type the analysis did not predict raises TypeError instead of
 * reinterpreting the word.
 */
#ifndef PICORUBY_AOT_H
#define PICORUBY_AOT_H

#include <mruby.h>
#include <mruby/array.h>
#include <mruby/class.h>
#include <mruby/error.h>
#include <mruby/hash.h>
#include <mruby/numeric.h>
#include <mruby/proc.h>
#include <mruby/range.h>
#include <mruby/string.h>
#include <mruby/variable.h>

#ifdef __cplusplus
extern "C" {
#endif

/* The C form of a proved Integer: 64 bits on every target. On an
   MRB_INT32 build the VM makes a Bignum of a microsecond count after 36
   minutes and of a UNIX time; a 32-bit slot would turn those into an
   error where the irep goes on. A value crosses into the VM as an Integer
   when mrb_int holds it and as a Bignum otherwise (mruby-bigint, which
   every MRB_INT32 target links; RangeError without it), and comes back
   from a Bignum that fits. Only past 64 bits does a compiled method
   differ from its irep: RangeError where the VM would grow the Bignum. */
typedef int64_t aot_int;
#define AOT_INT_MAX INT64_MAX
#define AOT_INT_MIN INT64_MIN
#define AOT_INT_BIT 64

/* Checked unboxing: the proved class and nothing else, or TypeError. */
aot_int aot_unbox_int(mrb_state *mrb, mrb_value v);
mrb_float aot_unbox_float(mrb_state *mrb, mrb_value v);
mrb_bool aot_unbox_bool(mrb_state *mrb, mrb_value v);

/* An aot_int as the VM holds it: an Integer, or a Bignum past mrb_int. */
mrb_value aot_box_int(mrb_state *mrb, aot_int i);

/* Integer arithmetic with mruby's semantics on the fast path. Overflow of
   the 64-bit slot raises RangeError. Division and modulo floor toward
   negative infinity and raise ZeroDivisionError. */
aot_int aot_int_add(mrb_state *mrb, aot_int a, aot_int b);
aot_int aot_int_sub(mrb_state *mrb, aot_int a, aot_int b);
aot_int aot_int_mul(mrb_state *mrb, aot_int a, aot_int b);
aot_int aot_int_div(mrb_state *mrb, aot_int a, aot_int b);
aot_int aot_int_mod(mrb_state *mrb, aot_int a, aot_int b);
aot_int aot_int_neg(mrb_state *mrb, aot_int a);
aot_int aot_int_lshift(mrb_state *mrb, aot_int a, aot_int b);
aot_int aot_int_rshift(mrb_state *mrb, aot_int a, aot_int b);
aot_int aot_int_pow(mrb_state *mrb, aot_int a, aot_int b);
mrb_float aot_float_mod(mrb_float a, mrb_float b);
aot_int aot_float_to_i(mrb_state *mrb, mrb_float f);

/* Guarded operators on boxed values: an inline Integer (and, where it
   applies, Float) fast path, and the VM for every other receiver, so nil,
   a Bignum and a user-defined operator behave exactly as in the irep. */
mrb_value aot_add(mrb_state *mrb, mrb_value a, mrb_value b);
mrb_value aot_sub(mrb_state *mrb, mrb_value a, mrb_value b);
mrb_value aot_mul(mrb_state *mrb, mrb_value a, mrb_value b);
mrb_value aot_div(mrb_state *mrb, mrb_value a, mrb_value b);
mrb_value aot_mod(mrb_state *mrb, mrb_value a, mrb_value b);
mrb_value aot_lt(mrb_state *mrb, mrb_value a, mrb_value b);
mrb_value aot_le(mrb_state *mrb, mrb_value a, mrb_value b);
mrb_value aot_gt(mrb_state *mrb, mrb_value a, mrb_value b);
mrb_value aot_ge(mrb_state *mrb, mrb_value a, mrb_value b);
mrb_value aot_eq(mrb_state *mrb, mrb_value a, mrb_value b);
mrb_value aot_neq(mrb_state *mrb, mrb_value a, mrb_value b);
mrb_value aot_band(mrb_state *mrb, mrb_value a, mrb_value b);
mrb_value aot_bor(mrb_state *mrb, mrb_value a, mrb_value b);
mrb_value aot_bxor(mrb_state *mrb, mrb_value a, mrb_value b);
mrb_value aot_lshift(mrb_state *mrb, mrb_value a, mrb_value b);
mrb_value aot_rshift(mrb_state *mrb, mrb_value a, mrb_value b);
mrb_value aot_neg(mrb_state *mrb, mrb_value a);

/* Guarded String methods, emitted where Spinel typed the receiver a
   String: the String case inline, every other receiver (nil, a value a
   redefinition handed back) sent to the VM by name, as a boxed operator
   is. `aot_add`, `aot_eq`, `aot_neq` and `aot_lshift` above take the
   String arms too (`+`, `==`, `!=`, `<<` of two Strings). */
mrb_value aot_str_size(mrb_state *mrb, mrb_value s);
mrb_value aot_str_length(mrb_state *mrb, mrb_value s);
mrb_value aot_str_bytesize(mrb_state *mrb, mrb_value s);
mrb_value aot_str_empty_p(mrb_state *mrb, mrb_value s);
mrb_value aot_str_getbyte(mrb_state *mrb, mrb_value s, mrb_value i);
mrb_value aot_str_aref1(mrb_state *mrb, mrb_value s, mrb_value i);
mrb_value aot_str_aref2(mrb_state *mrb, mrb_value s, mrb_value beg, mrb_value len);
mrb_value aot_str_byteslice(mrb_state *mrb, mrb_value s, mrb_value beg, mrb_value len);

/* Does `mid` on `recv` resolve to the C function `f` right now? The test a
   direct call makes before it skips the VM: the method lookup the VM would
   do (through its method cache), and nothing else of a send. False for an
   undefined method, a redefinition, a subclass override or a different
   receiver class, which all go through mrb_funcall as before. */
static inline mrb_bool
aot_method_is(mrb_state *mrb, mrb_value recv, mrb_sym mid, mrb_func_t f)
{
  struct RClass *c = mrb_class(mrb, recv);
  mrb_method_t m = mrb_method_search_vm(mrb, &c, mid);
  return !MRB_METHOD_UNDEF_P(m) && MRB_METHOD_CFUNC_P(m) && MRB_METHOD_CFUNC(m) == f;
}

/* GC roots. The VM's registers are roots; the generated C's locals are
   not. A value the C holds is safe while something else references it
   (an object it was read from, a constant) or while it sits in the GC
   arena (every object mrb_obj_alloc made, every mrb_funcall result). A
   value whose owner may let go of it (an element shift or pop removed,
   an instance variable a later write replaces) is pushed to the arena at
   the read, which protects it until the outermost cfunc returns to the
   VM. An immediate (Integer, Symbol, nil, true, false) needs nothing. */
static inline mrb_value
aot_protect(mrb_state *mrb, mrb_value v)
{
  if (!mrb_immediate_p(v)) mrb_gc_protect(mrb, v);
  return v;
}

static inline mrb_value
aot_iv_get(mrb_state *mrb, mrb_value self, mrb_sym name)
{
  return aot_protect(mrb, mrb_iv_get(mrb, self, name));
}

/* A local a block shares with its method: a one-element Array both read
   and write through, so a Proc that outlives the call sees what Ruby's
   closure sees. The method holds the cell in a C local (the arena
   protects it until the outermost cfunc returns) and the Proc's env
   holds it for as long as the Proc lives. A read protects the value as
   aot_iv_get does: a later write to the cell may let go of it. */
static inline mrb_value
aot_cell_new(mrb_state *mrb, mrb_value v)
{
  return mrb_ary_new_from_values(mrb, 1, &v);
}

static inline mrb_value
aot_cell_get(mrb_state *mrb, mrb_value cell)
{
  return aot_protect(mrb, RARRAY_PTR(cell)[0]);
}

static inline void
aot_cell_set(mrb_state *mrb, mrb_value cell, mrb_value v)
{
  mrb_ary_set(mrb, cell, 0, v);
}

/* Guarded Array and Hash methods, emitted where Spinel typed the receiver
   an Array or a Hash. `[]` and `[]=` take the test the VM's index opcodes
   take (the receiver's class is the core class and the operator is not
   redefined: mrb->idx_class). The others resolve the method as
   aot_method_is does and compare it with the builtin recorded at gem init,
   so a redefinition on Array itself is honoured too. */
mrb_value aot_ary_aref(mrb_state *mrb, mrb_value a, mrb_value i);
mrb_value aot_ary_aset(mrb_state *mrb, mrb_value a, mrb_value i, mrb_value v);
mrb_value aot_ary_size(mrb_state *mrb, mrb_value a);
mrb_value aot_ary_length(mrb_state *mrb, mrb_value a);
mrb_value aot_ary_empty_p(mrb_state *mrb, mrb_value a);
mrb_value aot_ary_push(mrb_state *mrb, mrb_value a, mrb_value v);
mrb_value aot_ary_pop(mrb_state *mrb, mrb_value a);
mrb_value aot_ary_shift(mrb_state *mrb, mrb_value a);
mrb_value aot_ary_first(mrb_state *mrb, mrb_value a);
mrb_value aot_ary_last(mrb_state *mrb, mrb_value a);
mrb_value aot_ary_clear(mrb_state *mrb, mrb_value a);
mrb_value aot_hash_aref(mrb_state *mrb, mrb_value h, mrb_value k);
mrb_value aot_hash_aset(mrb_state *mrb, mrb_value h, mrb_value k, mrb_value v);
mrb_value aot_hash_key_p(mrb_state *mrb, mrb_value h, mrb_value k);
mrb_value aot_hash_size(mrb_state *mrb, mrb_value h);
mrb_value aot_hash_delete(mrb_state *mrb, mrb_value h, mrb_value k);

/* `pat === v` for a case/when arm: a class or module asks kind_of?, the
   core scalars and String compare as `==` does (aot_eq), anything else
   (a Range, a Regexp, a user class) sends `===`. */
mrb_value aot_eqq(mrb_state *mrb, mrb_value pat, mrb_value v);

/* `n === v` and `(lo..hi) === v` for Integer literals, in place: an
   Integer or Float value compares, a Bignum compares through
   mruby-bigint, anything else (nil at the end of a String, a String)
   does not match, as Integer#== answers. */
int aot_bint_cmp_int(mrb_state *mrb, mrb_value v, aot_int n);

static inline mrb_bool
aot_eqq_int(mrb_state *mrb, mrb_value v, aot_int n)
{
  if (mrb_integer_p(v)) return (aot_int)mrb_integer(v) == n;
  if (mrb_float_p(v)) return mrb_float(v) == (mrb_float)n;
  if (mrb_bigint_p(v)) return aot_bint_cmp_int(mrb, v, n) == 0;
  return FALSE;
}

static inline mrb_bool
aot_eqq_int_range(mrb_state *mrb, mrb_value v, aot_int lo, aot_int hi, int excl)
{
  mrb_float f;
  if (mrb_integer_p(v)) {
    aot_int i = (aot_int)mrb_integer(v);
    return lo <= i && (excl ? i < hi : i <= hi);
  }
  if (mrb_bigint_p(v)) {
    int c = aot_bint_cmp_int(mrb, v, hi);
    return aot_bint_cmp_int(mrb, v, lo) >= 0 && (excl ? c < 0 : c <= 0);
  }
  if (!mrb_float_p(v)) return FALSE;
  f = mrb_float(v);
  return (mrb_float)lo <= f && (excl ? f < (mrb_float)hi : f <= (mrb_float)hi);
}

/* Element i of a multiple assignment's right-hand side: the Array's
   element, or for a non-Array the value itself at 0 and nil after. */
mrb_value aot_masgn_get(mrb_state *mrb, mrb_value v, mrb_int i);

/* A symbol interned once per call site. `cache` is a static mrb_sym in the
   generated translation unit, 0 until first use. */
static inline mrb_sym
aot_sym(mrb_state *mrb, mrb_sym *cache, const char *name)
{
  if (*cache == 0) *cache = mrb_intern_cstr(mrb, name);
  return *cache;
}

/* A method's lexical scope "A::B::C" as the generated translation unit
   holds it: one static per distinct path, with its names, the symbols
   interned on first use, and the classes recorded at registration. (A
   constant path in a method body is not one of these: the generated C
   reads it name by name, as the VM does.) */
struct aot_path {
  int n;
  const char *const *names;
  mrb_sym *syms;
  struct RClass **klass;
};

/* Resolve a method's lexical scope once, at registration, recording each
   class of the path for aot_const_get; returns the innermost. */
struct RClass *aot_scope_init(mrb_state *mrb, struct aot_path *p);

/* A constant read inside a method body. Ruby looks it up lexically first
   (the class the method is written in and its outer scopes), then through
   the receiver's ancestors. `scope` is the path aot_scope_init recorded,
   NULL for a top-level def. */
mrb_value aot_const_get(mrb_state *mrb, mrb_value self, const struct aot_path *scope, mrb_sym name);

/* Raise `klass` (a class value a constant read produced) with a literal
   message; TypeError when it is not a class. */
void aot_raise_obj(mrb_state *mrb, mrb_value klass, const char *msg);

/* `recv.mid(*argv, **kw, &blk)`: a call with keywords, through the VM
   (PicoRubyAOT.send_kw in this gem's mrblib; mruby has no C API that
   passes keywords). `blk` is a Proc or nil. */
mrb_value aot_funcall_kw(mrb_state *mrb, mrb_value recv, mrb_sym mid, mrb_int argc, const mrb_value *argv, mrb_value kw, mrb_value blk);

/* `yield argv` on the block the cfunc received (nil when the caller gave
   none): LocalJumpError as the VM's OP_BLKPUSH raises, else
   mrb_yield_argv. A `break` or a `return` in the block unwinds through
   the compiled method as through any cfunc. */
mrb_value aot_yield(mrb_state *mrb, mrb_value blk, mrb_int argc, const mrb_value *argv);

/* `&v` at a call site: nil and a Proc as they are, anything else through
   to_proc (a Symbol), as the VM converts a block argument at the send.
   The impl of a compiled callee takes the result as its last argument. */
mrb_value aot_block_arg(mrb_state *mrb, mrb_value v);

/* The `n` parameters of a block literal's cfunc, from the arguments of
   the call: one Array over several parameters is splat, a missing
   argument is nil and an extra one is dropped, as the VM does for a
   block. The cfunc reads self and the outer locals it captured from its
   env (mrb_proc_cfunc_env_get); see codegen.rb, block_literal. */
void aot_block_args(mrb_state *mrb, mrb_int n, mrb_value *out);

/* Boxing shorthands kept as macros so the generated C reads evenly. */
#define AOT_BOX_INT(mrb, i)   aot_box_int((mrb), (i))
#define AOT_BOX_FLOAT(mrb, f) mrb_float_value((mrb), (f))
#define AOT_BOX_BOOL(b)       mrb_bool_value((b))

#ifdef __cplusplus
}
#endif

#endif /* PICORUBY_AOT_H */
