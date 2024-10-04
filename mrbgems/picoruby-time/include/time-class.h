#ifndef TIME_DEFINED_H_
#define TIME_DEFINED_H_

#include <time.h>
#include <sys/time.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct
{
  struct tm   tm;
  mrbc_int_t  unixtime_us;
  long int    timezone;
} PICORUBY_TIME;

typedef struct
{
  void (*time_now)(mrbc_vm *vm, mrbc_value *v, int argc);
} prb_time_methods;

#ifdef __cplusplus
}
#endif

#endif /* TIME_DEFINED_H_ */

