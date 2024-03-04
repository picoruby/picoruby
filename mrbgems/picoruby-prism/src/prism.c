#include <mrubyc.h>
#include "prism.h" // in lib/prism/include

#define GETIV(str)      mrbc_instance_getiv(&v[0], mrbc_str_to_symid(#str))
#define SETIV(str, val) mrbc_instance_setiv(&v[0], mrbc_str_to_symid(#str), val)

static void
c_parse(mrbc_vm *vm, mrbc_value v[], int argc)
{
  if (argc != 1) {
    mrbc_raise(vm, MRBC_CLASS(ArgumentError), "wrong number of arguments");
    return;
  }
  if (GET_TT_ARG(1) != MRBC_TT_STRING) {
    mrbc_raise(vm, MRBC_CLASS(TypeError), "wrong argument type");
    return;
  }

  pm_parser_t parser;
  mrbc_value source = GET_ARG(1);
  pm_parser_init(&parser, (const uint8_t *)source.string->data, source.string->size, NULL);
  pm_node_t *root = pm_parse(&parser);
  pm_buffer_t buffer = { 0 };
  pm_prettyprint(&buffer, &parser, root);
  mrbc_value str = mrbc_string_new_cstr(vm, buffer.value);
  SET_RETURN(str);
  pm_buffer_free(&buffer);
  pm_node_destroy(&parser, root);
  pm_parser_free(&parser);
}

void
mrbc_prism_init(void)
{
  mrbc_class *mrbc_class_Prism = mrbc_define_class(0, "Prism", mrbc_class_object);

  mrbc_define_method(0, mrbc_class_Prism, "parse", c_parse);
}
