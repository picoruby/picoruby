#ifndef PICORUBY_H
#define PICORUBY_H

#include "version.h"

#if !defined(PICORB_VM_MRUBYC) && !defined(PICORB_VM_MRUBY)
# define PICORB_VM_MRUBY
#endif

#include <mrc_common.h>
#include <mrc_ccontext.h>
#include <mrc_compile.h>
#include <mrc_dump.h>

#if defined(PICORB_VM_MRUBYC)

#include <mrubyc.h>
#include <picogem_init.c>

#define picorb_bool     mrc_bool
#define picorb_alloc    mrbc_raw_alloc
#define picorb_realloc  mrbc_raw_realloc
#define picorb_free     mrbc_raw_free
#define picorb_array_new(vm,size)       mrbc_array_new(vm,size)
#define picorb_array_push(mrb,a,v)      mrbc_array_push(a,v)
#define picorb_string_new(vm,src,len)   mrbc_string_new(vm,src,len)
#define picorb_bool_value(n)            mrbc_bool_value(n)
#define picorb_define_const(mrb,name,value) \
        mrbc_set_const(mrbc_str_to_symid(name),value)
#define picorb_define_global_const(mrb,name,value) \
        mrbc_set_global(mrbc_str_to_symid(name),value)

#elif defined(PICORB_VM_MRUBY)

#define picorb_value mrb_value

#undef mrb_state
#define MRUBY_IREP_H 1
#include <mruby.h>
#include <mruby/value.h>
#include <mruby/proc.h>
#include <mruby/array.h>
#include <mruby/variable.h>
#include <mruby/presym.h>
#include <mruby/error.h>

#define picorb_bool     mrb_bool
#define picorb_alloc    malloc
#define picorb_realloc  realloc
#define picorb_free     free

#define picorb_array_new(mrb,size)      mrb_ary_new_capa(mrb,size)
#define picorb_array_push(mrb,a,v)      mrb_ary_push(mrb,a,v)
#define picorb_string_new(mrb,src,len)  mrb_str_new(mrb,src,len)
#define picorb_bool_value(n)            mrb_bool_value(n)
#define picorb_define_const(mrb,name,value) \
          mrb_define_global_const(mrb,name,value)
#define picorb_define_global_const(mrb,name,value) \
          mrb_define_global_const(mrb,name,value)

#else /* PICORB_VM_MRUBY */

# error "This should not happen"

#endif

#endif // PICORUBY_H
