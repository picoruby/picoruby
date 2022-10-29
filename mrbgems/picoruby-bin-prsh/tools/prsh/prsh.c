#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include <picorbc.h>
#include <picogem_init.c>

#ifndef HEAP_SIZE
#define HEAP_SIZE (1024 * 200 - 1)
#endif

int loglevel = LOGLEVEL_FATAL; /* must be LOGLEVEL_FATAL so that irb works */
static uint8_t heap_pool[HEAP_SIZE];

int
main(void)
{
  mrbc_init(heap_pool, HEAP_SIZE);
  mrbc_require_init();
  mrbc_create_task(prsh, 0);
  mrbc_run();
  return 0;
}

