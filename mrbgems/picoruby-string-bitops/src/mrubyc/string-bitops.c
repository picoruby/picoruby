#include <mrubyc.h>

/*
 * mruby/c passes only positional arguments in argc; a keyword-argument
 * Hash, when given, sits at v[argc + 1] (followed by the block slot,
 * which is nil or a Proc, never a Hash).
 */
static mrbc_value *
bitop_kwargs(mrbc_value v[], int argc)
{
  if (v[argc + 1].tt == MRBC_TT_HASH) {
    return &v[argc + 1];
  }
  return NULL;
}

/*
 * Scans the offset argument and the optional lsb_first keyword.
 * Returns 1 on success, 0 after raising.
 */
static int
bitop_scan_offset(mrbc_vm *vm, mrbc_value v[], int argc, mrbc_int_t *offset, int *lsb_first)
{
  mrbc_value *kwargs = bitop_kwargs(v, argc);

  *lsb_first = 1;
  if (kwargs) {
    int pairs = kwargs->hash->n_stored / 2;
    /* mrbc_hash_search_by_id returns a pointer to the key; the value
       occupies the next slot. */
    mrbc_value *key = mrbc_hash_search_by_id(kwargs, mrbc_str_to_symid("lsb_first"));
    mrbc_value *val = key ? key + 1 : NULL;
    if (val == NULL) {
      if (pairs > 0) {
        mrbc_raise(vm, MRBC_CLASS(ArgumentError), "unknown keyword");
        return 0;
      }
    }
    else if (pairs > 1) {
      mrbc_raise(vm, MRBC_CLASS(ArgumentError), "unknown keyword");
      return 0;
    }
    else if (val->tt == MRBC_TT_TRUE) {
      /* default */
    }
    else if (val->tt == MRBC_TT_FALSE) {
      *lsb_first = 0;
    }
    else {
      mrbc_raise(vm, MRBC_CLASS(ArgumentError), "lsb_first must be true or false");
      return 0;
    }
  }
  if (argc != 1) {
    mrbc_raise(vm, MRBC_CLASS(ArgumentError), "wrong number of arguments");
    return 0;
  }
  if (v[1].tt != MRBC_TT_INTEGER) {
    mrbc_raise(vm, MRBC_CLASS(TypeError), "offset must be an Integer");
    return 0;
  }
  *offset = v[1].i;
  return 1;
}

static mrbc_int_t
bitop_physical_index(mrbc_int_t logical, int lsb_first)
{
  if (lsb_first) return logical;
  return (logical & ~(mrbc_int_t)7) | (7 - (logical & 7));
}

/* Returns 0 or 1, -1 when offset is beyond the end, -2 after raising. */
static int
bitop_get_bit(mrbc_vm *vm, mrbc_value v[], int argc)
{
  mrbc_int_t offset, physical;
  int lsb_first;

  if (!bitop_scan_offset(vm, v, argc, &offset, &lsb_first)) {
    return -2;
  }
  if (offset < 0) {
    mrbc_raise(vm, MRBC_CLASS(IndexError), "bit index out of range");
    return -2;
  }
  /* Compare byte indexes to avoid overflowing size * 8.  The int64_t
     width holds both operands even when mrbc_int_t is narrower than
     MRBC_STRING_SIZE_T (e.g. MRBC_INT16). */
  if ((int64_t)(offset / 8) >= (int64_t)v[0].string->size) {
    return -1;
  }
  physical = bitop_physical_index(offset, lsb_first);
  return (v[0].string->data[physical / 8] >> (physical % 8)) & 1;
}

static void
c_string_bit_get(mrbc_vm *vm, mrbc_value v[], int argc)
{
  int bit = bitop_get_bit(vm, v, argc);
  if (bit == -2) return;
  if (bit < 0) {
    SET_NIL_RETURN();
  }
  else {
    SET_INT_RETURN(bit);
  }
}

static void
c_string_bit_set_p(mrbc_vm *vm, mrbc_value v[], int argc)
{
  int bit = bitop_get_bit(vm, v, argc);
  if (bit == -2) return;
  if (bit < 0) {
    SET_NIL_RETURN();
  }
  else {
    SET_BOOL_RETURN(bit);
  }
}

enum bitop_mutation {
  BITOP_MUT_SET,
  BITOP_MUT_CLEAR,
  BITOP_MUT_FLIP
};

/* Mutates self in place and leaves v[0] untouched, so self is returned. */
static void
bitop_mutate(mrbc_vm *vm, mrbc_value v[], int argc, enum bitop_mutation mutation)
{
  mrbc_int_t offset, physical;
  int lsb_first;
  uint8_t *ptr;
  uint8_t mask;

  if (!bitop_scan_offset(vm, v, argc, &offset, &lsb_first)) {
    return;
  }
  if (offset < 0 || (int64_t)(offset / 8) >= (int64_t)v[0].string->size) {
    mrbc_raise(vm, MRBC_CLASS(IndexError), "bit index out of range");
    return;
  }
  physical = bitop_physical_index(offset, lsb_first);
  ptr = v[0].string->data;
  mask = (uint8_t)(1u << (physical % 8));
  switch (mutation) {
  case BITOP_MUT_SET:
    ptr[physical / 8] |= mask;
    break;
  case BITOP_MUT_CLEAR:
    ptr[physical / 8] &= (uint8_t)~mask;
    break;
  case BITOP_MUT_FLIP:
    ptr[physical / 8] ^= mask;
    break;
  }
}

static void
c_string_bit_set(mrbc_vm *vm, mrbc_value v[], int argc)
{
  bitop_mutate(vm, v, argc, BITOP_MUT_SET);
}

static void
c_string_bit_clear(mrbc_vm *vm, mrbc_value v[], int argc)
{
  bitop_mutate(vm, v, argc, BITOP_MUT_CLEAR);
}

static void
c_string_bit_flip(mrbc_vm *vm, mrbc_value v[], int argc)
{
  bitop_mutate(vm, v, argc, BITOP_MUT_FLIP);
}

/* Validates that no positional nor keyword argument was given. */
static int
bitop_check_no_args(mrbc_vm *vm, mrbc_value v[], int argc)
{
  if (argc != 0 || bitop_kwargs(v, argc) != NULL) {
    mrbc_raise(vm, MRBC_CLASS(ArgumentError), "wrong number of arguments");
    return 0;
  }
  return 1;
}

static void
c_string_bit_count(mrbc_vm *vm, mrbc_value v[], int argc)
{
  uint32_t count;

  if (!bitop_check_no_args(vm, v, argc)) {
    return;
  }
  count = bitop_count_bits(v[0].string->data, (int)v[0].string->size);
  /* The count fits in uint32_t, but not necessarily in mrbc_int_t:
     under MRBC_INT16 a 4096-byte all-ones string already exceeds it.
     Out-of-range conversion to a signed type is implementation-
     defined, so compare against the mrbc_int_t maximum before
     casting.  The comparison is done in uint64_t, which holds both
     operands for every MRBC_INT configuration. */
  if ((uint64_t)count > (uint64_t)((mrbc_uint_t)-1 >> 1)) {
    mrbc_raise(vm, MRBC_CLASS(RangeError), "bit count exceeds Integer range");
    return;
  }
  SET_INT_RETURN((mrbc_int_t)count);
}

static void
c_string_bitwise_not(mrbc_vm *vm, mrbc_value v[], int argc)
{
  int len;
  mrbc_value result;

  if (!bitop_check_no_args(vm, v, argc)) {
    return;
  }
  len = (int)v[0].string->size;
  result = mrbc_string_new(vm, NULL, len);
  bitop_not_kernel(result.string->data, v[0].string->data, len);
  result.string->data[len] = '\0';
  SET_RETURN(result);
}

static void
c_string_bitwise_not_bang(mrbc_vm *vm, mrbc_value v[], int argc)
{
  uint8_t *ptr;

  if (!bitop_check_no_args(vm, v, argc)) {
    return;
  }
  ptr = v[0].string->data;
  bitop_not_kernel(ptr, ptr, (int)v[0].string->size);
  /* v[0] left untouched: returns self */
}

/* Validates the operand of a binary operation.  Returns 1 on success. */
static int
bitop_binary_operand(mrbc_vm *vm, mrbc_value v[], int argc)
{
  if (argc != 1 || bitop_kwargs(v, argc) != NULL) {
    mrbc_raise(vm, MRBC_CLASS(ArgumentError), "wrong number of arguments");
    return 0;
  }
  if (v[1].tt != MRBC_TT_STRING) {
    mrbc_raise(vm, MRBC_CLASS(TypeError), "operand must be a String");
    return 0;
  }
  if (v[1].string->size != v[0].string->size) {
    mrbc_raisef(vm, MRBC_CLASS(ArgumentError), "operands must have the same length (%d vs %d)",
                (int)v[0].string->size, (int)v[1].string->size);
    return 0;
  }
  return 1;
}

typedef void (*bitop_binary_kernel)(uint8_t*, const uint8_t*, const uint8_t*, int);

static void
bitop_binary(mrbc_vm *vm, mrbc_value v[], int argc, bitop_binary_kernel kernel)
{
  int len;
  mrbc_value result;

  if (!bitop_binary_operand(vm, v, argc)) {
    return;
  }
  len = (int)v[0].string->size;
  result = mrbc_string_new(vm, NULL, len);
  kernel(result.string->data, v[0].string->data, v[1].string->data, len);
  result.string->data[len] = '\0';
  SET_RETURN(result);
}

static void
bitop_binary_bang(mrbc_vm *vm, mrbc_value v[], int argc, bitop_binary_kernel kernel)
{
  uint8_t *ptr;

  if (!bitop_binary_operand(vm, v, argc)) {
    return;
  }
  ptr = v[0].string->data;
  kernel(ptr, ptr, v[1].string->data, (int)v[0].string->size);
  /* v[0] left untouched: returns self */
}

static void
c_string_bitwise_and(mrbc_vm *vm, mrbc_value v[], int argc)
{
  bitop_binary(vm, v, argc, bitop_and_kernel);
}

static void
c_string_bitwise_and_bang(mrbc_vm *vm, mrbc_value v[], int argc)
{
  bitop_binary_bang(vm, v, argc, bitop_and_kernel);
}

static void
c_string_bitwise_or(mrbc_vm *vm, mrbc_value v[], int argc)
{
  bitop_binary(vm, v, argc, bitop_or_kernel);
}

static void
c_string_bitwise_or_bang(mrbc_vm *vm, mrbc_value v[], int argc)
{
  bitop_binary_bang(vm, v, argc, bitop_or_kernel);
}

static void
c_string_bitwise_xor(mrbc_vm *vm, mrbc_value v[], int argc)
{
  bitop_binary(vm, v, argc, bitop_xor_kernel);
}

static void
c_string_bitwise_xor_bang(mrbc_vm *vm, mrbc_value v[], int argc)
{
  bitop_binary_bang(vm, v, argc, bitop_xor_kernel);
}

void
mrbc_string_bitops_init(mrbc_vm *vm)
{
  mrbc_class *s = MRBC_CLASS(String);

  mrbc_define_method(vm, s, "bit_get", c_string_bit_get);
  mrbc_define_method(vm, s, "bit_set?", c_string_bit_set_p);
  mrbc_define_method(vm, s, "bit_set", c_string_bit_set);
  mrbc_define_method(vm, s, "bit_clear", c_string_bit_clear);
  mrbc_define_method(vm, s, "bit_flip", c_string_bit_flip);
  mrbc_define_method(vm, s, "bit_count", c_string_bit_count);
  mrbc_define_method(vm, s, "bitwise_not", c_string_bitwise_not);
  mrbc_define_method(vm, s, "bitwise_not!", c_string_bitwise_not_bang);
  mrbc_define_method(vm, s, "bitwise_and", c_string_bitwise_and);
  mrbc_define_method(vm, s, "bitwise_and!", c_string_bitwise_and_bang);
  mrbc_define_method(vm, s, "bitwise_or", c_string_bitwise_or);
  mrbc_define_method(vm, s, "bitwise_or!", c_string_bitwise_or_bang);
  mrbc_define_method(vm, s, "bitwise_xor", c_string_bitwise_xor);
  mrbc_define_method(vm, s, "bitwise_xor!", c_string_bitwise_xor_bang);
}
