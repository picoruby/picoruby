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

/* A symbol interned once per call site. `cache` is a static mrb_sym in the
   generated translation unit, 0 until first use. */
static inline mrb_sym
aot_sym(mrb_state *mrb, mrb_sym *cache, const char *name)
{
  if (*cache == 0) *cache = mrb_intern_cstr(mrb, name);
  return *cache;
}

/* The class or module named by a "A::B::C" path, from Object. */
struct RClass *aot_resolve_class(mrb_state *mrb, const char *path);

/* A constant read inside a method body. Ruby looks it up lexically first
   (the class the method is written in and its outer scopes), then through
   the receiver's ancestors. `scope` is the lexical path the method was
   written in ("PSG::Synth"); it may be "" for a top-level def. */
mrb_value aot_const_get(mrb_state *mrb, mrb_value self, const char *scope, mrb_sym name);

/* Raise an exception class named at top level ("ArgumentError") with a
   literal message. */
void aot_raise(mrb_state *mrb, const char *class_name, const char *msg);

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
