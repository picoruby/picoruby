#include <mruby.h>
#include <mruby/presym.h>
#include <mruby/class.h>
#include <mruby/data.h>
#include <mruby/string.h>
#include <mruby/numeric.h>

#include <string.h>

#ifdef __EMSCRIPTEN__
#include <emscripten.h>

EM_JS(double, emscripten_date_now, (), {
  return Date.now();
});
#endif

typedef struct
{
  struct tm   tm;
  /* int64_t, not mrb_int: with MRB_INT32 a microsecond count overflows
     mrb_int in about 35 minutes */
  int64_t     unixtime_us;
  long int    timezone;     /* seconds west of UTC, as the POSIX global */
  uint8_t     utc;          /* a UTC time: to_s says UTC, zone is "UTC" */
  uint8_t     fixed;        /* a fixed offset given to Time.new */
} PICORB_TIME;

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

static int mrb_time_compare(mrb_state *mrb, mrb_value self, mrb_value other);

/*
 * Singleton methods
 */

static mrb_value
mrb_s_unixtime_offset(mrb_state *mrb, mrb_value klass)
{
  return mrb_int64_value(mrb, (int64_t)unixtime_offset);
}

/* the fields of data from its unixtime, in UTC, its fixed offset or
 * the local zone */
static void
time_refresh(PICORB_TIME *data)
{
  time_t unixtime = (time_t)(data->unixtime_us / USEC);
  if (data->unixtime_us < 0 && data->unixtime_us % USEC != 0) unixtime--;
  picorb_time_fields(unixtime, data->utc, data->fixed, -data->timezone,
                     &data->tm, &data->timezone);
}

/* a Time like proto (NULL: a local time) at unixtime_us */
static mrb_value
new_from_unixtime_us_like(mrb_state *mrb, mrb_value klass, int64_t unixtime_us, const PICORB_TIME *proto)
{
  struct RClass *cls;
  if (mrb_class_p(klass)) {
    cls = mrb_class_ptr(klass);
  } else {
    cls = mrb_obj_class(mrb, klass);
  }

  PICORB_TIME *data = (PICORB_TIME *)mrb_malloc(mrb, sizeof(PICORB_TIME));
  mrb_value self = mrb_obj_value(Data_Wrap_Struct(mrb, cls, &mrb_time_type, data));

  data->unixtime_us = unixtime_us + (int64_t)unixtime_offset * USEC;
  data->utc = proto ? proto->utc : 0;
  data->fixed = proto ? proto->fixed : 0;
  data->timezone = proto ? proto->timezone : 0;
  time_refresh(data);
  return self;
}

static mrb_value
new_from_unixtime_us(mrb_state *mrb, mrb_value klass, int64_t unixtime_us)
{
  return new_from_unixtime_us_like(mrb, klass, unixtime_us, NULL);
}

/* A Time from fields: local (zone NULL), UTC, or at a fixed offset
 * given as a zone argument. The tm is recomputed from the unixtime so
 * wday and yday are right. */
static mrb_value
new_from_tm(mrb_state *mrb, mrb_value klass, struct tm *tm, bool utc, bool fixed, long offset_east)
{
  struct RClass *cls = mrb_class_ptr(klass);

  PICORB_TIME *data = (PICORB_TIME *)mrb_malloc(mrb, sizeof(PICORB_TIME));
  mrb_value self = mrb_obj_value(Data_Wrap_Struct(mrb, cls, &mrb_time_type, data));

  time_t unixtime;
  if (utc || fixed) {
    unixtime = (time_t)(picorb_timegm(tm) - (utc ? 0 : offset_east));
  } else {
    tm->tm_isdst = -1;
    unixtime = mktime(tm);
#if !defined(PICORB_PLATFORM_POSIX)
    unixtime += ENV_get_timezone_offset();
#endif
  }
  data->unixtime_us = (int64_t)unixtime * USEC;
  data->utc = utc;
  data->fixed = fixed;
  data->timezone = fixed ? -offset_east : 0;
  time_refresh(data);
  return self;
}

/* the zone argument of Time.new: nil, a String or an Integer offset */
static void
zone_arg(mrb_state *mrb, mrb_value zone, bool *utc, bool *fixed, long *offset_east)
{
  *utc = false;
  *fixed = false;
  *offset_east = 0;
  if (mrb_nil_p(zone)) return;
  if (mrb_integer_p(zone)) {
    *fixed = true;
    *offset_east = (long)mrb_integer(zone);
    return;
  }
  if (mrb_symbol_p(zone)) zone = mrb_sym_str(mrb, mrb_symbol(zone));
  if (mrb_string_p(zone)) {
    bool is_utc;
    if (picorb_parse_offset(RSTRING_PTR(zone), (size_t)RSTRING_LEN(zone), offset_east, &is_utc)) {
      if (is_utc) *utc = true; else *fixed = true;
      return;
    }
  }
  mrb_raisef(mrb, E_ARGUMENT_ERROR, "\"+HH:MM\", \"-HH:MM\", \"UTC\" or \"A\"..\"I\",\"K\"..\"Z\" expected for utc_offset: %v", zone);
}

/* the zone of a Time, after the fact: Time.new(in: zone) */
static void
time_set_zone(mrb_state *mrb, mrb_value time, mrb_value zone)
{
  bool utc, fixed;
  long offset_east;
  zone_arg(mrb, zone, &utc, &fixed, &offset_east);
  PICORB_TIME *data = (PICORB_TIME *)DATA_PTR(time);
  data->utc = utc;
  data->fixed = fixed;
  data->timezone = fixed ? -offset_east : 0;
  time_refresh(data);
}

/* the microseconds argument of Time.local and Time.utc: an Integer or
 * a number with a fraction */
static int64_t
usec_arg(mrb_state *mrb, mrb_value v)
{
  double d;
  if (mrb_integer_p(v)) {
    d = (double)mrb_integer(v);
  } else if (mrb_float_p(v)) {
    d = mrb_float(v);
  } else if (!mrb_string_p(v) && mrb_respond_to(mrb, v, MRB_SYM(to_f))) {
    d = mrb_as_float(mrb, mrb_funcall_id(mrb, v, MRB_SYM(to_f), 0));
  } else {
    d = (double)Integer(v);
  }
  if (!(0 <= d && d < USEC)) mrb_raise(mrb, E_ARGUMENT_ERROR, "subsecx out of range");
  return (int64_t)d; /* truncated, as CRuby does */
}

/* Time.local, Time.utc and Time.new share the fields; Time.new takes
 * a 7th zone argument, Time.local and Time.utc a 7th microseconds one */
static mrb_value
time_from_fields(mrb_state *mrb, mrb_value klass, mrb_value *argv, mrb_int argc, bool utc, bool with_zone)
{
  /* the 10-argument form of Time.local and Time.utc is the order of
   * Time#to_a: sec, min, hour, mday, mon, year, wday, yday, isdst, zone */
  mrb_value reordered[6];
  if (!with_zone && argc == 10) {
    for (int i = 0; i < 6; i++) reordered[i] = argv[5 - i];
    argv = reordered;
    argc = 6;
  }
  mrb_int nfields = argc == 7 ? 6 : argc;
  if (argc < 1 || 7 < argc) {
    mrb_raisef(mrb, E_ARGUMENT_ERROR, "wrong number of arguments (given %d, expected 1..7)", argc);
    return mrb_nil_value();
  }
  bool fixed = false;
  long offset_east = 0;
  int64_t usec_extra = 0;
  if (argc == 7) {
    if (with_zone) zone_arg(mrb, argv[6], &utc, &fixed, &offset_east);
    else usec_extra = usec_arg(mrb, argv[6]);
  }
  argc = nfields;

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

  int usec = 0;
  if (5 < argc) {
    double sec;
    if (mrb_float_p(argv[5])) {
      sec = mrb_float(argv[5]);
      if (sec != sec) mrb_raise(mrb, E_FLOATDOMAIN_ERROR, "NaN");
    } else if (!mrb_integer_p(argv[5]) && !mrb_string_p(argv[5]) &&
               mrb_respond_to(mrb, argv[5], MRB_SYM(to_f))) {
      /* a Rational or another numeric with a fraction */
      sec = mrb_as_float(mrb, mrb_funcall_id(mrb, argv[5], MRB_SYM(to_f), 0));
    } else {
      sec = (double)Integer(argv[5]);
    }
    if (sec < 0 || 60 < sec) mrb_raise(mrb, E_ARGUMENT_ERROR, "sec out of range");
    /* truncated, as CRuby does: rounding could reach a whole second */
    usec = (sec - (int)sec) * USEC;
    tm.tm_sec = (int)sec;
  } else {
    tm.tm_sec = 0;
  }
  mrb_value time = new_from_tm(mrb, klass, &tm, utc, fixed, offset_east);
  PICORB_TIME *data = (PICORB_TIME *)DATA_PTR(time);
  data->unixtime_us += usec + usec_extra;

  return time;
}

/* Time.new("2026-11-05 12:00:00[.frac][ zone]"), Time.new("2026"): the
 * string's own zone wins over in: */
static mrb_value
time_from_string(mrb_state *mrb, mrb_value klass, mrb_value str, mrb_value in_zone)
{
  struct tm tm;
  int usec;
  bool has_zone, utc;
  long offset_east;
  const char *err = picorb_parse_time_string(RSTRING_PTR(str), (size_t)RSTRING_LEN(str),
                                             &tm, &usec, &has_zone, &utc, &offset_east);
  if (err) {
    if (strcmp(err, "can't parse") == 0) mrb_raisef(mrb, E_ARGUMENT_ERROR, "can't parse: %v", str);
    mrb_raise(mrb, E_ARGUMENT_ERROR, err);
  }
  bool fixed = has_zone && !utc;
  if (!has_zone && !mrb_nil_p(in_zone)) zone_arg(mrb, in_zone, &utc, &fixed, &offset_east);
  mrb_value time = new_from_tm(mrb, klass, &tm, utc, fixed, offset_east);
  PICORB_TIME *data = (PICORB_TIME *)DATA_PTR(time);
  data->unixtime_us += usec;
  return time;
}

static mrb_value
mrb_s_local(mrb_state *mrb, mrb_value klass)
{
  mrb_value *argv;
  mrb_int argc;
  mrb_get_args(mrb, "*", &argv, &argc);
  return time_from_fields(mrb, klass, argv, argc, false, false);
}

/* Time.utc(year, mon = 1, mday = 1, hour = 0, min = 0, sec = 0), Time.gm */
static mrb_value
mrb_s_utc(mrb_state *mrb, mrb_value klass)
{
  mrb_value *argv;
  mrb_int argc;
  mrb_get_args(mrb, "*", &argv, &argc);
  return time_from_fields(mrb, klass, argv, argc, true, false);
}

/* Time.at(seconds), Time.at(seconds, usec), Time.at(seconds, sub, unit)
 * with unit :millisecond, :usec/:microsecond or :nsec/:nanosecond */
static mrb_value
mrb_s_at(mrb_state *mrb, mrb_value klass)
{
  mrb_value arg, sub = mrb_nil_value(), unit = mrb_nil_value();
  mrb_get_args(mrb, "o|oo", &arg, &sub, &unit);
  int64_t us;
  if (mrb_float_p(arg)) {
    us = (int64_t)(mrb_float(arg) * USEC + (mrb_float(arg) < 0 ? -0.5 : 0.5));
  } else {
    /* mrb_as_int64() accepts a Bignum, which a timestamp past the fixnum
       range becomes under MRB_INT32 */
    us = mrb_as_int64(mrb, arg) * USEC;
  }
  if (!mrb_nil_p(sub)) {
    double s = mrb_float_p(sub) ? mrb_float(sub) : (double)mrb_as_int64(mrb, sub);
    double scale = 1.0; /* microseconds */
    if (!mrb_nil_p(unit)) {
      mrb_sym u = mrb_symbol_p(unit) ? mrb_symbol(unit) : mrb_intern_str(mrb, mrb_ensure_string_type(mrb, unit));
      if (u == MRB_SYM(millisecond)) scale = 1000.0;
      else if (u == MRB_SYM(usec) || u == MRB_SYM(microsecond)) scale = 1.0;
      else if (u == MRB_SYM(nsec) || u == MRB_SYM(nanosecond)) scale = 0.001;
      else mrb_raisef(mrb, E_ARGUMENT_ERROR, "unexpected unit: %v", unit);
    }
    us += (int64_t)(s * scale);
  }
  return new_from_unixtime_us(mrb, klass, us - (int64_t)unixtime_offset * USEC);
}

static mrb_value
mrb_s_now(mrb_state *mrb, mrb_value klass)
{
#ifdef __EMSCRIPTEN__
  double ms = emscripten_date_now();
  int64_t unixtime_us = (int64_t)(ms * 1000.0);
  return new_from_unixtime_us(mrb, klass, unixtime_us);
#elif defined(NO_CLOCK_GETTIME)
  struct timeval tv;
  gettimeofday(&tv, 0);
  return new_from_unixtime_us(mrb, klass, (int64_t)tv.tv_sec * USEC + tv.tv_usec);
#else
  struct timespec ts;
  clock_gettime(CLOCK_REALTIME, &ts);
  return new_from_unixtime_us(mrb, klass, (int64_t)ts.tv_sec * USEC + ts.tv_nsec / 1000);
#endif
}

/* Time.new, Time.new(year, ..., sec, zone = nil, in: nil),
 * Time.new(string, in: nil) */
static mrb_value
mrb_s_new(mrb_state *mrb, mrb_value klass)
{
  mrb_value *argv;
  mrb_int argc;
  const mrb_sym kw_names[1] = { MRB_SYM(in) };
  mrb_value kw_values[1];
  const mrb_kwargs kwargs = { 1, 0, kw_names, kw_values, NULL };
  mrb_get_args(mrb, "*:", &argv, &argc, &kwargs);
  mrb_value in_zone = mrb_undef_p(kw_values[0]) ? mrb_nil_value() : kw_values[0];

  if (argc == 0) {
    mrb_value now = mrb_s_now(mrb, klass);
    if (!mrb_nil_p(in_zone)) time_set_zone(mrb, now, in_zone);
    return now;
  }
  /* a lone String is a time to parse; Strings among other arguments are
     fields, each read with Integer() */
  if (argc == 1 && mrb_string_p(argv[0])) {
    return time_from_string(mrb, klass, argv[0], in_zone);
  }
  if (7 < argc) {
    mrb_raisef(mrb, E_ARGUMENT_ERROR, "wrong number of arguments (given %d, expected 0..7)", argc);
    return mrb_nil_value();
  }
  if (mrb_nil_p(in_zone)) {
    return time_from_fields(mrb, klass, argv, argc, false, true);
  }
  if (argc == 7) {
    mrb_raise(mrb, E_ARGUMENT_ERROR, "timezone argument given as positional and keyword arguments");
  }
  mrb_value with_in[7];
  for (mrb_int i = 0; i < argc; i++) with_in[i] = argv[i];
  for (mrb_int i = argc; i < 6; i++) with_in[i] = i == 1 || i == 2 ? mrb_fixnum_value(1) : mrb_fixnum_value(0);
  with_in[6] = in_zone;
  return time_from_fields(mrb, klass, with_in, 7, false, true);
}

/*
 * Instance methods
 */

static mrb_value
mrb_to_i(mrb_state *mrb, mrb_value self)
{
  PICORB_TIME *data = (PICORB_TIME *)DATA_PTR(self);
  int64_t sec = data->unixtime_us / USEC;
  /* floored: C's / truncates toward zero before 1970 */
  if (data->unixtime_us % USEC < 0) sec--;
  return mrb_int64_value(mrb, sec);
}

static mrb_value
mrb_to_f(mrb_state *mrb, mrb_value self)
{
  PICORB_TIME *data = (PICORB_TIME *)DATA_PTR(self);
  return mrb_float_value(mrb, (mrb_float)data->unixtime_us / USEC);
}

#define INSPECT_LENGTH 80

static mrb_value
mrb_to_s(mrb_state *mrb, mrb_value self)
{
  PICORB_TIME *data = (PICORB_TIME *)DATA_PTR(self);
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

  sprintf(str + strlen(str), "-%02d-%02d %02d:%02d:%02d",
    tm->tm_mon + 1,
    tm->tm_mday,
    tm->tm_hour,
    tm->tm_min,
    tm->tm_sec
  );
  if (data->utc) {
    strcat(str, " UTC");
  } else {
    sprintf(str + strlen(str), " %c%02d%02d",
      (0 < data->timezone ? '-' : '+'),
      (int)(a / 60),
      (int)(a % 60)
    );
  }

  return mrb_str_new_cstr(mrb, str);
}

static mrb_value
mrb_time_inspect(mrb_state *mrb, mrb_value self)
{
  if (mrb_class_p(self)) {
    return mrb_str_new_cstr(mrb, "Time");
  }

  PICORB_TIME *data = (PICORB_TIME *)DATA_PTR(self);
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

  long int frac = (long int)(data->unixtime_us % USEC);
  if (frac < 0) frac += USEC;
  sprintf(str + strlen(str), "-%02d-%02d %02d:%02d:%02d",
    tm->tm_mon + 1,
    tm->tm_mday,
    tm->tm_hour,
    tm->tm_min,
    tm->tm_sec
  );
  if (frac) {
    /* CRuby prints the fraction without trailing zeros */
    char fs[8];
    sprintf(fs, "%06ld", frac);
    int n = 6;
    while (n > 0 && fs[n - 1] == '0') n--;
    fs[n] = '\0';
    sprintf(str + strlen(str), ".%s", fs);
  }
  if (data->utc) {
    strcat(str, " UTC");
  } else {
    sprintf(str + strlen(str), " %c%02d%02d",
      (0 < data->timezone ? '-' : '+'),
      (int)(a / 60),
      (int)(a % 60)
    );
  }

  return mrb_str_new_cstr(mrb, str);
}

static mrb_value
mrb_year(mrb_state *mrb, mrb_value self)
{
  PICORB_TIME *data = (PICORB_TIME *)DATA_PTR(self);
  return mrb_int_value(mrb, data->tm.tm_year + 1900);
}

static mrb_value
mrb_mon(mrb_state *mrb, mrb_value self)
{
  PICORB_TIME *data = (PICORB_TIME *)DATA_PTR(self);
  return mrb_int_value(mrb, data->tm.tm_mon + 1);
}

static mrb_value
mrb_mday(mrb_state *mrb, mrb_value self)
{
  PICORB_TIME *data = (PICORB_TIME *)DATA_PTR(self);
  return mrb_int_value(mrb, data->tm.tm_mday);
}

static mrb_value
mrb_hour(mrb_state *mrb, mrb_value self)
{
  PICORB_TIME *data = (PICORB_TIME *)DATA_PTR(self);
  return mrb_int_value(mrb, data->tm.tm_hour);
}

static mrb_value
mrb_min(mrb_state *mrb, mrb_value self)
{
  PICORB_TIME *data = (PICORB_TIME *)DATA_PTR(self);
  return mrb_int_value(mrb, data->tm.tm_min);
}

static mrb_value
mrb_sec(mrb_state *mrb, mrb_value self)
{
  PICORB_TIME *data = (PICORB_TIME *)DATA_PTR(self);
  return mrb_int_value(mrb, data->tm.tm_sec);
}

static mrb_value
mrb_usec(mrb_state *mrb, mrb_value self)
{
  PICORB_TIME *data = (PICORB_TIME *)DATA_PTR(self);
  int64_t usec = data->unixtime_us % USEC;
  if (usec < 0) usec += USEC; /* floored, as to_i */
  return mrb_int_value(mrb, (mrb_int)usec);
}

static mrb_value
mrb_wday(mrb_state *mrb, mrb_value self)
{
  PICORB_TIME *data = (PICORB_TIME *)DATA_PTR(self);
  return mrb_int_value(mrb, data->tm.tm_wday);
}

static mrb_value
mrb_yday(mrb_state *mrb, mrb_value self)
{
  PICORB_TIME *data = (PICORB_TIME *)DATA_PTR(self);
  return mrb_int_value(mrb, data->tm.tm_yday + 1);
}

static mrb_value
mrb_isdst(mrb_state *mrb, mrb_value self)
{
  PICORB_TIME *data = (PICORB_TIME *)DATA_PTR(self);
  return mrb_bool_value(data->tm.tm_isdst > 0);
}

/* Time#utc, #gmtime: the same instant as a UTC time, in place */
static mrb_value
mrb_time_utc(mrb_state *mrb, mrb_value self)
{
  PICORB_TIME *data = (PICORB_TIME *)DATA_PTR(self);
  data->utc = 1;
  data->fixed = 0;
  data->timezone = 0;
  time_refresh(data);
  return self;
}

/* Time#getutc, #getgm: a UTC copy */
static mrb_value
mrb_time_getutc(mrb_state *mrb, mrb_value self)
{
  PICORB_TIME *data = (PICORB_TIME *)DATA_PTR(self);
  PICORB_TIME proto = *data;
  proto.utc = 1;
  proto.fixed = 0;
  proto.timezone = 0;
  return new_from_unixtime_us_like(mrb, self, data->unixtime_us - (int64_t)unixtime_offset * USEC, &proto);
}

/* Time#localtime(zone = nil): the local zone, or the given offset, in place */
static mrb_value
mrb_time_localtime(mrb_state *mrb, mrb_value self)
{
  mrb_value zone = mrb_nil_value();
  mrb_get_args(mrb, "|o", &zone);
  PICORB_TIME *data = (PICORB_TIME *)DATA_PTR(self);
  bool utc, fixed;
  long offset_east;
  zone_arg(mrb, zone, &utc, &fixed, &offset_east);
  data->utc = utc;
  data->fixed = fixed;
  data->timezone = fixed ? -offset_east : 0;
  time_refresh(data);
  return self;
}

/* Time#getlocal(zone = nil): a copy in the local zone or the given offset */
static mrb_value
mrb_time_getlocal(mrb_state *mrb, mrb_value self)
{
  mrb_value zone = mrb_nil_value();
  mrb_get_args(mrb, "|o", &zone);
  PICORB_TIME *data = (PICORB_TIME *)DATA_PTR(self);
  PICORB_TIME proto = *data;
  bool utc, fixed;
  long offset_east;
  zone_arg(mrb, zone, &utc, &fixed, &offset_east);
  proto.utc = utc;
  proto.fixed = fixed;
  proto.timezone = fixed ? -offset_east : 0;
  return new_from_unixtime_us_like(mrb, self, data->unixtime_us - (int64_t)unixtime_offset * USEC, &proto);
}

static mrb_value
mrb_time_utc_p(mrb_state *mrb, mrb_value self)
{
  PICORB_TIME *data = (PICORB_TIME *)DATA_PTR(self);
  return mrb_bool_value(data->utc);
}

/* Time#utc_offset, #gmt_offset, #gmtoff: seconds east of UTC */
static mrb_value
mrb_time_utc_offset(mrb_state *mrb, mrb_value self)
{
  PICORB_TIME *data = (PICORB_TIME *)DATA_PTR(self);
  return mrb_int_value(mrb, data->utc ? 0 : (mrb_int)-data->timezone);
}

/* Time#zone: "UTC", the local zone's abbreviation, or nil for a fixed
 * offset and on a platform that does not know the abbreviation */
static mrb_value
mrb_time_zone(mrb_state *mrb, mrb_value self)
{
  PICORB_TIME *data = (PICORB_TIME *)DATA_PTR(self);
  if (data->utc) return mrb_str_new_lit(mrb, "UTC");
  if (data->fixed) return mrb_nil_value();
  const char *zone = picorb_local_zone(&data->tm);
  return zone ? mrb_str_new_cstr(mrb, zone) : mrb_nil_value();
}

static mrb_value
mrb_time_eql(mrb_state *mrb, mrb_value self)
{
  mrb_value other;
  mrb_get_args(mrb, "o", &other);
  return mrb_bool_value(mrb_time_compare(mrb, self, other) == 0);
}

static mrb_value
mrb_time_hash(mrb_state *mrb, mrb_value self)
{
  PICORB_TIME *data = (PICORB_TIME *)DATA_PTR(self);
  int64_t us = data->unixtime_us;
  return mrb_int_value(mrb, (mrb_int)(us ^ (us >> 32)));
}

static int
mrb_time_compare(mrb_state *mrb, mrb_value self, mrb_value other)
{
  if (!mrb_obj_is_instance_of(mrb, other, mrb_obj_class(mrb, self))) {
    return -2;
  }
  int64_t other_unixtime_us = ((PICORB_TIME *)DATA_PTR(other))->unixtime_us;
  int64_t self_unixtime_us = ((PICORB_TIME *)DATA_PTR(self))->unixtime_us;
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
  return mrb_int_value(mrb, mrb_time_compare(mrb, self, other));
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

  int64_t self_unixtime_us = ((PICORB_TIME *)DATA_PTR(self))->unixtime_us;

  PICORB_TIME *self_data = (PICORB_TIME *)DATA_PTR(self);
  int64_t base = self_unixtime_us - (int64_t)unixtime_offset * USEC;
  if (mrb_integer_p(other)) {
    return new_from_unixtime_us_like(mrb, self, base - mrb_as_int64(mrb, other) * USEC, self_data);
  } else if (mrb_float_p(other)) {
    return new_from_unixtime_us_like(mrb, self, base - picorb_float_usec(mrb_float(other)), self_data);
  } else if (mrb_obj_is_instance_of(mrb, other, mrb_obj_class(mrb, self))) {
    int64_t other_unixtime_us = ((PICORB_TIME *)DATA_PTR(other))->unixtime_us;
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

  PICORB_TIME *self_data = (PICORB_TIME *)DATA_PTR(self);
  int64_t base = self_data->unixtime_us - (int64_t)unixtime_offset * USEC;

  if (mrb_integer_p(other)) {
    return new_from_unixtime_us_like(mrb, self, base + mrb_as_int64(mrb, other) * USEC, self_data);
  } else if (mrb_float_p(other)) {
    return new_from_unixtime_us_like(mrb, self, base + picorb_float_usec(mrb_float(other)), self_data);
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
  /* 1..6 fields, or the 10-argument form in the order of Time#to_a */
  mrb_define_class_method_id(mrb, class_Time, MRB_SYM(mktime), mrb_s_local, MRB_ARGS_ARG(1, 9));
  mrb_define_class_method_id(mrb, class_Time, MRB_SYM(local), mrb_s_local, MRB_ARGS_ARG(1, 9));
  mrb_define_class_method_id(mrb, class_Time, MRB_SYM(utc), mrb_s_utc, MRB_ARGS_ARG(1, 9));
  mrb_define_class_method_id(mrb, class_Time, MRB_SYM(gm), mrb_s_utc, MRB_ARGS_ARG(1, 9));
  mrb_define_class_method_id(mrb, class_Time, MRB_SYM(at), mrb_s_at, MRB_ARGS_ARG(1, 2));
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
  mrb_define_method_id(mrb, class_Time, MRB_SYM(yday), mrb_yday, MRB_ARGS_NONE());
  mrb_define_method_id(mrb, class_Time, MRB_SYM(isdst), mrb_isdst, MRB_ARGS_NONE());
  mrb_define_method_id(mrb, class_Time, MRB_SYM_Q(dst), mrb_isdst, MRB_ARGS_NONE());
  mrb_define_method_id(mrb, class_Time, MRB_SYM(utc), mrb_time_utc, MRB_ARGS_NONE());
  mrb_define_method_id(mrb, class_Time, MRB_SYM(gmtime), mrb_time_utc, MRB_ARGS_NONE());
  mrb_define_method_id(mrb, class_Time, MRB_SYM(getutc), mrb_time_getutc, MRB_ARGS_NONE());
  mrb_define_method_id(mrb, class_Time, MRB_SYM(getgm), mrb_time_getutc, MRB_ARGS_NONE());
  mrb_define_method_id(mrb, class_Time, MRB_SYM(localtime), mrb_time_localtime, MRB_ARGS_OPT(1));
  mrb_define_method_id(mrb, class_Time, MRB_SYM(getlocal), mrb_time_getlocal, MRB_ARGS_OPT(1));
  mrb_define_method_id(mrb, class_Time, MRB_SYM_Q(utc), mrb_time_utc_p, MRB_ARGS_NONE());
  mrb_define_method_id(mrb, class_Time, MRB_SYM_Q(gmt), mrb_time_utc_p, MRB_ARGS_NONE());
  mrb_define_method_id(mrb, class_Time, MRB_SYM(utc_offset), mrb_time_utc_offset, MRB_ARGS_NONE());
  mrb_define_method_id(mrb, class_Time, MRB_SYM(gmt_offset), mrb_time_utc_offset, MRB_ARGS_NONE());
  mrb_define_method_id(mrb, class_Time, MRB_SYM(gmtoff), mrb_time_utc_offset, MRB_ARGS_NONE());
  mrb_define_method_id(mrb, class_Time, MRB_SYM(zone), mrb_time_zone, MRB_ARGS_NONE());
  mrb_define_method_id(mrb, class_Time, MRB_SYM_Q(eql), mrb_time_eql, MRB_ARGS_REQ(1));
  mrb_define_method_id(mrb, class_Time, MRB_SYM(hash), mrb_time_hash, MRB_ARGS_NONE());
  mrb_define_method_id(mrb, class_Time, MRB_OPSYM(cmp), mrb_compare, MRB_ARGS_REQ(1));
  mrb_define_method_id(mrb, class_Time, MRB_OPSYM(eq), mrb_eq, MRB_ARGS_REQ(1));
  mrb_define_method_id(mrb, class_Time, MRB_OPSYM(lt), mrb_lt, MRB_ARGS_REQ(1));
  mrb_define_method_id(mrb, class_Time, MRB_OPSYM(le), mrb_lte, MRB_ARGS_REQ(1));
  mrb_define_method_id(mrb, class_Time, MRB_OPSYM(gt), mrb_gt, MRB_ARGS_REQ(1));
  mrb_define_method_id(mrb, class_Time, MRB_OPSYM(ge), mrb_gte, MRB_ARGS_REQ(1));
  mrb_define_method_id(mrb, class_Time, MRB_OPSYM(sub), mrb_sub, MRB_ARGS_REQ(1));
  mrb_define_method_id(mrb, class_Time, MRB_OPSYM(add), mrb_add, MRB_ARGS_REQ(1));

  mrb_define_class_method_id(mrb, class_Time, MRB_SYM(time_methods), mrb_time_methods, MRB_ARGS_NONE());
}

void
mrb_picoruby_time_gem_final(mrb_state *mrb)
{
}
