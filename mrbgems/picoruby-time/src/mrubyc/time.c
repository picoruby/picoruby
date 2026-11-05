#include <mrubyc.h>

typedef struct
{
  struct tm   tm;
  mrbc_int_t  unixtime_us;
  long int    timezone;     /* seconds west of UTC, as the POSIX global */
  uint8_t     utc;          /* a UTC time: to_s says UTC, zone is "UTC" */
  uint8_t     fixed;        /* a fixed offset given to Time.new */
} PICORB_TIME;

typedef struct
{
  void (*time_now)(mrbc_vm *vm, mrbc_value *v, int argc);
} prb_time_methods;

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

/*
 * Singleton methods
 */

static void
c_unixtime_offset(struct VM *vm, mrbc_value v[], int argc)
{
  SET_INT_RETURN((mrbc_int_t)unixtime_offset);
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
static mrbc_value
new_from_unixtime_us_like(struct VM *vm, mrbc_value v[], mrbc_int_t unixtime_us, const PICORB_TIME *proto)
{
  mrbc_class *cls;
  if (v->tt == MRBC_TT_CLASS) {
    cls = v->cls;
  } else {
    assert(v->tt == MRBC_TT_OBJECT);
    cls = v->instance->cls;
  }
  mrbc_value value = mrbc_instance_new(vm, cls, sizeof(PICORB_TIME));
  PICORB_TIME *data = (PICORB_TIME *)value.instance->data;
  data->unixtime_us = unixtime_us + unixtime_offset * USEC;
  data->utc = proto ? proto->utc : 0;
  data->fixed = proto ? proto->fixed : 0;
  data->timezone = proto ? proto->timezone : 0;
  time_refresh(data);
  return value;
}

static mrbc_value
new_from_unixtime_us(struct VM *vm, mrbc_value v[], mrbc_int_t unixtime_us)
{
  return new_from_unixtime_us_like(vm, v, unixtime_us, NULL);
}

/* A Time from fields: local, UTC, or at a fixed offset. The tm is
 * recomputed from the unixtime so wday and yday are right. */
inline static mrbc_value
new_from_tm(struct VM *vm, mrbc_value v[], struct tm *tm, bool utc, bool fixed, long offset_east)
{
  mrbc_value value = mrbc_instance_new(vm, v->cls, sizeof(PICORB_TIME));
  PICORB_TIME *data = (PICORB_TIME *)value.instance->data;
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
  data->unixtime_us = (mrbc_int_t)unixtime * USEC;
  data->utc = utc;
  data->fixed = fixed;
  data->timezone = fixed ? -offset_east : 0;
  time_refresh(data);
  return value;
}

/* the zone argument of Time.new: nil, a String or an Integer offset;
 * returns 0 after raising */
static int
zone_arg(struct VM *vm, const mrbc_value *zone, bool *utc, bool *fixed, long *offset_east)
{
  *utc = false;
  *fixed = false;
  *offset_east = 0;
  if (zone == NULL || zone->tt == MRBC_TT_NIL) return 1;
  if (zone->tt == MRBC_TT_INTEGER) {
    *fixed = true;
    *offset_east = (long)zone->i;
    return 1;
  }
  const char *s = NULL;
  size_t len = 0;
  if (zone->tt == MRBC_TT_STRING) {
    s = (const char *)zone->string->data;
    len = zone->string->size;
  } else if (zone->tt == MRBC_TT_SYMBOL) {
    s = mrbc_symbol_cstr(zone);
    len = strlen(s);
  }
  bool is_utc;
  if (s && picorb_parse_offset(s, len, offset_east, &is_utc)) {
    if (is_utc) *utc = true; else *fixed = true;
    return 1;
  }
  mrbc_raise(vm, MRBC_CLASS(ArgumentError), "\"+HH:MM\", \"-HH:MM\", \"UTC\" or \"A\"..\"I\",\"K\"..\"Z\" expected for utc_offset");
  return 0;
}

/* Time.local, Time.utc and Time.new share the fields; Time.new takes
 * a 7th zone argument */
static void
time_from_fields(struct VM *vm, mrbc_value v[], int argc, bool utc, bool with_zone, const mrbc_value *zone)
{
  /* the 10-argument form of Time.local and Time.utc is the order of
   * Time#to_a: sec, min, hour, mday, mon, year, wday, yday, isdst, zone */
  if (!with_zone && argc == 10) {
    /* the six fields go back into v[1..6], reversed; v stays the frame
     * so SET_RETURN still writes the caller's register */
    mrbc_value fields[6];
    for (int i = 0; i < 6; i++) fields[i] = v[6 - i];
    for (int i = 0; i < 6; i++) v[1 + i] = fields[i];
    argc = 6;
  }
  if (argc < 1 || 7 < argc) {
    mrbc_raise(vm, MRBC_CLASS(ArgumentError), "wrong number of arguments (expected 1..7)");
    return;
  }
  bool fixed = false;
  long offset_east = 0;
  mrbc_int_t usec_extra = 0;
  if (argc == 7) {
    /* Time.new's 7th argument is the zone, Time.local's and Time.utc's
     * the microseconds */
    if (with_zone) {
      if (!zone_arg(vm, &v[7], &utc, &fixed, &offset_east)) return;
    } else {
      mrbc_value u = GET_ARG(7);
      if (u.tt == MRBC_TT_FLOAT) usec_extra = (mrbc_int_t)(u.d + 0.5);
      else {
        usec_extra = (mrbc_int_t)Integer(u);
        if (vm->exception.tt == MRBC_TT_EXCEPTION) return;
      }
    }
    argc = 6;
  } else if (zone) {
    /* the in: keyword of Time.new */
    if (!zone_arg(vm, zone, &utc, &fixed, &offset_east)) return;
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
  int usec = 0;
  if (5 < argc) {
    double sec;
    mrbc_value value = GET_ARG(6);
    if (value.tt == MRBC_TT_FLOAT) {
      sec = value.d;
      if (sec != sec) {
        mrbc_raise(vm, MRBC_CLASS(RangeError), "NaN");
        return;
      }
    } else {
      sec = (mrbc_float_t)Integer(value);
      if (vm->exception.tt == MRBC_TT_EXCEPTION) return;
    }
    if (sec < 0 || 60 < sec) {
      mrbc_raise(vm, MRBC_CLASS(ArgumentError), "sec out of range");
      return;
    }
    usec = (sec - (int)sec) * USEC + 0.5;
    tm.tm_sec = (int)sec;
  } else tm.tm_sec = 0;
  mrbc_value time = new_from_tm(vm, v, &tm, utc, fixed, offset_east);
  PICORB_TIME *data = (PICORB_TIME *)time.instance->data;
  data->unixtime_us += usec + usec_extra;
  SET_RETURN(time);
}

static void
c_local(struct VM *vm, mrbc_value v[], int argc)
{
  time_from_fields(vm, v, argc, false, false, NULL);
}

/* Time.utc(year, mon = 1, mday = 1, hour = 0, min = 0, sec = 0, usec = 0), Time.gm */
static void
c_utc_new(struct VM *vm, mrbc_value v[], int argc)
{
  time_from_fields(vm, v, argc, true, false, NULL);
}

/* the zone of a Time, after the fact: Time.new(in: zone); 0 after raising */
static int
time_set_zone(struct VM *vm, mrbc_value *time, const mrbc_value *zone)
{
  bool utc, fixed;
  long offset_east;
  if (!zone_arg(vm, zone, &utc, &fixed, &offset_east)) return 0;
  PICORB_TIME *data = (PICORB_TIME *)time->instance->data;
  data->utc = utc;
  data->fixed = fixed;
  data->timezone = fixed ? -offset_east : 0;
  time_refresh(data);
  return 1;
}

/* Time.new("2026-11-05 12:00:00[.frac][ zone]"), Time.new("2026"): the
 * string's own zone wins over in: */
static void
time_from_string(struct VM *vm, mrbc_value v[], const mrbc_value *str, const mrbc_value *in_zone)
{
  struct tm tm;
  int usec;
  bool has_zone, utc;
  long offset_east;
  const char *err = picorb_parse_time_string((const char *)str->string->data, str->string->size,
                                             &tm, &usec, &has_zone, &utc, &offset_east);
  if (err) {
    mrbc_raise(vm, MRBC_CLASS(ArgumentError), err);
    return;
  }
  bool fixed = has_zone && !utc;
  if (!has_zone && in_zone && !zone_arg(vm, in_zone, &utc, &fixed, &offset_east)) return;
  mrbc_value time = new_from_tm(vm, v, &tm, utc, fixed, offset_east);
  PICORB_TIME *data = (PICORB_TIME *)time.instance->data;
  data->unixtime_us += usec;
  SET_RETURN(time);
}

/* Time.at(seconds), Time.at(seconds, usec), Time.at(seconds, sub, unit)
 * with unit :millisecond, :usec/:microsecond or :nsec/:nanosecond */
static void
c_at(struct VM *vm, mrbc_value v[], int argc)
{
  if (argc < 1 || 3 < argc) {
    mrbc_raise(vm, MRBC_CLASS(ArgumentError), "wrong number of arguments (expected 1..3)");
    return;
  }
  mrbc_int_t us;
  if (v[1].tt == MRBC_TT_FLOAT) {
    us = (mrbc_int_t)(v[1].d * USEC + (v[1].d < 0 ? -0.5 : 0.5));
  } else if (v[1].tt == MRBC_TT_INTEGER) {
    us = (mrbc_int_t)v[1].i * USEC;
  } else {
    mrbc_raise(vm, MRBC_CLASS(TypeError), "can't convert into an exact number");
    return;
  }
  if (argc >= 2) {
    double sub = v[2].tt == MRBC_TT_FLOAT ? v[2].d : (double)v[2].i;
    double scale = 1.0;
    if (argc == 3) {
      const char *u = v[3].tt == MRBC_TT_SYMBOL ? mrbc_symbol_cstr(&v[3]) : (v[3].tt == MRBC_TT_STRING ? mrbc_string_cstr(&v[3]) : "");
      if (strcmp(u, "millisecond") == 0) scale = 1000.0;
      else if (strcmp(u, "usec") == 0 || strcmp(u, "microsecond") == 0) scale = 1.0;
      else if (strcmp(u, "nsec") == 0 || strcmp(u, "nanosecond") == 0) scale = 0.001;
      else { mrbc_raise(vm, MRBC_CLASS(ArgumentError), "unexpected unit"); return; }
    }
    us += (mrbc_int_t)(sub * scale);
  }
  SET_RETURN(new_from_unixtime_us(vm, v, us - (mrbc_int_t)unixtime_offset * USEC));
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


/* Time.new, Time.new(year, ..., sec, zone = nil, in: nil),
 * Time.new(string, in: nil). mruby/c hands a keyword Hash in at
 * v[argc + 1], after the positional arguments. */
static void
c_new(struct VM *vm, mrbc_value v[], int argc)
{
  const mrbc_value *in_zone = NULL;
  if (v[argc + 1].tt == MRBC_TT_HASH) {
    mrbc_value *key = mrbc_hash_search_by_id(&v[argc + 1], mrbc_str_to_symid("in"));
    if (key) {
      in_zone = key + 1;
    } else if (v[argc + 1].hash->n_stored > 0) {
      mrbc_raise(vm, MRBC_CLASS(ArgumentError), "unknown keyword");
      return;
    }
  }
  if (argc == 0) {
    c_now(vm, v, 0);
    if (in_zone) time_set_zone(vm, &v[0], in_zone);
  } else if (v[1].tt == MRBC_TT_STRING) {
    if (argc != 1) {
      mrbc_raise(vm, MRBC_CLASS(ArgumentError), "wrong number of arguments (expected 1)");
      return;
    }
    time_from_string(vm, v, &v[1], in_zone);
  } else if (7 < argc) {
    mrbc_raise(vm, MRBC_CLASS(ArgumentError), "wrong number of arguments (expected 0..7)");
  } else if (in_zone && argc == 7) {
    mrbc_raise(vm, MRBC_CLASS(ArgumentError), "timezone argument given as positional and keyword arguments");
  } else {
    time_from_fields(vm, v, argc, false, true, in_zone);
  }
}


/*
 * Instance methods
 */

static void
c_to_i(struct VM *vm, mrbc_value v[], int argc)
{
  PICORB_TIME *data = (PICORB_TIME *)v->instance->data;
  SET_INT_RETURN((mrbc_int_t)data->unixtime_us / USEC);
}

static void
c_to_f(struct VM *vm, mrbc_value v[], int argc)
{
  PICORB_TIME *data = (PICORB_TIME *)v->instance->data;
  SET_FLOAT_RETURN((mrbc_float_t)data->unixtime_us / USEC);
}

#define INSPECT_LENGTH 80

static void
c_to_s(struct VM *vm, mrbc_value v[], int argc)
{
  PICORB_TIME *data = (PICORB_TIME *)v->instance->data;
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
  SET_RETURN(mrbc_string_new_cstr(vm, str));
}

static void
c_inspect(struct VM *vm, mrbc_value v[], int argc)
{
  if (v[0].tt == MRBC_TT_CLASS) {
    SET_RETURN(mrbc_string_new_cstr(vm, "Time"));
    return;
  }
  PICORB_TIME *data = (PICORB_TIME *)v->instance->data;
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
  SET_RETURN(mrbc_string_new_cstr(vm, str));
}

static void
c_year(struct VM *vm, mrbc_value v[], int argc)
{
  SET_INT_RETURN(((PICORB_TIME *)v->instance->data)->tm.tm_year + 1900);
}
static void
c_mon(struct VM *vm, mrbc_value v[], int argc)
{
  SET_INT_RETURN(((PICORB_TIME *)v->instance->data)->tm.tm_mon + 1);
}
static void
c_mday(struct VM *vm, mrbc_value v[], int argc)
{
  SET_INT_RETURN(((PICORB_TIME *)v->instance->data)->tm.tm_mday);
}
static void
c_hour(struct VM *vm, mrbc_value v[], int argc)
{
  SET_INT_RETURN(((PICORB_TIME *)v->instance->data)->tm.tm_hour);
}
static void
c_min(struct VM *vm, mrbc_value v[], int argc)
{
  SET_INT_RETURN(((PICORB_TIME *)v->instance->data)->tm.tm_min);
}
static void
c_sec(struct VM *vm, mrbc_value v[], int argc)
{
  SET_INT_RETURN(((PICORB_TIME *)v->instance->data)->tm.tm_sec);
}
static void
c_usec(struct VM *vm, mrbc_value v[], int argc)
{
  SET_INT_RETURN(((PICORB_TIME *)v->instance->data)->unixtime_us % USEC);
}
static void
c_wday(struct VM *vm, mrbc_value v[], int argc)
{
  SET_INT_RETURN(((PICORB_TIME *)v->instance->data)->tm.tm_wday);
}

static void
c_yday(struct VM *vm, mrbc_value v[], int argc)
{
  PICORB_TIME *data = (PICORB_TIME *)v->instance->data;
  SET_INT_RETURN(data->tm.tm_yday + 1);
}

static void
c_isdst(struct VM *vm, mrbc_value v[], int argc)
{
  PICORB_TIME *data = (PICORB_TIME *)v->instance->data;
  SET_BOOL_RETURN(data->tm.tm_isdst > 0);
}

/* Time#utc, #gmtime: the same instant as a UTC time, in place. mruby/c
 * does not tell a class method from an instance method, so Time.utc
 * lands here too. */
static void
c_utc(struct VM *vm, mrbc_value v[], int argc)
{
  if (v[0].tt == MRBC_TT_CLASS) {
    c_utc_new(vm, v, argc);
    return;
  }
  PICORB_TIME *data = (PICORB_TIME *)v->instance->data;
  data->utc = 1;
  data->fixed = 0;
  data->timezone = 0;
  time_refresh(data);
  mrbc_incref(&v[0]);
  SET_RETURN(v[0]);
}

/* Time#getutc, #getgm: a UTC copy */
static void
c_getutc(struct VM *vm, mrbc_value v[], int argc)
{
  PICORB_TIME *data = (PICORB_TIME *)v->instance->data;
  PICORB_TIME proto = *data;
  proto.utc = 1;
  proto.fixed = 0;
  proto.timezone = 0;
  SET_RETURN(new_from_unixtime_us_like(vm, v, data->unixtime_us - unixtime_offset * USEC, &proto));
}

/* Time#localtime(zone = nil): the local zone, or the given offset, in place */
static void
c_localtime(struct VM *vm, mrbc_value v[], int argc)
{
  PICORB_TIME *data = (PICORB_TIME *)v->instance->data;
  bool utc, fixed;
  long offset_east;
  if (!zone_arg(vm, argc >= 1 ? &v[1] : NULL, &utc, &fixed, &offset_east)) return;
  data->utc = utc;
  data->fixed = fixed;
  data->timezone = fixed ? -offset_east : 0;
  time_refresh(data);
  mrbc_incref(&v[0]);
  SET_RETURN(v[0]);
}

/* Time#getlocal(zone = nil): a copy in the local zone or the given offset */
static void
c_getlocal(struct VM *vm, mrbc_value v[], int argc)
{
  PICORB_TIME *data = (PICORB_TIME *)v->instance->data;
  PICORB_TIME proto = *data;
  bool utc, fixed;
  long offset_east;
  if (!zone_arg(vm, argc >= 1 ? &v[1] : NULL, &utc, &fixed, &offset_east)) return;
  proto.utc = utc;
  proto.fixed = fixed;
  proto.timezone = fixed ? -offset_east : 0;
  SET_RETURN(new_from_unixtime_us_like(vm, v, data->unixtime_us - unixtime_offset * USEC, &proto));
}

static void
c_utc_p(struct VM *vm, mrbc_value v[], int argc)
{
  PICORB_TIME *data = (PICORB_TIME *)v->instance->data;
  SET_BOOL_RETURN(data->utc);
}

/* Time#utc_offset, #gmt_offset, #gmtoff: seconds east of UTC */
static void
c_utc_offset(struct VM *vm, mrbc_value v[], int argc)
{
  PICORB_TIME *data = (PICORB_TIME *)v->instance->data;
  SET_INT_RETURN(data->utc ? 0 : (mrbc_int_t)-data->timezone);
}

/* Time#zone: "UTC", the local zone's abbreviation, or nil for a fixed
 * offset and on a platform that does not know the abbreviation */
static void
c_zone(struct VM *vm, mrbc_value v[], int argc)
{
  PICORB_TIME *data = (PICORB_TIME *)v->instance->data;
  if (data->utc) { SET_RETURN(mrbc_string_new_cstr(vm, "UTC")); return; }
  if (data->fixed) { SET_NIL_RETURN(); return; }
  const char *zone = picorb_local_zone(&data->tm);
  if (zone) { SET_RETURN(mrbc_string_new_cstr(vm, zone)); } else { SET_NIL_RETURN(); }
}

static void
c_hash(struct VM *vm, mrbc_value v[], int argc)
{
  PICORB_TIME *data = (PICORB_TIME *)v->instance->data;
  mrbc_int_t us = data->unixtime_us;
  SET_INT_RETURN(us ^ (us >> 16));
}

static int
mrbc_time_compare(mrbc_value *self, mrbc_value *other)
{
  if (other->tt != MRBC_TT_OBJECT || self->instance->cls != other->instance->cls) {
    return -2;
  }
  mrbc_int_t other_unixtime_us = ((PICORB_TIME *)other->instance->data)->unixtime_us;
  mrbc_int_t self_unixtime_us = ((PICORB_TIME *)self->instance->data)->unixtime_us;
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
  PICORB_TIME *self_data = (PICORB_TIME *)v[0].instance->data;
  mrbc_int_t self_unixtime_us = self_data->unixtime_us;
  mrbc_int_t base = self_unixtime_us - unixtime_offset * USEC;
  switch (v[1].tt) {
    case MRBC_TT_INTEGER:
    {
      SET_RETURN(new_from_unixtime_us_like(vm, v, base - (mrbc_int_t)v[1].i * USEC, self_data));
      break;
    }
    case MRBC_TT_FLOAT:
    {
      SET_RETURN(new_from_unixtime_us_like(vm, v, base - (mrbc_int_t)(v[1].d * USEC), self_data));
      break;
    }
    default:
    {
      if (v[0].instance->cls == v[1].instance->cls) {
        mrbc_int_t other_unixtime_us = ((PICORB_TIME *)v[1].instance->data)->unixtime_us;
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
  PICORB_TIME *self_data = (PICORB_TIME *)v[0].instance->data;
  mrbc_int_t base = self_data->unixtime_us - unixtime_offset * USEC;
  switch (v[1].tt) {
    case MRBC_TT_INTEGER:
    {
      SET_RETURN(new_from_unixtime_us_like(vm, v, base + (mrbc_int_t)v[1].i * USEC, self_data));
      break;
    }
    case MRBC_TT_FLOAT:
    {
      SET_RETURN(new_from_unixtime_us_like(vm, v, base + (mrbc_int_t)(v[1].d * USEC), self_data));
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
mrbc_time_init(mrbc_vm *vm)
{
  mrbc_class *class_Time = mrbc_define_class(vm, "Time", mrbc_class_object);
  class_TimeMethods = mrbc_define_class_under(vm, class_Time, "TimeMethods", mrbc_class_object);

  mrbc_define_method(vm, class_Time, "unixtime_offset", c_unixtime_offset);
  mrbc_define_method(vm, class_Time, "mktime", c_local);
  mrbc_define_method(vm, class_Time, "local", c_local);
  mrbc_define_method(vm, class_Time, "gm", c_utc_new);
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
  mrbc_define_method(vm, class_Time, "yday", c_yday);
  mrbc_define_method(vm, class_Time, "isdst", c_isdst);
  mrbc_define_method(vm, class_Time, "dst?", c_isdst);
  mrbc_define_method(vm, class_Time, "utc", c_utc);
  mrbc_define_method(vm, class_Time, "gmtime", c_utc);
  mrbc_define_method(vm, class_Time, "getutc", c_getutc);
  mrbc_define_method(vm, class_Time, "getgm", c_getutc);
  mrbc_define_method(vm, class_Time, "localtime", c_localtime);
  mrbc_define_method(vm, class_Time, "getlocal", c_getlocal);
  mrbc_define_method(vm, class_Time, "utc?", c_utc_p);
  mrbc_define_method(vm, class_Time, "gmt?", c_utc_p);
  mrbc_define_method(vm, class_Time, "utc_offset", c_utc_offset);
  mrbc_define_method(vm, class_Time, "gmt_offset", c_utc_offset);
  mrbc_define_method(vm, class_Time, "gmtoff", c_utc_offset);
  mrbc_define_method(vm, class_Time, "zone", c_zone);
  mrbc_define_method(vm, class_Time, "eql?", c_eq);
  mrbc_define_method(vm, class_Time, "hash", c_hash);
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
