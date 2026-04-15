#include "bdffont_mrubyc.h"

static void
c_s_10(mrbc_vm *vm, mrbc_value *v, int argc)
{
  const char *text = (const char *)GET_STRING_ARG(1);
  mrbc_int_t scale = (argc > 1) ? GET_INT_ARG(2) : 1;
  mrbc_value result = bdffont_render(vm, text, scale,
                                     KARMATIC_ARCADE_HEIGHT,
                                     array_of_karmatic_arcade_sub);
  SET_RETURN(result);
}

void
mrbc_karmatic_arcade_init(mrbc_vm *vm)
{
  mrbc_class *mod = mrbc_define_module(vm, "KarmaticArcade");
  mrbc_define_method(vm, mod, "_10", c_s_10);
}
