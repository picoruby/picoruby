#include <math.h>
#include <string.h>
#include "picoruby_aot.h"

mrb_int
aot_unbox_int(mrb_state *mrb, mrb_value v)
{
  if (mrb_integer_p(v)) return mrb_integer(v);
  mrb_raisef(mrb, E_TYPE_ERROR, "picoruby-aot: expected Integer, got %Y", v);
  return 0;
}

mrb_float
aot_unbox_float(mrb_state *mrb, mrb_value v)
{
  if (mrb_float_p(v)) return mrb_float(v);
  if (mrb_integer_p(v)) return (mrb_float)mrb_integer(v);
  mrb_raisef(mrb, E_TYPE_ERROR, "picoruby-aot: expected Float, got %Y", v);
  return 0.0;
}

static void
aot_overflow(mrb_state *mrb)
{
  mrb_raise(mrb, E_RANGE_ERROR, "picoruby-aot: Integer overflow in compiled method");
}

mrb_int
aot_int_add(mrb_state *mrb, mrb_int a, mrb_int b)
{
  mrb_int r;
  if (mrb_int_add_overflow(a, b, &r)) aot_overflow(mrb);
  return r;
}

mrb_int
aot_int_sub(mrb_state *mrb, mrb_int a, mrb_int b)
{
  mrb_int r;
  if (mrb_int_sub_overflow(a, b, &r)) aot_overflow(mrb);
  return r;
}

mrb_int
aot_int_mul(mrb_state *mrb, mrb_int a, mrb_int b)
{
  mrb_int r;
  if (mrb_int_mul_overflow(a, b, &r)) aot_overflow(mrb);
  return r;
}

mrb_int
aot_int_div(mrb_state *mrb, mrb_int a, mrb_int b)
{
  mrb_int q;
  if (b == 0) mrb_raise(mrb, E_ZERODIV_ERROR, "divided by 0");
  if (a == MRB_INT_MIN && b == -1) aot_overflow(mrb);
  q = a / b;
  if ((a % b != 0) && ((a < 0) != (b < 0))) q--;
  return q;
}

mrb_int
aot_int_mod(mrb_state *mrb, mrb_int a, mrb_int b)
{
  mrb_int m;
  if (b == 0) mrb_raise(mrb, E_ZERODIV_ERROR, "divided by 0");
  if (b == -1) return 0;
  m = a % b;
  if (m != 0 && ((m < 0) != (b < 0))) m += b;
  return m;
}

mrb_int
aot_int_neg(mrb_state *mrb, mrb_int a)
{
  if (a == MRB_INT_MIN) aot_overflow(mrb);
  return -a;
}

mrb_int
aot_int_lshift(mrb_state *mrb, mrb_int a, mrb_int b)
{
  if (b < 0) return aot_int_rshift(mrb, a, -b);
  if (b >= MRB_INT_BIT) {
    if (a == 0) return 0;
    aot_overflow(mrb);
  }
  if (a > 0) {
    if (a > (MRB_INT_MAX >> b)) aot_overflow(mrb);
  }
  else if (a < 0) {
    if (a < (MRB_INT_MIN >> b)) aot_overflow(mrb);
  }
  return (mrb_int)((mrb_uint)a << b);
}

mrb_int
aot_int_rshift(mrb_state *mrb, mrb_int a, mrb_int b)
{
  if (b < 0) return aot_int_lshift(mrb, a, -b);
  if (b >= MRB_INT_BIT) return a < 0 ? -1 : 0;
  return a >> b;
}

mrb_int
aot_int_pow(mrb_state *mrb, mrb_int a, mrb_int b)
{
  mrb_int r = 1;
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

mrb_int
aot_float_to_i(mrb_state *mrb, mrb_float f)
{
  if (isnan(f) || isinf(f)) {
    mrb_raise(mrb, E_FLOATDOMAIN_ERROR, "picoruby-aot: Float out of Integer range");
  }
  if (f >= (mrb_float)MRB_INT_MAX || f < (mrb_float)MRB_INT_MIN) aot_overflow(mrb);
  return (mrb_int)f;
}

/* ---- guarded operators on boxed values ------------------------------ */

static mrb_value
aot_send1(mrb_state *mrb, mrb_value a, const char *op, mrb_value b)
{
  return mrb_funcall_argv(mrb, a, mrb_intern_cstr(mrb, op), 1, &b);
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
  return aot_send1(mrb, a, "+", b);
}

mrb_value
aot_sub(mrb_state *mrb, mrb_value a, mrb_value b)
{
  mrb_int r;
  if (AOT_BOTH_INT(a, b) && !mrb_int_sub_overflow(mrb_integer(a), mrb_integer(b), &r)) {
    return mrb_int_value(mrb, r);
  }
  if (mrb_float_p(a) && AOT_NUM_P(b)) return mrb_float_value(mrb, mrb_float(a) - AOT_AS_F(b));
  return aot_send1(mrb, a, "-", b);
}

mrb_value
aot_mul(mrb_state *mrb, mrb_value a, mrb_value b)
{
  mrb_int r;
  if (AOT_BOTH_INT(a, b) && !mrb_int_mul_overflow(mrb_integer(a), mrb_integer(b), &r)) {
    return mrb_int_value(mrb, r);
  }
  if (mrb_float_p(a) && AOT_NUM_P(b)) return mrb_float_value(mrb, mrb_float(a) * AOT_AS_F(b));
  return aot_send1(mrb, a, "*", b);
}

mrb_value
aot_div(mrb_state *mrb, mrb_value a, mrb_value b)
{
  if (AOT_BOTH_INT(a, b) && mrb_integer(b) != 0 && !(mrb_integer(a) == MRB_INT_MIN && mrb_integer(b) == -1)) {
    return mrb_int_value(mrb, aot_int_div(mrb, mrb_integer(a), mrb_integer(b)));
  }
  if (mrb_float_p(a) && AOT_NUM_P(b)) return mrb_float_value(mrb, mrb_float(a) / AOT_AS_F(b));
  return aot_send1(mrb, a, "/", b);
}

mrb_value
aot_mod(mrb_state *mrb, mrb_value a, mrb_value b)
{
  if (AOT_BOTH_INT(a, b) && mrb_integer(b) != 0) {
    return mrb_int_value(mrb, aot_int_mod(mrb, mrb_integer(a), mrb_integer(b)));
  }
  return aot_send1(mrb, a, "%", b);
}

#define AOT_CMP(name, op, sym)                                               \
  mrb_value                                                                  \
  name(mrb_state *mrb, mrb_value a, mrb_value b)                             \
  {                                                                          \
    if (AOT_BOTH_INT(a, b)) return mrb_bool_value(mrb_integer(a) op mrb_integer(b)); \
    if (AOT_NUM_P(a) && AOT_NUM_P(b)) return mrb_bool_value(AOT_AS_F(a) op AOT_AS_F(b)); \
    return aot_send1(mrb, a, sym, b);                                        \
  }

AOT_CMP(aot_lt, <, "<")
AOT_CMP(aot_le, <=, "<=")
AOT_CMP(aot_gt, >, ">")
AOT_CMP(aot_ge, >=, ">=")

mrb_value
aot_eq(mrb_state *mrb, mrb_value a, mrb_value b)
{
  if (AOT_BOTH_INT(a, b)) return mrb_bool_value(mrb_integer(a) == mrb_integer(b));
  if (mrb_symbol_p(a) && mrb_symbol_p(b)) return mrb_bool_value(mrb_symbol(a) == mrb_symbol(b));
  if (mrb_nil_p(a) || mrb_true_p(a) || mrb_false_p(a)) return mrb_bool_value(mrb_obj_eq(mrb, a, b));
  return aot_send1(mrb, a, "==", b);
}

mrb_value
aot_neq(mrb_state *mrb, mrb_value a, mrb_value b)
{
  if (AOT_BOTH_INT(a, b)) return mrb_bool_value(mrb_integer(a) != mrb_integer(b));
  if (mrb_symbol_p(a) && mrb_symbol_p(b)) return mrb_bool_value(mrb_symbol(a) != mrb_symbol(b));
  return aot_send1(mrb, a, "!=", b);
}

#define AOT_BITOP(name, op, sym)                                             \
  mrb_value                                                                  \
  name(mrb_state *mrb, mrb_value a, mrb_value b)                             \
  {                                                                          \
    if (AOT_BOTH_INT(a, b)) return mrb_int_value(mrb, mrb_integer(a) op mrb_integer(b)); \
    return aot_send1(mrb, a, sym, b);                                        \
  }

AOT_BITOP(aot_band, &, "&")
AOT_BITOP(aot_bor, |, "|")
AOT_BITOP(aot_bxor, ^, "^")

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
  return aot_send1(mrb, a, "<<", b);
}

mrb_value
aot_rshift(mrb_state *mrb, mrb_value a, mrb_value b)
{
  if (AOT_BOTH_INT(a, b)) {
    mrb_int x = mrb_integer(a), s = mrb_integer(b);
    if (s >= 0) return mrb_int_value(mrb, s >= MRB_INT_BIT ? (x < 0 ? -1 : 0) : (x >> s));
  }
  return aot_send1(mrb, a, ">>", b);
}

mrb_value
aot_neg(mrb_state *mrb, mrb_value a)
{
  if (mrb_integer_p(a) && mrb_integer(a) != MRB_INT_MIN) return mrb_int_value(mrb, -mrb_integer(a));
  if (mrb_float_p(a)) return mrb_float_value(mrb, -mrb_float(a));
  return mrb_funcall_argv(mrb, a, mrb_intern_lit(mrb, "-@"), 0, NULL);
}

struct RClass *
aot_resolve_class(mrb_state *mrb, const char *path)
{
  struct RClass *c = mrb->object_class;
  const char *p = path;
  while (*p) {
    const char *q = strstr(p, "::");
    size_t len = q ? (size_t)(q - p) : strlen(p);
    mrb_value v = mrb_const_get(mrb, mrb_obj_value(c), mrb_intern(mrb, p, len));
    if (!mrb_class_p(v) && !mrb_module_p(v)) {
      mrb_raisef(mrb, E_TYPE_ERROR, "picoruby-aot: %s is not a class/module", path);
    }
    c = mrb_class_ptr(v);
    p = q ? q + 2 : p + len;
  }
  return c;
}

mrb_value
aot_const_get(mrb_state *mrb, mrb_value self, const char *scope, mrb_sym name)
{
  /* Lexical scopes, innermost first: "A::B::C", then "A::B", then "A". */
  size_t len = strlen(scope);
  while (len > 0) {
    char buf[128];
    struct RClass *c;
    if (len >= sizeof(buf)) break;
    memcpy(buf, scope, len);
    buf[len] = '\0';
    c = aot_resolve_class(mrb, buf);
    if (mrb_const_defined_at(mrb, mrb_obj_value(c), name)) {
      return mrb_const_get(mrb, mrb_obj_value(c), name);
    }
    while (len > 0 && !(len >= 2 && scope[len - 1] == ':' && scope[len - 2] == ':')) len--;
    if (len >= 2) len -= 2;
  }
  /* Then the receiver's class and its ancestors. mrb_const_get on a class
     stops short of Object, so Object is the last explicit step; its own
     NameError is the right one when nothing has the constant. */
  {
    struct RClass *c;
    for (c = mrb_class(mrb, self); c && c != mrb->object_class; c = c->super) {
      if (c->tt != MRB_TT_CLASS && c->tt != MRB_TT_MODULE) continue;
      if (mrb_const_defined_at(mrb, mrb_obj_value(c), name)) {
        return mrb_const_get(mrb, mrb_obj_value(c), name);
      }
    }
  }
  return mrb_const_get(mrb, mrb_obj_value(mrb->object_class), name);
}

void
aot_raise(mrb_state *mrb, const char *class_name, const char *msg)
{
  struct RClass *c = aot_resolve_class(mrb, class_name);
  mrb_raise(mrb, c, msg);
}

void
mrb_picoruby_aot_gem_init(mrb_state *mrb)
{
  (void)mrb;
}

void
mrb_picoruby_aot_gem_final(mrb_state *mrb)
{
  (void)mrb;
}
