#include <mruby.h>
#include <mruby/presym.h>
#include <mruby/class.h>
#include <mruby/data.h>
#include <mruby/string.h>
#include <mruby/numeric.h>

#include <string.h>

typedef struct
{
  struct tm   tm;
  mrb_int     unixtime_us;
  long int    timezone;
} PICORUBY_TIME;

typedef struct
{
  mrb_value (*time_now)(mrb_state *mrb, mrb_value self);
} prb_time_methods;

static void
mrb_time_free(mrb_state *mrb, void *ptr)
{
  mrb_free(mrb, ptr);
}

static const struct mrb_data_type mrb_time_type = {
  "Time", mrb_time_free
};

static const struct mrb_data_type mrb_time_methods_type = {
  "TimeMethods", mrb_time_free
};

static mrb_int
mrb_Integer(mrb_state *mrb, mrb_value value)
{
  if (mrb_integer_p(value)) return mrb_integer(value);
  if (mrb_string_p(value)) {
    int sign = 0;
    mrb_int len = RSTRING_LEN(value);
    const char *data = RSTRING_PTR(value);
    if (data[0] == '-') sign = -1;
    if (data[0] == '+') sign = 1;
    if (data[0] == '_' || data[len - 1] == '_') goto error;
    char str[len + 1];
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
    if (j == 0) goto error;
    str[j] = '\0';
    return (sign < 0 ? atoi(str) * -1 : atoi(str));
  }
error:
  mrb_raise(mrb, E_ARGUMENT_ERROR, "invalid value for Integer()");
  return 0;
}

#define Integer(n) mrb_Integer(mrb, n)

static time_t unixtime_offset = 0;

/*
 * Singleton methods
 */

static mrb_value
mrb_s_unixtime_offset(mrb_state *mrb, mrb_value klass)
{
  return mrb_fixnum_value((mrb_int)unixtime_offset);
}

static mrb_value
new_from_unixtime_us(mrb_state *mrb, mrb_value klass, mrb_int unixtime_us)
{
  struct RClass *cls;
  if (mrb_class_p(klass)) {
    cls = mrb_class_ptr(klass);
  } else {
    cls = mrb_obj_class(mrb, klass);
  }

  PICORUBY_TIME *data = (PICORUBY_TIME *)mrb_malloc(mrb, sizeof(PICORUBY_TIME));
  mrb_value self = mrb_obj_value(Data_Wrap_Struct(mrb, cls, &mrb_time_type, data));

  data->unixtime_us = unixtime_us + unixtime_offset * USEC;
  time_t unixtime = data->unixtime_us / USEC;
  localtime_r(&unixtime, &data->tm);
#if defined(PICORB_PLATFORM_POSIX)
  data->timezone = timezone;
#else
  data->timezone = calculate_timezone_offset(unixtime);
#endif
  return self;
}

static mrb_value
new_from_tm(mrb_state *mrb, mrb_value klass, struct tm *tm)
{
  struct RClass *cls = mrb_class_ptr(klass);

  PICORUBY_TIME *data = (PICORUBY_TIME *)mrb_malloc(mrb, sizeof(PICORUBY_TIME));
  mrb_value self = mrb_obj_value(Data_Wrap_Struct(mrb, cls, &mrb_time_type, data));

  time_t unixtime = mktime(tm);
  data->unixtime_us = unixtime * USEC;
  memcpy(&data->tm, tm, sizeof(struct tm));
#if defined(PICORB_PLATFORM_POSIX)
  data->timezone = timezone;
#else
  data->timezone = calculate_timezone_offset(unixtime);
#endif
  return self;
}

static mrb_value
mrb_s_local(mrb_state *mrb, mrb_value klass)
{
  mrb_value *argv;
  mrb_int argc;
  mrb_get_args(mrb, "*", &argv, &argc);

  if (argc < 1 || 6 < argc) {
    mrb_raisef(mrb, E_ARGUMENT_ERROR, "wrong number of arguments (given %d, expected 1..6)", argc);
    return mrb_nil_value();
  }

  struct tm tm = {0};
  tm.tm_year = Integer(argv[0]) - 1900;

  if (1 < argc) {
    int mon = Integer(argv[1]);
    if (mon < 1 || 12 < mon) mrb_raise(mrb, E_ARGUMENT_ERROR, "mon out of range");
    tm.tm_mon = mon - 1;
  } else {
    tm.tm_mon = 0;
  }

  if (2 < argc) {
    int mday = Integer(argv[2]);
    if (mday < 1 || 31 < mday) mrb_raise(mrb, E_ARGUMENT_ERROR, "mday out of range");
    tm.tm_mday = mday;
  } else {
    tm.tm_mday = 1;
  }

  if (3 < argc) {
    int hour = Integer(argv[3]);
    if (hour < 0 || 23 < hour) mrb_raise(mrb, E_ARGUMENT_ERROR, "hour out of range");
    tm.tm_hour = hour;
  } else {
    tm.tm_hour = 0;
  }

  if (4 < argc) {
    int min = Integer(argv[4]);
    if (min < 0 || 59 < min) mrb_raise(mrb, E_ARGUMENT_ERROR, "min out of range");
    tm.tm_min = min;
  } else {
    tm.tm_min = 0;
  }

  if (5 < argc) {
    int sec = Integer(argv[5]);
    if (sec < 0 || 60 < sec) mrb_raise(mrb, E_ARGUMENT_ERROR, "sec out of range");
    tm.tm_sec = sec;
  } else {
    tm.tm_sec = 0;
  }

  return new_from_tm(mrb, klass, &tm);
}

static mrb_value
mrb_s_at(mrb_state *mrb, mrb_value klass)
{
  mrb_int unixtime;
  mrb_get_args(mrb, "i", &unixtime);
  return new_from_unixtime_us(mrb, klass, (mrb_int)((time_t)unixtime - unixtime_offset) * USEC);
}

static mrb_value
mrb_s_now(mrb_state *mrb, mrb_value klass)
{
#if defined(NO_CLOCK_GETTIME)
  struct timeval tv;
  gettimeofday(&tv, 0);
  return new_from_unixtime_us(mrb, klass, (mrb_int)(tv.tv_sec * USEC + tv.tv_usec));
#else
  struct timespec ts;
  clock_gettime(CLOCK_REALTIME, &ts);
  return new_from_unixtime_us(mrb, klass, (mrb_int)(ts.tv_sec * USEC + ts.tv_nsec / 1000));
#endif
}

static mrb_value
mrb_s_new(mrb_state *mrb, mrb_value klass)
{
  mrb_value *argv;
  mrb_int argc;
  mrb_get_args(mrb, "*", &argv, &argc);

  if (argc == 0) {
    return mrb_s_now(mrb, klass);
  } else if (6 < argc) {
    mrb_raisef(mrb, E_ARGUMENT_ERROR, "wrong number of arguments (given %d, expected 0..6)", argc);
    return mrb_nil_value();
  } else {
    return mrb_s_local(mrb, klass);
  }
}

/*
 * Instance methods
 */

static mrb_value
mrb_to_i(mrb_state *mrb, mrb_value self)
{
  PICORUBY_TIME *data = (PICORUBY_TIME *)DATA_PTR(self);
  return mrb_fixnum_value((mrb_int)data->unixtime_us / USEC);
}

static mrb_value
mrb_to_f(mrb_state *mrb, mrb_value self)
{
  PICORUBY_TIME *data = (PICORUBY_TIME *)DATA_PTR(self);
  return mrb_float_value(mrb, (mrb_float)data->unixtime_us / USEC);
}

#define INSPECT_LENGTH 80

static mrb_value
mrb_to_s(mrb_state *mrb, mrb_value self)
{
  PICORUBY_TIME *data = (PICORUBY_TIME *)DATA_PTR(self);
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

  return mrb_str_new_cstr(mrb, str);
}

static mrb_value
mrb_time_inspect(mrb_state *mrb, mrb_value self)
{
  if (mrb_class_p(self)) {
    return mrb_str_new_cstr(mrb, "Time");
  }

  PICORUBY_TIME *data = (PICORUBY_TIME *)DATA_PTR(self);
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

  return mrb_str_new_cstr(mrb, str);
}

static mrb_value
mrb_year(mrb_state *mrb, mrb_value self)
{
  PICORUBY_TIME *data = (PICORUBY_TIME *)DATA_PTR(self);
  return mrb_fixnum_value(data->tm.tm_year + 1900);
}

static mrb_value
mrb_mon(mrb_state *mrb, mrb_value self)
{
  PICORUBY_TIME *data = (PICORUBY_TIME *)DATA_PTR(self);
  return mrb_fixnum_value(data->tm.tm_mon + 1);
}

static mrb_value
mrb_mday(mrb_state *mrb, mrb_value self)
{
  PICORUBY_TIME *data = (PICORUBY_TIME *)DATA_PTR(self);
  return mrb_fixnum_value(data->tm.tm_mday);
}

static mrb_value
mrb_hour(mrb_state *mrb, mrb_value self)
{
  PICORUBY_TIME *data = (PICORUBY_TIME *)DATA_PTR(self);
  return mrb_fixnum_value(data->tm.tm_hour);
}

static mrb_value
mrb_min(mrb_state *mrb, mrb_value self)
{
  PICORUBY_TIME *data = (PICORUBY_TIME *)DATA_PTR(self);
  return mrb_fixnum_value(data->tm.tm_min);
}

static mrb_value
mrb_sec(mrb_state *mrb, mrb_value self)
{
  PICORUBY_TIME *data = (PICORUBY_TIME *)DATA_PTR(self);
  return mrb_fixnum_value(data->tm.tm_sec);
}

static mrb_value
mrb_usec(mrb_state *mrb, mrb_value self)
{
  PICORUBY_TIME *data = (PICORUBY_TIME *)DATA_PTR(self);
  return mrb_fixnum_value(data->unixtime_us % USEC);
}

static mrb_value
mrb_wday(mrb_state *mrb, mrb_value self)
{
  PICORUBY_TIME *data = (PICORUBY_TIME *)DATA_PTR(self);
  return mrb_fixnum_value(data->tm.tm_wday);
}

static int
mrb_time_compare(mrb_state *mrb, mrb_value self, mrb_value other)
{
  if (!mrb_obj_is_instance_of(mrb, other, mrb_obj_class(mrb, self))) {
    return -2;
  }
  mrb_int other_unixtime_us = ((PICORUBY_TIME *)DATA_PTR(other))->unixtime_us;
  mrb_int self_unixtime_us = ((PICORUBY_TIME *)DATA_PTR(self))->unixtime_us;
  if (self_unixtime_us < other_unixtime_us) {
    return -1;
  } else if (other_unixtime_us < self_unixtime_us) {
    return 1;
  } else {
    return 0;
  }
}

static void
mrb_time_comparison_failed(mrb_state *mrb)
{
  mrb_raise(mrb, E_ARGUMENT_ERROR, "comparison of Time failed");
}

static mrb_value
mrb_compare(mrb_state *mrb, mrb_value self)
{
  mrb_value other;
  mrb_get_args(mrb, "o", &other);
  return mrb_fixnum_value(mrb_time_compare(mrb, self, other));
}

static mrb_value
mrb_eq(mrb_state *mrb, mrb_value self)
{
  mrb_value other;
  mrb_get_args(mrb, "o", &other);
  switch (mrb_time_compare(mrb, self, other)) {
    case 0:
      return mrb_true_value();
    default:
      return mrb_false_value();
  }
}

static mrb_value
mrb_lt(mrb_state *mrb, mrb_value self)
{
  mrb_value other;
  mrb_get_args(mrb, "o", &other);
  switch (mrb_time_compare(mrb, self, other)) {
    case -1:
      return mrb_true_value();
    case 0:
    case 1:
      return mrb_false_value();
    default:
      mrb_time_comparison_failed(mrb);
      return mrb_nil_value();
  }
}

static mrb_value
mrb_lte(mrb_state *mrb, mrb_value self)
{
  mrb_value other;
  mrb_get_args(mrb, "o", &other);
  switch (mrb_time_compare(mrb, self, other)) {
    case -1:
    case 0:
      return mrb_true_value();
    case 1:
      return mrb_false_value();
    default:
      mrb_time_comparison_failed(mrb);
      return mrb_nil_value();
  }
}

static mrb_value
mrb_gt(mrb_state *mrb, mrb_value self)
{
  mrb_value other;
  mrb_get_args(mrb, "o", &other);
  switch (mrb_time_compare(mrb, self, other)) {
    case -1:
    case 0:
      return mrb_false_value();
    case 1:
      return mrb_true_value();
    default:
      mrb_time_comparison_failed(mrb);
      return mrb_nil_value();
  }
}

static mrb_value
mrb_gte(mrb_state *mrb, mrb_value self)
{
  mrb_value other;
  mrb_get_args(mrb, "o", &other);
  switch (mrb_time_compare(mrb, self, other)) {
    case -1:
      return mrb_false_value();
    case 0:
    case 1:
      return mrb_true_value();
    default:
      mrb_time_comparison_failed(mrb);
      return mrb_nil_value();
  }
}

static mrb_value
mrb_sub(mrb_state *mrb, mrb_value self)
{
  mrb_value other;
  mrb_get_args(mrb, "o", &other);

  mrb_int self_unixtime_us = ((PICORUBY_TIME *)DATA_PTR(self))->unixtime_us;

  if (mrb_integer_p(other)) {
    return new_from_unixtime_us(mrb, self, self_unixtime_us - mrb_integer(other) * USEC);
  } else if (mrb_float_p(other)) {
    return new_from_unixtime_us(mrb, self, self_unixtime_us - (mrb_int)(mrb_float(other) * USEC));
  } else if (mrb_obj_is_instance_of(mrb, other, mrb_obj_class(mrb, self))) {
    mrb_int other_unixtime_us = ((PICORUBY_TIME *)DATA_PTR(other))->unixtime_us;
    mrb_float result = (mrb_float)(self_unixtime_us - other_unixtime_us) / (mrb_float)USEC;
    return mrb_float_value(mrb, result);
  } else {
    mrb_raise(mrb, E_ARGUMENT_ERROR, "wrong argument type");
    return mrb_nil_value();
  }
}

static mrb_value
mrb_add(mrb_state *mrb, mrb_value self)
{
  mrb_value other;
  mrb_get_args(mrb, "o", &other);

  mrb_int self_unixtime_us = ((PICORUBY_TIME *)DATA_PTR(self))->unixtime_us;

  if (mrb_integer_p(other)) {
    return new_from_unixtime_us(mrb, self, self_unixtime_us + mrb_integer(other) * USEC);
  } else if (mrb_float_p(other)) {
    return new_from_unixtime_us(mrb, self, self_unixtime_us + (mrb_int)(mrb_float(other) * USEC));
  } else {
    mrb_raise(mrb, E_ARGUMENT_ERROR, "wrong argument type");
    return mrb_nil_value();
  }
}

static mrb_value
mrb_time_methods(mrb_state *mrb, mrb_value self)
{
  struct RClass *class_Time = mrb_class_get_id(mrb, MRB_SYM(Time));
  struct RClass *class_TimeMethods = mrb_class_get_under_id(mrb, class_Time, MRB_SYM(TimeMethods));

  prb_time_methods *m = (prb_time_methods *)mrb_malloc(mrb, sizeof(prb_time_methods));
  m->time_now = mrb_s_now;

  mrb_value methods = mrb_obj_value(Data_Wrap_Struct(mrb, class_TimeMethods, &mrb_time_methods_type, m));
  return methods;
}

void
mrb_picoruby_time_gem_init(mrb_state *mrb)
{
  struct RClass *class_Time = mrb_define_class_id(mrb, MRB_SYM(Time), mrb->object_class);
  MRB_SET_INSTANCE_TT(class_Time, MRB_TT_CDATA);
  struct RClass *class_TimeMethods = mrb_define_class_under_id(mrb, class_Time, MRB_SYM(TimeMethods), mrb->object_class);
  MRB_SET_INSTANCE_TT(class_TimeMethods, MRB_TT_CDATA);

  mrb_define_class_method_id(mrb, class_Time, MRB_SYM(unixtime_offset), mrb_s_unixtime_offset, MRB_ARGS_NONE());
  mrb_define_class_method_id(mrb, class_Time, MRB_SYM(mktime), mrb_s_local, MRB_ARGS_ARG(1, 5));
  mrb_define_class_method_id(mrb, class_Time, MRB_SYM(local), mrb_s_local, MRB_ARGS_ARG(1, 5));
  mrb_define_class_method_id(mrb, class_Time, MRB_SYM(at), mrb_s_at, MRB_ARGS_REQ(1));
  mrb_define_class_method_id(mrb, class_Time, MRB_SYM(now), mrb_s_now, MRB_ARGS_NONE());
  mrb_define_class_method_id(mrb, class_Time, MRB_SYM(new), mrb_s_new, MRB_ARGS_ANY());
  mrb_define_method_id(mrb, class_Time, MRB_SYM(to_i), mrb_to_i, MRB_ARGS_NONE());
  mrb_define_method_id(mrb, class_Time, MRB_SYM(to_f), mrb_to_f, MRB_ARGS_NONE());
  mrb_define_method_id(mrb, class_Time, MRB_SYM(to_s), mrb_to_s, MRB_ARGS_NONE());
  mrb_define_method_id(mrb, class_Time, MRB_SYM(inspect), mrb_time_inspect, MRB_ARGS_NONE());
  mrb_define_method_id(mrb, class_Time, MRB_SYM(year), mrb_year, MRB_ARGS_NONE());
  mrb_define_method_id(mrb, class_Time, MRB_SYM(mon), mrb_mon, MRB_ARGS_NONE());
  mrb_define_method_id(mrb, class_Time, MRB_SYM(mday), mrb_mday, MRB_ARGS_NONE());
  mrb_define_method_id(mrb, class_Time, MRB_SYM(hour), mrb_hour, MRB_ARGS_NONE());
  mrb_define_method_id(mrb, class_Time, MRB_SYM(min), mrb_min, MRB_ARGS_NONE());
  mrb_define_method_id(mrb, class_Time, MRB_SYM(sec), mrb_sec, MRB_ARGS_NONE());
  mrb_define_method_id(mrb, class_Time, MRB_SYM(usec), mrb_usec, MRB_ARGS_NONE());
  mrb_define_method_id(mrb, class_Time, MRB_SYM(wday), mrb_wday, MRB_ARGS_NONE());
  mrb_define_method_id(mrb, class_Time, MRB_OPSYM(cmp), mrb_compare, MRB_ARGS_REQ(1));
  mrb_define_method_id(mrb, class_Time, MRB_OPSYM(eq), mrb_eq, MRB_ARGS_REQ(1));
  mrb_define_method_id(mrb, class_Time, MRB_OPSYM(lt), mrb_lt, MRB_ARGS_REQ(1));
  mrb_define_method_id(mrb, class_Time, MRB_OPSYM(le), mrb_lte, MRB_ARGS_REQ(1));
  mrb_define_method_id(mrb, class_Time, MRB_OPSYM(gt), mrb_gt, MRB_ARGS_REQ(1));
  mrb_define_method_id(mrb, class_Time, MRB_OPSYM(ge), mrb_gte, MRB_ARGS_REQ(1));
  mrb_define_method_id(mrb, class_Time, MRB_OPSYM(sub), mrb_sub, MRB_ARGS_REQ(1));
  mrb_define_method_id(mrb, class_Time, MRB_OPSYM(add), mrb_add, MRB_ARGS_REQ(1));

  mrb_define_method_id(mrb, class_Time, MRB_SYM(time_methods), mrb_time_methods, MRB_ARGS_NONE());
}

void
mrb_picoruby_time_gem_final(mrb_state *mrb)
{
}
