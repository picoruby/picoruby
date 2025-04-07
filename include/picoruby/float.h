#ifndef PICORB_FLOAT_DEFINED_H_
#define PICORB_FLOAT_DEFINED_H_

#ifdef __cplusplus
extern "C" {
#endif


#if defined(PICORB_VM_MRUBY)

#if defined(MRB_NO_FLOAT)
#define PICORB_NO_FLOAT 1
#else
#include "mruby/value.h"
typedef mrb_float picorb_float_t;
#endif

#elif defined(PICORB_VM_MRUBYC)

#if defined(MRBC_USE_FLOAT)
#include "mrubyc.h"
typedef mrbc_float_t picorb_float_t;
#else
#define PICORB_NO_FLOAT 1
#endif

#endif


#ifdef __cplusplus
}
#endif

#endif // PICORB_FLOAT_DEFINED_H_
