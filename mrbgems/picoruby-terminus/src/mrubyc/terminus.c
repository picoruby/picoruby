#include "bdffont_mrubyc.h"

static void
c_s_6x12(mrbc_vm *vm, mrbc_value *v, int argc)
{
  const char *text = (const char *)GET_STRING_ARG(1);
  mrbc_int_t scale = (argc > 1) ? GET_INT_ARG(2) : 1;
  mrbc_value result = bdffont_render(vm, text, scale, 12, array_of_terminus_6x12);
  SET_RETURN(result);
}

static void
c_s_8x16(mrbc_vm *vm, mrbc_value *v, int argc)
{
  const char *text = (const char *)GET_STRING_ARG(1);
  mrbc_int_t scale = (argc > 1) ? GET_INT_ARG(2) : 1;
  mrbc_value result = bdffont_render(vm, text, scale, 16, array_of_terminus_8x16);
  SET_RETURN(result);
}

static void
c_s_12x24(mrbc_vm *vm, mrbc_value *v, int argc)
{
  const char *text = (const char *)GET_STRING_ARG(1);
  mrbc_int_t scale = (argc > 1) ? GET_INT_ARG(2) : 1;
  mrbc_value result = bdffont_render(vm, text, scale, 24, array_of_terminus_12x24);
  SET_RETURN(result);
}

static void
c_s_16x32(mrbc_vm *vm, mrbc_value *v, int argc)
{
  const char *text = (const char *)GET_STRING_ARG(1);
  mrbc_int_t scale = (argc > 1) ? GET_INT_ARG(2) : 1;
  mrbc_value result = bdffont_render(vm, text, scale, 32, array_of_terminus_16x32);
  SET_RETURN(result);
}

void
mrbc_terminus_init(mrbc_vm *vm)
{
  mrbc_class *module_Terminus = mrbc_define_module(vm, "Terminus");

  mrbc_define_method(vm, module_Terminus, "_6x12",  c_s_6x12);
  mrbc_define_method(vm, module_Terminus, "_8x16",  c_s_8x16);
  mrbc_define_method(vm, module_Terminus, "_12x24", c_s_12x24);
  mrbc_define_method(vm, module_Terminus, "_16x32", c_s_16x32);
}
