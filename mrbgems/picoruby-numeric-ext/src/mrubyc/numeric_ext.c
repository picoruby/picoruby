#include <mrubyc.h>
#include <math.h>

static mrbc_float_t
c_flo_rounding(mrbc_float_t f, mrbc_int_t ndigits, double (*func)(double))
{
  if (f == 0.0) return f;

  if (ndigits > 0) {
    mrbc_float_t d = pow(10.0, (double)ndigits);
    return func(f * d) / d;
  }
  if (ndigits < 0) {
    mrbc_float_t d = pow(10.0, (double)-ndigits);
    return func(f / d) * d;
  }
  /* ndigits == 0 */
  return func(f);
}

static void
c_flo_round(struct VM *vm, mrbc_value v[], int argc)
{
  mrbc_float_t f = mrbc_float(v[0]);
  mrbc_int_t ndigits = (argc >= 1) ? mrbc_integer(v[1]) : 0;

  mrbc_float_t int_part;
  if (ndigits == 0) {
    /* home-made round: round half away from zero */
    if (f > 0.0) {
      int_part = floor(f);
      f = int_part + (f - int_part >= 0.5);
    } else if (f < 0.0) {
      int_part = ceil(f);
      f = int_part - (int_part - f >= 0.5);
    }
  } else {
    mrbc_float_t factor = pow(10.0, (double)(ndigits > 0 ? ndigits : -ndigits));
    if (ndigits > 0) {
      f *= factor;
    } else {
      f /= factor;
    }
    if (f > 0.0) {
      int_part = floor(f);
      f = int_part + (f - int_part >= 0.5);
    } else if (f < 0.0) {
      int_part = ceil(f);
      f = int_part - (int_part - f >= 0.5);
    }
    if (ndigits > 0) {
      f /= factor;
    } else {
      f *= factor;
    }
  }

  if (ndigits <= 0) {
    SET_INT_RETURN((mrbc_int_t)f);
  } else {
    SET_FLOAT_RETURN(f);
  }
}

static void
c_flo_floor(struct VM *vm, mrbc_value v[], int argc)
{
  mrbc_float_t f = mrbc_float(v[0]);
  mrbc_int_t ndigits = (argc >= 1) ? mrbc_integer(v[1]) : 0;

  f = c_flo_rounding(f, ndigits, floor);

  if (ndigits <= 0) {
    SET_INT_RETURN((mrbc_int_t)f);
  } else {
    SET_FLOAT_RETURN(f);
  }
}

static void
c_flo_ceil(struct VM *vm, mrbc_value v[], int argc)
{
  mrbc_float_t f = mrbc_float(v[0]);
  mrbc_int_t ndigits = (argc >= 1) ? mrbc_integer(v[1]) : 0;

  f = c_flo_rounding(f, ndigits, ceil);

  if (ndigits <= 0) {
    SET_INT_RETURN((mrbc_int_t)f);
  } else {
    SET_FLOAT_RETURN(f);
  }
}

static void
c_flo_mod(struct VM *vm, mrbc_value v[], int argc)
{
  mrbc_float_t left = mrbc_float(v[0]);
  mrbc_float_t right;
  switch (mrbc_type(v[1])) {
  case MRBC_TT_INTEGER: right = (mrbc_float_t)mrbc_integer(v[1]); break;
  case MRBC_TT_FLOAT: right = mrbc_float(v[1]); break;
  default: return;
  }
  if (right == 0.0) {
    mrbc_raise(vm, MRBC_CLASS(ZeroDivisionError), 0);
    return;
  }
  mrbc_float_t mod = fmod(left, right);
  if (mod != 0.0 && (left < 0.0) != (right < 0.0)) mod += right;
  SET_FLOAT_RETURN(mod);
}

void
mrbc_numeric_ext_init(mrbc_vm *vm)
{
  mrbc_class *fl = mrbc_get_class_by_name("Float");
  mrbc_define_method(vm, fl, "round", c_flo_round);
  mrbc_define_method(vm, fl, "floor", c_flo_floor);
  mrbc_define_method(vm, fl, "ceil",  c_flo_ceil);
  mrbc_define_method(vm, fl, "%",     c_flo_mod);
}
