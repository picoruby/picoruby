#include <mrubyc.h>

static void
base64_encode_value(mrbc_vm *vm, mrbc_value *v, size_t line_len)
{
  mrbc_value input = GET_ARG(1);
  if (input.tt != MRBC_TT_STRING) {
    mrbc_raise(vm, MRBC_CLASS(TypeError), "wrong type of argument");
    return;
  }

  size_t len = input.string->size;
  uint8_t *output = mrbc_alloc(vm, base64_encoded_size(len, line_len) + 1);
  if (!output) {
    mrbc_raise(vm, MRBC_CLASS(StandardError), "failed to allocate memory");
    return;
  }
  size_t size = base64_encode(input.string->data, len, output, line_len);

  mrbc_value ret = mrbc_string_new_alloc(vm, output, (int)size);
  SET_RETURN(ret);
}

static void
base64_decode_value(mrbc_vm *vm, mrbc_value *v, int strict)
{
  mrbc_value input = GET_ARG(1);
  if (input.tt != MRBC_TT_STRING) {
    mrbc_raise(vm, MRBC_CLASS(TypeError), "wrong type of argument");
    return;
  }

  size_t len = input.string->size;
  uint8_t *output = mrbc_alloc(vm, base64_decoded_capa(len));
  if (!output) {
    mrbc_raise(vm, MRBC_CLASS(StandardError), "failed to allocate memory");
    return;
  }
  size_t size;
  if (base64_decode(input.string->data, len, output, &size, strict) < 0) {
    mrbc_free(vm, output);
    mrbc_raise(vm, MRBC_CLASS(ArgumentError), "invalid base64");
    return;
  }

  mrbc_value ret = mrbc_string_new_alloc(vm, output, (int)size);
  SET_RETURN(ret);
}

/* RFC 2045: a newline after every 60 characters and at the end */
static void
c_base64_encode64(mrbc_vm *vm, mrbc_value *v, int argc)
{
  base64_encode_value(vm, v, BASE64_LINE_LEN);
}

static void
c_base64_strict_encode64(mrbc_vm *vm, mrbc_value *v, int argc)
{
  base64_encode_value(vm, v, 0);
}

static void
c_base64_decode64(mrbc_vm *vm, mrbc_value *v, int argc)
{
  base64_decode_value(vm, v, 0);
}

static void
c_base64_strict_decode64(mrbc_vm *vm, mrbc_value *v, int argc)
{
  base64_decode_value(vm, v, 1);
}

void
mrbc_base64_init(mrbc_vm *vm)
{
  mrbc_class *class_Base64 = mrbc_define_class(vm, "Base64", mrbc_class_object);

  mrbc_define_method(vm, class_Base64, "encode64", c_base64_encode64);
  mrbc_define_method(vm, class_Base64, "strict_encode64", c_base64_strict_encode64);
  mrbc_define_method(vm, class_Base64, "decode64", c_base64_decode64);
  mrbc_define_method(vm, class_Base64, "strict_decode64", c_base64_strict_decode64);
}
