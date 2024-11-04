#ifndef ENV_DEFINED_H_
#define ENV_DEFINED_H_

#include <mrubyc.h>

void c_env_new(struct VM *vm, mrbc_value v[], int argc);

#endif

