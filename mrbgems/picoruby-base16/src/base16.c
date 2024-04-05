#include <mrubyc.h>
#include <alloc.h>

const char base16chars[] = "0123456789abcdef";

static void
c_base16_encode(mrbc_vm *vm, mrbc_value *v, int argc)
{
  size_t index = 0;
  size_t in_size, out_size;
  uint8_t n, n0, n1;

  mrbc_value input = GET_ARG(1);
  if (input.tt != MRBC_TT_STRING) {
    mrbc_raise(vm, MRBC_CLASS(TypeError), "wrong type of argument");
    return;
  }

  in_size  = input.string->size;
  out_size = in_size * 2;
  uint8_t* output = mrbc_alloc(vm, out_size);

  for (index = 0; index < in_size; index++) {
    n = input.string->data[index];
    n0 = (n >> 4) & 15;
    n1 = n & 15;

    output[index * 2] = base16chars[n0];
    output[index * 2 + 1] = base16chars[n1];
  }

  mrbc_value ret = mrbc_string_new(vm, output, out_size);
  mrbc_free(vm, output);
  SET_RETURN(ret);
}

unsigned char convert(unsigned char n) {
  if (n >= 0x30 && n <= 0x39) {
    return n - 0x30;
  } else if (n >= 0x41 && n <= 0x46) {
    return n - 55; // 0x41(A) = 65 should return 10
  } else if (n >= 0x61 && n <= 0x66) {
    return n - 87; // 0x61(a) = 97 should return 10
  } else {
    return 0;
  }
}

static void
c_base16_decode(mrbc_vm *vm, mrbc_value *v, int argc)
{
  size_t index = 0;
  size_t in_size, out_size;
  uint8_t n0, n1;

  mrbc_value input = GET_ARG(1);
  if (input.tt != MRBC_TT_STRING) {
    mrbc_raise(vm, MRBC_CLASS(TypeError), "wrong type of argument");
    return;
  }

  in_size  = input.string->size;
  out_size = in_size / 2;
  // Base16 in RFC4648 assumes input size to be even,
  // but Ruby's pack('H*') treats as if there is a 0 at the end
  if (in_size % 2 == 1) {
    out_size += 1;
  }
  uint8_t* output = mrbc_alloc(vm, out_size);

  for (index = 0; index < out_size; index++) {
    n0 = convert(input.string->data[index * 2]);
    if (index * 2 + 1 < in_size) {
      n1 = convert(input.string->data[index * 2 + 1]);
    } else {
      n1 = 0;
    }
    output[index] = (n0 << 4) + n1;
  }

  mrbc_value ret = mrbc_string_new(vm, output, out_size);
  mrbc_free(vm, output);
  SET_RETURN(ret);
}

void
mrbc_base16_init(void)
{
  mrbc_class *class_Base16 = mrbc_define_class(0, "Base16", mrbc_class_object);

  mrbc_define_method(0, class_Base16, "encode16", c_base16_encode);
  mrbc_define_method(0, class_Base16, "decode16", c_base16_decode);
}