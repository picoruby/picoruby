#include <mrubyc.h>

static void
c_base64_encode(mrbc_vm *vm, mrbc_value *v, int argc)
{
  size_t index = 0;
  size_t out_index = 0;
  size_t in_size, out_size;
  uint32_t n = 0;
  uint8_t n0, n1, n2, n3;

  mrbc_value input = GET_ARG(1);
  if (input.tt != MRBC_TT_STRING) {
    mrbc_raise(vm, MRBC_CLASS(TypeError), "wrong type of argument");
    return;
  }

  in_size  = input.string->size;
  out_size = (in_size + 2) / 3 * 4 + 1;
  uint8_t* output = mrbc_alloc(vm, out_size);
  uint8_t* data   = input.string->data;

  // create the main encoding
  for (index = 0; index < in_size; index += 3) {
    n = ((uint32_t)data[index]) << 16;

    if ((index + 1) < in_size)
      n += ((uint32_t)data[index + 1]) << 8;

    if ((index + 2) < in_size)
      n += data[index + 2];

    n0 = (uint8_t)(n >> 18) & 63;
    n1 = (uint8_t)(n >> 12) & 63;
    n2 = (uint8_t)(n >> 6)  & 63;
    n3 = (uint8_t)n         & 63;

    // generate 2 chars from 1 byte
    output[out_index++] = base64chars[n0];
    output[out_index++] = base64chars[n1];
    // if 2nd byte, generate 3rd char
    if(index + 1 < in_size)
      output[out_index++] = base64chars[n2];
    // if 3rd byte, generate 4th char
    if(index + 2 < in_size)
      output[out_index++] = base64chars[n3];
  }

  // create padding
  int padding = in_size % 3;
  if (padding > 0) {
    for (; padding < 3; padding++) {
      output[out_index++] = '=';
    }
  }

  mrbc_value ret = mrbc_string_new_alloc(vm, output, out_size - 1); // remove the termination byte at the end
  SET_RETURN(ret);
}

static void
c_base64_decode(mrbc_vm *vm, mrbc_value *v, int argc)
{
  size_t index = 0;
  size_t out_index = 0;
  size_t in_size, out_size;
  size_t actual_out_size = 0;
  uint32_t buf = 0;
  int iter = 0;

  mrbc_value input = GET_ARG(1);
  if (input.tt != MRBC_TT_STRING) {
    mrbc_raise(vm, MRBC_CLASS(TypeError), "wrong type of argument");
    return;
  }

  in_size  = input.string->size;
  out_size = (in_size + 3) / 4 * 3 + 1;
  uint8_t* data   = input.string->data;
  uint8_t* output = mrbc_alloc(vm, out_size);

  for (; index < in_size; index++) {
    if (data[index] == '=')
      break;
    unsigned char c = base64_convert(data[index]);
    buf = buf << 6 | c;
    iter++;
    if (iter == 4) {
      output[out_index++] = (buf >> 16) & 255;
      output[out_index++] = (buf >> 8)  & 255;
      output[out_index++] = buf         & 255;
      actual_out_size += 3;
      buf = 0;
      iter = 0;
    }
  }

  if (iter == 3) {
    output[out_index++] = (buf >> 10) & 255;
    output[out_index++] = (buf >>  2) & 255;
    actual_out_size += 2;
  } else if (iter == 2) {
    output[out_index++] = (buf >> 4) & 255;
    actual_out_size += 1;
  }

  mrbc_value ret = mrbc_string_new_alloc(vm, output, actual_out_size);
  SET_RETURN(ret);
}

void
mrbc_base64_init(mrbc_vm *vm)
{
  mrbc_class *class_Base64 = mrbc_define_class(vm, "Base64", mrbc_class_object);

  mrbc_define_method(vm, class_Base64, "encode64", c_base64_encode);
  mrbc_define_method(vm, class_Base64, "decode64", c_base64_decode);
}

