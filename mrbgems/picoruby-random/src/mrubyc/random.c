#include <stdint.h>

#include <mrubyc.h>

/*
 * Random of FemtoRuby: a subset of the Random of CRuby. All values come
 * from the source of the platform (picorb_random_bytes). There is no seeded
 * generator, so there is no srand and no Random.new. The rand methods take
 * the arguments of mruby-random, the Random of PicoRuby.
 */

static int
random_fill(mrbc_vm *vm, void *buf, size_t len)
{
  if (picorb_random_bytes(buf, len) != 0) {
    mrbc_raise(vm, MRBC_CLASS(RuntimeError), "failed to get urandom");
    return -1;
  }
  return 0;
}

/* Uniform value in [0, span) without modulo bias. span 0 means 2**64. */
static int
random_below(mrbc_vm *vm, uint64_t span, uint64_t *out)
{
  uint64_t r;
  if (span == 0) return random_fill(vm, out, sizeof(*out));
  uint64_t threshold = (0 - span) % span; /* == 2**64 % span */
  do {
    if (random_fill(vm, &r, sizeof(r)) != 0) return -1;
  } while (r < threshold);
  *out = r % span;
  return 0;
}

#if MRBC_USE_FLOAT
/* Uniform value in [0.0, 1.0) */
static int
random_real(mrbc_vm *vm, mrbc_float_t *out)
{
  uint64_t r;
  if (random_fill(vm, &r, sizeof(r)) != 0) return -1;
#if MRBC_USE_FLOAT == 1
  *out = (mrbc_float_t)(r >> 40) * (1.0f / 16777216.0f);
#else
  *out = (mrbc_float_t)(r >> 11) * (1.0 / 9007199254740992.0);
#endif
  return 0;
}
#endif

/* Integer in [0, |max|), or Float in [0.0, 1.0) when max is 0 */
static void
random_rand_int(mrbc_vm *vm, mrbc_value v[], mrbc_int_t max)
{
  if (max == 0) {
#if MRBC_USE_FLOAT
    mrbc_float_t f;
    if (random_real(vm, &f) != 0) return;
    SET_FLOAT_RETURN(f);
    return;
#else
    max = 100;
#endif
  }
  int64_t m = (int64_t)max;
  uint64_t lim = m < 0 ? 0 - (uint64_t)m : (uint64_t)m;
  uint64_t r;
  if (random_below(vm, lim, &r) != 0) return;
  SET_INT_RETURN((mrbc_int_t)r);
}

static void
random_rand_range(mrbc_vm *vm, mrbc_value v[], mrbc_value *range)
{
  mrbc_value first = mrbc_range_first(range);
  mrbc_value last = mrbc_range_last(range);
  int excl = mrbc_range_exclude_end(range);

  if (first.tt == MRBC_TT_INTEGER && last.tt == MRBC_TT_INTEGER) {
    int64_t b = (int64_t)first.i;
    int64_t e = (int64_t)last.i;
    /* a reversed or empty range gives nil, as CRuby does */
    if (e < b || (excl && b == e)) {
      SET_NIL_RETURN();
      return;
    }
    /* an inclusive range over all of int64 wraps to 0, the whole domain */
    uint64_t span = (uint64_t)e - (uint64_t)b + (excl ? 0 : 1);
    uint64_t r;
    if (random_below(vm, span, &r) != 0) return;
    SET_INT_RETURN((mrbc_int_t)(int64_t)((uint64_t)b + r));
    return;
  }

#if MRBC_USE_FLOAT
  if ((first.tt == MRBC_TT_INTEGER || first.tt == MRBC_TT_FLOAT) &&
      (last.tt == MRBC_TT_INTEGER || last.tt == MRBC_TT_FLOAT)) {
    mrbc_float_t b = first.tt == MRBC_TT_FLOAT ? first.d : (mrbc_float_t)first.i;
    mrbc_float_t e = last.tt == MRBC_TT_FLOAT ? last.d : (mrbc_float_t)last.i;
    mrbc_float_t span = e - b;
    if (span <= 0) {
      SET_NIL_RETURN();
      return;
    }
    mrbc_float_t f;
    if (random_real(vm, &f) != 0) return;
    SET_FLOAT_RETURN(f * span + b);
    return;
  }
#endif

  mrbc_raise(vm, MRBC_CLASS(TypeError), "no implicit conversion into Integer");
}

/*
 * Random.rand rejects a negative limit, as CRuby does. Kernel#rand
 * (kernel == 1) takes its absolute value instead.
 */
static void
random_rand_impl(mrbc_vm *vm, mrbc_value v[], int argc, int kernel)
{
  if (1 < argc) {
    mrbc_raise(vm, MRBC_CLASS(ArgumentError), "wrong number of arguments");
    return;
  }
  if (argc == 0 || v[1].tt == MRBC_TT_NIL) {
    random_rand_int(vm, v, 0);
    return;
  }

  switch (v[1].tt) {
  case MRBC_TT_INTEGER:
    if (!kernel && v[1].i < 0) break;
    random_rand_int(vm, v, v[1].i);
    return;

#if MRBC_USE_FLOAT
  case MRBC_TT_FLOAT: {
    mrbc_float_t d = v[1].d;
    if (!kernel && d < 0) break;
    /* the limit of mrbc_int_t; NaN fails both comparisons */
    mrbc_float_t limit = (mrbc_float_t)((uint64_t)1 << (sizeof(mrbc_int_t) * 8 - 1));
    if (!(-limit <= d && d < limit)) {
      mrbc_raise(vm, MRBC_CLASS(RangeError), "float out of range of integer");
      return;
    }
    random_rand_int(vm, v, (mrbc_int_t)d);
    return;
  }
#endif

  case MRBC_TT_RANGE:
    random_rand_range(vm, v, &v[1]);
    return;

  default:
    mrbc_raise(vm, MRBC_CLASS(TypeError), "no implicit conversion into Integer");
    return;
  }

  mrbc_raise(vm, MRBC_CLASS(ArgumentError), "invalid argument");
}

static void
c_random_rand(mrbc_vm *vm, mrbc_value v[], int argc)
{
  random_rand_impl(vm, v, argc, 0);
}

static void
c_kernel_rand(mrbc_vm *vm, mrbc_value v[], int argc)
{
  random_rand_impl(vm, v, argc, 1);
}

/* Random.urandom(size) and Random.bytes(size): a String of size bytes */
static void
c_random_urandom(mrbc_vm *vm, mrbc_value v[], int argc)
{
  if (argc != 1) {
    mrbc_raise(vm, MRBC_CLASS(ArgumentError), "wrong number of arguments");
    return;
  }
  if (v[1].tt != MRBC_TT_INTEGER) {
    mrbc_raise(vm, MRBC_CLASS(TypeError), "no implicit conversion into Integer");
    return;
  }
  mrbc_int_t len = v[1].i;
  if (len < 0) {
    mrbc_raise(vm, MRBC_CLASS(ArgumentError), "negative string size");
    return;
  }
  /* mrbc_alloc takes an unsigned int, and the String length is an int */
  if ((mrbc_int_t)0x7ffffffe < len) {
    mrbc_raise(vm, MRBC_CLASS(ArgumentError), "string size too big");
    return;
  }
  uint8_t *buf = mrbc_alloc(vm, (unsigned int)len + 1);
  if (buf == NULL) {
    mrbc_raise(vm, MRBC_CLASS(NoMemoryError), "failed to allocate memory");
    return;
  }
  if (0 < len && random_fill(vm, buf, (size_t)len) != 0) {
    mrbc_free(vm, buf);
    return;
  }
  buf[len] = '\0';
  mrbc_value ret = mrbc_string_new_alloc(vm, buf, (int)len);
  SET_RETURN(ret);
}

void
mrbc_random_init(mrbc_vm *vm)
{
  mrbc_class *class_Random = mrbc_define_class(vm, "Random", mrbc_class_object);

  mrbc_define_method(vm, class_Random, "urandom", c_random_urandom);
  mrbc_define_method(vm, class_Random, "bytes", c_random_urandom);
  mrbc_define_method(vm, class_Random, "rand", c_random_rand);
  mrbc_define_method(vm, mrbc_class_object, "rand", c_kernel_rand);
}
