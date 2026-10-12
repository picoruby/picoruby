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

/* Checked unboxing. */
mrb_int aot_unbox_int(mrb_state *mrb, mrb_value v);
mrb_float aot_unbox_float(mrb_state *mrb, mrb_value v);

/* Integer arithmetic with mruby's semantics on the fast path. Overflow of
   mrb_int raises RangeError: the generated code has no slot for a Bignum.
   Division and modulo floor toward negative infinity and raise
   ZeroDivisionError. */
mrb_int aot_int_add(mrb_state *mrb, mrb_int a, mrb_int b);
mrb_int aot_int_sub(mrb_state *mrb, mrb_int a, mrb_int b);
mrb_int aot_int_mul(mrb_state *mrb, mrb_int a, mrb_int b);
mrb_int aot_int_div(mrb_state *mrb, mrb_int a, mrb_int b);
mrb_int aot_int_mod(mrb_state *mrb, mrb_int a, mrb_int b);
mrb_int aot_int_neg(mrb_state *mrb, mrb_int a);
mrb_int aot_int_lshift(mrb_state *mrb, mrb_int a, mrb_int b);
mrb_int aot_int_rshift(mrb_state *mrb, mrb_int a, mrb_int b);
mrb_int aot_int_pow(mrb_state *mrb, mrb_int a, mrb_int b);
mrb_float aot_float_mod(mrb_float a, mrb_float b);
mrb_int aot_float_to_i(mrb_state *mrb, mrb_float f);

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
   Integer or Float value compares, anything else (nil at the end of a
   String, a String) does not match, as Integer#== answers. */
static inline mrb_bool
aot_eqq_int(mrb_value v, mrb_int n)
{
  if (mrb_integer_p(v)) return mrb_integer(v) == n;
  if (mrb_float_p(v)) return mrb_float(v) == (mrb_float)n;
  return FALSE;
}

static inline mrb_bool
aot_eqq_int_range(mrb_value v, mrb_int lo, mrb_int hi, int excl)
{
  mrb_float f;
  if (mrb_integer_p(v)) {
    mrb_int i = mrb_integer(v);
    return lo <= i && (excl ? i < hi : i <= hi);
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

/* A constant path "A::B::C" as the generated translation unit holds it:
   one static per distinct path, with its names, the symbols interned on
   first use, and (for a method's lexical scope) the classes recorded at
   registration. */
struct aot_path {
  int n;
  const char *const *names;
  mrb_sym *syms;
  struct RClass **klass;
};

/* The class or module the path names, looked up from Object by symbol on
   every call, as the VM's OP_GETMCNST chain is. */
struct RClass *aot_path_class(mrb_state *mrb, struct aot_path *p);

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

/* Boxing shorthands kept as macros so the generated C reads evenly. */
#define AOT_BOX_INT(mrb, i)   mrb_int_value((mrb), (i))
#define AOT_BOX_FLOAT(mrb, f) mrb_float_value((mrb), (f))
#define AOT_BOX_BOOL(b)       mrb_bool_value((b))

#ifdef __cplusplus
}
#endif

#endif /* PICORUBY_AOT_H */
