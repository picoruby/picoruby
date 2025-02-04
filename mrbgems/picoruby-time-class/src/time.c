#include <mrubyc.h>

#include <stdlib.h>
#include <stdio.h>
#include "../include/time-class.h"

#define USEC 1000000

static mrbc_class *class_TimeMethods;

static mrbc_int_t
mrbc_Integer(struct VM *vm, mrbc_value value)
{
  if (value.tt == MRBC_TT_INTEGER) return value.i;
  if (value.tt == MRBC_TT_STRING) {
    int sign = 0;
    size_t len = value.string->size;
    uint8_t *data = value.string->data;
    if (data[0] == '-') sign = -1;
    if (data[0] == '+') sign = 1;
    if (data[0] == '_' || data[len - 1] == '_') goto error;
    char str[len];
    int c;
    int i = (sign == 0 ? 0 : 1);
    int j = 0;
    for (; i < len; i++) {
      c = data[i];
      if (c == '_') continue;
      if (47 < c && c < 58) {
        str[j++] = c;
      } else {
        goto error;
      }
    }
    if (str[0] == '\0') goto error;
    str[j] = '\0';
    return (sign < 0 ? mrbc_atoi(str, 10) * -1 : mrbc_atoi(str, 10));
  }
error:
  mrbc_raise(vm, MRBC_CLASS(ArgumentError), "invalid value for Integer()");
  return 0;
}
#define Integer(n) mrbc_Integer(vm, n)


static time_t unixtime_offset = 0;

static void
tz_env_set(struct VM *vm)
{
  mrbc_value *env_instance = mrbc_get_const(mrbc_search_symid("ENV"));
  mrbc_value env = mrbc_instance_getiv(env_instance, mrbc_str_to_symid("env"));
  if (env.tt != MRBC_TT_HASH) return;
  mrbc_value key = mrbc_string_new_cstr(vm, "TZ");
  mrbc_value tz = mrbc_hash_get(&env, &key);
  mrbc_decref(&key);
  if (tz.tt != MRBC_TT_STRING) return;
#if defined(_BSD_SOURCE) || \
    (defined(_POSIX_C_SOURCE) && _POSIX_C_SOURCE >= 200112L) || \
    (defined(_XOPEN_SOURCE) && _XOPEN_SOURCE >= 600)
  setenv("TZ", (const char *)tz.string->data, 1);
#elif defined(_SVID_SOURCE) || defined(_XOPEN_SOURCE)
  char *tzstr[4 + tz.string->size] = {0};
  tzstr[0] = 'T';
  tzstr[1] = 'Z';
  tzstr[2] = '=';
  memcpy(&tzstr[3], tz.string->data, tz.string->size);
  putenv(tzstr);
#endif
#if defined(_SVID_SOURCE) || defined(_XOPEN_SOURCE)
  tzset();
#endif
}

/*
 * Singleton methods
 */

static void
c_unixtime_offset(struct VM *vm, mrbc_value v[], int argc)
{
  SET_INT_RETURN((mrbc_int_t)unixtime_offset);
}

static void
c_set_hwclock(struct VM *vm, mrbc_value v[], int argc)
{
  /*
   * Usage:
   *   time = Time.local(2023,1,1,0,0,0)
   *   # or time = Net::NTP.get.time
   *   Time.set_hwclock(time)
   */
  mrbc_value value = GET_ARG(1);
  mrbc_class *class_Time = mrbc_get_class_by_name("Time");
  if (value.tt != MRBC_TT_OBJECT || value.instance->cls != class_Time) {
    mrbc_raise(vm, MRBC_CLASS(ArgumentError), "value is not a Time");
    return;
  }
  time_t unixtime = ((PICORUBY_TIME *)value.instance->data)->unixtime_us / USEC;
  unixtime_offset = unixtime - time(NULL);
  mrbc_incref(&value);
  SET_RETURN(value);
}

static mrbc_value
new_from_unixtime_us(struct VM *vm, mrbc_value v[], mrbc_int_t unixtime_us)
{
  tz_env_set(vm);
  mrbc_class *cls;
  if (v->tt == MRBC_TT_CLASS) {
    cls = v->cls;
  } else {
    assert(v->tt == MRBC_TT_OBJECT);
    cls = v->instance->cls;
  }
  mrbc_value value = mrbc_instance_new(vm, cls, sizeof(PICORUBY_TIME));
  PICORUBY_TIME *data = (PICORUBY_TIME *)value.instance->data;
  data->unixtime_us = unixtime_us + unixtime_offset * USEC;
  time_t unixtime = data->unixtime_us / USEC;
  localtime_r(&unixtime, &data->tm);
#if defined(MRBC_USE_HAL_POSIX)
  data->timezone = timezone;  /* global variable from time.h of glibc */
#else
  data->timezone = _timezone; /* newlib? */
#endif
  return value;
}

inline static mrbc_value
new_from_tm(struct VM *vm, mrbc_value v[], struct tm *tm)
{
  tz_env_set(vm);
  mrbc_value value = mrbc_instance_new(vm, v->cls, sizeof(PICORUBY_TIME));
  PICORUBY_TIME *data = (PICORUBY_TIME *)value.instance->data;
  data->unixtime_us = mktime(tm) * USEC;
  memcpy(&data->tm, tm, sizeof(struct tm));
#if defined(MRBC_USE_HAL_POSIX)
  data->timezone = timezone;
#else
  data->timezone = _timezone;
#endif
  return value;
}

static void
c_local(struct VM *vm, mrbc_value v[], int argc)
{
  if (argc < 1 || 6 < argc) {
    mrbc_raise(vm, MRBC_CLASS(ArgumentError), "wrong number of arguments (expected 1..6)");
    return;
  }
  struct tm tm;
  tm = (struct tm){0};
  tm.tm_year = Integer(GET_ARG(1)) - 1900;
  if (1 < argc) {
    int mon = Integer(GET_ARG(2));
    if (mon < 1 || 12 < mon) mrbc_raise(vm, MRBC_CLASS(ArgumentError), "mon out of range");
    tm.tm_mon = mon - 1;
  } else tm.tm_mon = 0;
  if (2 < argc) {
    int mday = Integer(GET_ARG(3));
    if (mday < 1 || 31 < mday) mrbc_raise(vm, MRBC_CLASS(ArgumentError), "mday out of range");
    tm.tm_mday = mday;
  } else tm.tm_mday = 1;
  if (3 < argc) {
    int hour = Integer(GET_ARG(4));
    if (hour < 0 || 23 < hour) mrbc_raise(vm, MRBC_CLASS(ArgumentError), "hour out of range");
    tm.tm_hour = hour;
  } else tm.tm_hour = 0;
  if (4 < argc) {
    int min = Integer(GET_ARG(5));
    if (min < 0 || 59 < min) mrbc_raise(vm, MRBC_CLASS(ArgumentError), "min out of range");
    tm.tm_min = min;
  } else tm.tm_min = 0;
  if (5 < argc) {
    int sec = Integer(GET_ARG(6));
    if (sec < 0 || 60 < sec) mrbc_raise(vm, MRBC_CLASS(ArgumentError), "sec out of range");
    tm.tm_sec = sec;
  } else tm.tm_sec = 0;
  SET_RETURN(new_from_tm(vm, v, &tm));
}

static void
c_at(struct VM *vm, mrbc_value v[], int argc)
{
  if (argc != 1) {
    mrbc_raise(vm, MRBC_CLASS(ArgumentError), "wrong number of arguments (expected 1)");
    return;
  }
  SET_RETURN(new_from_unixtime_us(vm, v, (mrbc_int_t)((time_t)GET_INT_ARG(1) - unixtime_offset) * USEC));
}

static void
c_now(struct VM *vm, mrbc_value v[], int argc)
{
  if (0 < argc) {
    mrbc_raise(vm, MRBC_CLASS(ArgumentError), "wrong number of arguments");
    return;
  }
#if defined(NO_CLOCK_GETTIME)
  struct timeval tv;
  gettimeofday(&tv, 0);
  SET_RETURN(new_from_unixtime_us(vm, v, (mrbc_int_t)(tv.tv_sec * USEC + tv.tv_usec)));
#else
  struct timespec ts;
  clock_gettime(CLOCK_REALTIME, &ts);
  SET_RETURN(new_from_unixtime_us(vm, v, (mrbc_int_t)(ts.tv_sec * USEC + ts.tv_nsec / 1000)));
#endif
}


static void
c_new(struct VM *vm, mrbc_value v[], int argc)
{
  if (argc == 0) {
    c_now(vm, v, 0);
  } else if (6 < argc) {
    mrbc_raise(vm, MRBC_CLASS(ArgumentError), "wrong number of arguments (expected 0..6)");
  } else {
    c_local(vm, v, argc);
  }
}


/*
 * Instance methods
 */

static void
c_to_i(struct VM *vm, mrbc_value v[], int argc)
{
  PICORUBY_TIME *data = (PICORUBY_TIME *)v->instance->data;
  SET_INT_RETURN((mrbc_int_t)data->unixtime_us / USEC);
}

static void
c_to_f(struct VM *vm, mrbc_value v[], int argc)
{
  PICORUBY_TIME *data = (PICORUBY_TIME *)v->instance->data;
  SET_FLOAT_RETURN((mrbc_float_t)data->unixtime_us / USEC);
}

#define INSPECT_LENGTH 80

static void
c_to_s(struct VM *vm, mrbc_value v[], int argc)
{
  PICORUBY_TIME *data = (PICORUBY_TIME *)v->instance->data;
  struct tm *tm = &data->tm;
  char str[INSPECT_LENGTH];
  long int a = labs(data->timezone) / 60;
  int year = tm->tm_year + 1900;
  if (year < 0) {
    sprintf(str, "%05d", year);
  } else if (year < 10000) {
    sprintf(str, "%04d", year);
  } else {
    sprintf(str, "%d", year);
  }
  sprintf(str + strlen(str), "-%02d-%02d %02d:%02d:%02d %c%02d%02d",
    tm->tm_mon + 1,
    tm->tm_mday,
    tm->tm_hour,
    tm->tm_min,
    tm->tm_sec,
    (0 < data->timezone ? '-' : (data->timezone == 0 ? ' ' : '+')),
    (int)(a / 60),
    (int)(a % 60)
  );
  SET_RETURN(mrbc_string_new_cstr(vm, str));
}

static void
c_inspect(struct VM *vm, mrbc_value v[], int argc)
{
  if (v[0].tt == MRBC_TT_CLASS) {
    SET_RETURN(mrbc_string_new_cstr(vm, "Time"));
    return;
  }
  PICORUBY_TIME *data = (PICORUBY_TIME *)v->instance->data;
  struct tm *tm = &data->tm;
  char str[INSPECT_LENGTH];
  long int a = labs(data->timezone) / 60;
  int year = tm->tm_year + 1900;
  if (year < 0) {
    sprintf(str, "%05d", year);
  } else if (year < 10000) {
    sprintf(str, "%04d", year);
  } else {
    sprintf(str, "%d", year);
  }
  sprintf(str + strlen(str), "-%02d-%02d %02d:%02d:%02d.%06ld %c%02d%02d",
    tm->tm_mon + 1,
    tm->tm_mday,
    tm->tm_hour,
    tm->tm_min,
    tm->tm_sec,
    (long int)(data->unixtime_us % USEC),
    (0 < data->timezone ? '-' : (data->timezone == 0 ? ' ' : '+')),
    (int)(a / 60),
    (int)(a % 60)
  );
  SET_RETURN(mrbc_string_new_cstr(vm, str));
}

static void
c_year(struct VM *vm, mrbc_value v[], int argc)
{
  SET_INT_RETURN(((PICORUBY_TIME *)v->instance->data)->tm.tm_year + 1900);
}
static void
c_mon(struct VM *vm, mrbc_value v[], int argc)
{
  SET_INT_RETURN(((PICORUBY_TIME *)v->instance->data)->tm.tm_mon + 1);
}
static void
c_mday(struct VM *vm, mrbc_value v[], int argc)
{
  SET_INT_RETURN(((PICORUBY_TIME *)v->instance->data)->tm.tm_mday);
}
static void
c_hour(struct VM *vm, mrbc_value v[], int argc)
{
  SET_INT_RETURN(((PICORUBY_TIME *)v->instance->data)->tm.tm_hour);
}
static void
c_min(struct VM *vm, mrbc_value v[], int argc)
{
  SET_INT_RETURN(((PICORUBY_TIME *)v->instance->data)->tm.tm_min);
}
static void
c_sec(struct VM *vm, mrbc_value v[], int argc)
{
  SET_INT_RETURN(((PICORUBY_TIME *)v->instance->data)->tm.tm_sec);
}
static void
c_usec(struct VM *vm, mrbc_value v[], int argc)
{
  SET_INT_RETURN(((PICORUBY_TIME *)v->instance->data)->unixtime_us % USEC);
}
static void
c_wday(struct VM *vm, mrbc_value v[], int argc)
{
  SET_INT_RETURN(((PICORUBY_TIME *)v->instance->data)->tm.tm_wday);
}

static int
mrbc_time_compare(mrbc_value *self, mrbc_value *other)
{
  if (other->tt != MRBC_TT_OBJECT || self->instance->cls != other->instance->cls) {
    return -2;
  }
  mrbc_int_t other_unixtime_us = ((PICORUBY_TIME *)other->instance->data)->unixtime_us;
  mrbc_int_t self_unixtime_us = ((PICORUBY_TIME *)self->instance->data)->unixtime_us;
  if (self_unixtime_us < other_unixtime_us) {
    return -1;
  } else if (other_unixtime_us < self_unixtime_us) {
    return 1;
  } else {
    return 0;
  }
}

static void
mrbc_time_comparison_failed(struct VM *vm)
{
  mrbc_raise(vm, MRBC_CLASS(ArgumentError), "comparison of Time failed");
}

static void
c_compare(struct VM *vm, mrbc_value v[], int argc)
{
  SET_INT_RETURN(mrbc_time_compare(v, &v[1]));
}

static void
c_eq(struct VM *vm, mrbc_value v[], int argc)
{
  switch (mrbc_time_compare(v, &v[1])) {
    case 0:
      SET_TRUE_RETURN();
      break;
    default:
      SET_FALSE_RETURN();
  }
}

static void
c_lt(struct VM *vm, mrbc_value v[], int argc)
{
  switch (mrbc_time_compare(v, &v[1])) {
    case -1:
      SET_TRUE_RETURN();
      break;
    case 0:
    case 1:
      SET_FALSE_RETURN();
      break;
    default:
      mrbc_time_comparison_failed(vm);
  }
}

static void
c_lte(struct VM *vm, mrbc_value v[], int argc)
{
  switch (mrbc_time_compare(v, &v[1])) {
    case -1:
    case 0:
      SET_TRUE_RETURN();
      break;
    case 1:
      SET_FALSE_RETURN();
      break;
    default:
      mrbc_time_comparison_failed(vm);
  }
}

static void
c_gt(struct VM *vm, mrbc_value v[], int argc)
{
  switch (mrbc_time_compare(v, &v[1])) {
    case -1:
    case 0:
      SET_FALSE_RETURN();
      break;
    case 1:
      SET_TRUE_RETURN();
      break;
    default:
      mrbc_time_comparison_failed(vm);
  }
}

static void
c_gte(struct VM *vm, mrbc_value v[], int argc)
{
  switch (mrbc_time_compare(v, &v[1])) {
    case -1:
      SET_FALSE_RETURN();
      break;
    case 0:
    case 1:
      SET_TRUE_RETURN();
      break;
    default:
      mrbc_time_comparison_failed(vm);
  }
}

static void
c_sub(struct VM *vm, mrbc_value v[], int argc)
{
  mrbc_int_t self_unixtime_us = ((PICORUBY_TIME *)v[0].instance->data)->unixtime_us;
  switch (v[1].tt) {
    case MRBC_TT_INTEGER:
    {
      SET_RETURN(new_from_unixtime_us(vm, v, self_unixtime_us - (mrbc_int_t)v[1].i * USEC));
      break;
    }
    case MRBC_TT_FLOAT:
    {
      SET_RETURN(new_from_unixtime_us(vm, v, self_unixtime_us - (mrbc_int_t)(v[1].d * USEC)));
      break;
    }
    default:
    {
      if (v[0].instance->cls == v[1].instance->cls) {
        mrbc_int_t other_unixtime_us = ((PICORUBY_TIME *)v[1].instance->data)->unixtime_us;
        mrbc_float_t result = (mrbc_float_t)(self_unixtime_us - other_unixtime_us) / (mrbc_float_t)USEC;
        mrbc_value ret = mrbc_float_value(vm, result);
        SET_RETURN(ret);
      } else {
        mrbc_raise(vm, MRBC_CLASS(ArgumentError), "wrong argument type");
      }
    }
  }
}

static void
c_add(struct VM *vm, mrbc_value v[], int argc)
{
  mrbc_int_t self_unixtime_us = ((PICORUBY_TIME *)v[0].instance->data)->unixtime_us;
  switch (v[1].tt) {
    case MRBC_TT_INTEGER:
    {
      SET_RETURN(new_from_unixtime_us(vm, v, self_unixtime_us + (mrbc_int_t)v[1].i * USEC));
      break;
    }
    case MRBC_TT_FLOAT:
    {
      SET_RETURN(new_from_unixtime_us(vm, v, self_unixtime_us + (mrbc_int_t)(v[1].d * USEC)));
      break;
    }
    default:
      mrbc_raise(vm, MRBC_CLASS(ArgumentError), "wrong argument type");
  }
}

static void
c_time_methods(mrbc_vm *vm, mrbc_value v[], int argc)
{
  prb_time_methods m = {
    c_now
  };
  mrbc_value methods = mrbc_instance_new(vm, class_TimeMethods, sizeof(prb_time_methods));
  memcpy(methods.instance->data, &m, sizeof(prb_time_methods));
  SET_RETURN(methods);
}

void
mrbc_time_class_init(mrbc_vm *vm)
{
  mrbc_class *class_Time = mrbc_define_class(vm, "Time", mrbc_class_object);
  class_TimeMethods = mrbc_define_class_under(vm, class_Time, "TimeMethods", mrbc_class_object);

  mrbc_define_method(vm, class_Time, "unixtime_offset", c_unixtime_offset);
  mrbc_define_method(vm, class_Time, "set_hwclock", c_set_hwclock);
  mrbc_define_method(vm, class_Time, "mktime", c_local);
  mrbc_define_method(vm, class_Time, "local", c_local);
  mrbc_define_method(vm, class_Time, "at", c_at);
  mrbc_define_method(vm, class_Time, "now", c_now);
  mrbc_define_method(vm, class_Time, "new", c_new);
  mrbc_define_method(vm, class_Time, "to_i", c_to_i);
  mrbc_define_method(vm, class_Time, "to_f", c_to_f);
  mrbc_define_method(vm, class_Time, "to_s", c_to_s);
  mrbc_define_method(vm, class_Time, "inspect", c_inspect);
  mrbc_define_method(vm, class_Time, "year", c_year);
  mrbc_define_method(vm, class_Time, "mon",  c_mon);
  mrbc_define_method(vm, class_Time, "mday", c_mday);
  mrbc_define_method(vm, class_Time, "hour", c_hour);
  mrbc_define_method(vm, class_Time, "min",  c_min);
  mrbc_define_method(vm, class_Time, "sec",  c_sec);
  mrbc_define_method(vm, class_Time, "usec", c_usec);
  mrbc_define_method(vm, class_Time, "wday", c_wday);
  mrbc_define_method(vm, class_Time, "<=>", c_compare);
  mrbc_define_method(vm, class_Time, "==", c_eq);
  mrbc_define_method(vm, class_Time, "<",  c_lt);
  mrbc_define_method(vm, class_Time, "<=", c_lte);
  mrbc_define_method(vm, class_Time, ">",  c_gt);
  mrbc_define_method(vm, class_Time, ">=", c_gte);
  mrbc_define_method(vm, class_Time, "-", c_sub);
  mrbc_define_method(vm, class_Time, "+", c_add);

  mrbc_define_method(vm, class_Time, "time_methods", c_time_methods);
}
