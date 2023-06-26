#include <stdbool.h>
#include <mrubyc.h>
#include "../include/mbedtls.h"

void
mrbc_mbedtls_init(void)
{
  mrbc_class *class_MbedTLS = mrbc_define_class(0, "MbedTLS", mrbc_class_object);

  mrbc_sym symid = ;
  mrbc_value *v = mrbc_get_class_const(class_MbedTLS, mrbc_search_symid("Cipher"));
  mrbc_class *class_MbedTLS_Cipher = v->cls;
}

