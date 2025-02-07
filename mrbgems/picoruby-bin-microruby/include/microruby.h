#ifndef MICRORUBY_DEFINED_H_
#define MICRORUBY_DEFINED_H_

#include <mrc_common.h>
#include <mrc_ccontext.h>
#include <mrc_compile.h>
#include <mrc_dump.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef mrc_bool      mrb_bool;
//typedef mrc_ccontext  mrb_ccontext;
//typedef mrc_irep      mrb_irep;

#define mrbc_raw_alloc    malloc
#define mrbc_raw_realloc  realloc
#define mrbc_raw_free     free

#ifdef __cplusplus
}
#endif

#endif /* MICRORUBY_DEFINED_H_ */


