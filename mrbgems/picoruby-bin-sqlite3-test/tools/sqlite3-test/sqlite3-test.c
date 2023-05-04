#include <picorbc.h>
#include <picogem_init.c>

#define MRBC_MEMORY_SIZE (1024*1024)
static uint8_t heap[MRBC_MEMORY_SIZE];

int loglevel = LOGLEVEL_FATAL;

static void
c_print_alloc_stats(mrbc_vm *vm, mrbc_value *v, int argc)
{
  struct MRBC_ALLOC_STATISTICS mem;
  mrbc_alloc_statistics(&mem);
  console_printf("\nSTATS %d/%d\n", mem.used, mem.total);
  SET_NIL_RETURN();
}

int
main(void)
{
  mrbc_init(heap, MRBC_MEMORY_SIZE);
  mrbc_class *mrbc_class_PicoRubyVM = mrbc_define_class(0, "PicoRubyVM", mrbc_class_object);
  mrbc_define_method(0, mrbc_class_PicoRubyVM, "print_alloc_stats", c_print_alloc_stats);
  picoruby_init_require();
  if( mrbc_create_task(sqlite3_test, 0) != NULL ){
    mrbc_run();
    return 0;
  } else {
    return 1;
  }
}

