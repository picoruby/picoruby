#include <picorbc.h>
#include <picogem_init.c>
#include "ruby.c"

#define MRBC_MEMORY_SIZE (1024*1024)
static uint8_t memory_pool[MRBC_MEMORY_SIZE];

int loglevel = LOGLEVEL_FATAL;

int
main(void)
{
  mrbc_init(memory_pool, MRBC_MEMORY_SIZE);
  picoruby_init_require();
  if( mrbc_create_task(ruby, 0) != NULL ){
    mrbc_run();
    return 0;
  } else {
    return 1;
  }

}
