#include "mruby.h"
#include "mruby/presym.h"
#include "mruby/class.h"
#include "mruby/data.h"
#include "mruby/string.h"
#include "mruby/variable.h"
#include "mruby/array.h"

#include "regex.h"

#include <string.h>
#include <stdlib.h>
#include <stdio.h>

/* Data structures */

typedef struct picorb_regexp_light {
  regex_t regex;
  mrb_value source;
  int flags;
} picorb_regexp_light;

typedef struct picorb_match_data_light {
  regmatch_t *pmatch;
  size_t nmatch;
  mrb_value str;
  mrb_value regexp;
} picorb_match_data_light;

static struct RClass *class_Regexp;
static struct RClass *class_MatchData;

/* Free functions */

static void
regexp_light_free(mrb_state *mrb, void *ptr)
{
  picorb_regexp_light *re = (picorb_regexp_light *)ptr;
  if (re) {
    regfree(&re->regex);
    mrb_free(mrb, re);
  }
}

static void
match_data_light_free(mrb_state *mrb, void *ptr)
{
  picorb_match_data_light *md = (picorb_match_data_light *)ptr;
  if (md) {
    if (md->pmatch) free(md->pmatch);
    mrb_free(mrb, md);
  }
}

static const struct mrb_data_type picorb_regexp_light_type = {
  "picorb_regexp_light", regexp_light_free
};

static const struct mrb_data_type picorb_match_data_light_type = {
  "picorb_match_data_light", match_data_light_free
};

/*
 * Convert Ruby-style anchors in pattern to regex_light equivalents.
 * \A -> ^, \z -> $, \Z -> $
 * Caller must mrb_free the returned string.
 */
static char *
convert_ruby_pattern(mrb_state *mrb, const char *pattern, mrb_int len)
{
  char *out = (char *)mrb_malloc(mrb, (size_t)len + 1);
  mrb_int i = 0, j = 0;
  while (i < len) {
    if (pattern[i] == '\\' && i + 1 < len) {
      switch (pattern[i + 1]) {
        case 'A':
          out[j++] = '^';
          i += 2;
          break;
        case 'z':
        case 'Z':
          out[j++] = '$';
          i += 2;
          break;
        default:
          out[j++] = pattern[i++];
          out[j++] = pattern[i++];
          break;
      }
    } else {
      out[j++] = pattern[i++];
    }
  }
  out[j] = '\0';
  return out;
}

/* Build cflags integer from Ruby flags string */
static int
build_cflags(const char *flags, int len)
{
  int cflags = 0;
  int i = 0;
  while (i < len) {
    if (flags[i] == 'i') cflags |= REG_ICASE;
    if (flags[i] == 'm') cflags |= REG_NEWLINE;
    /* 'x' is not supported by regex_light; silently ignored */
    i++;
  }
  return cflags;
}

/* Create Regexp object */
static mrb_value
regexp_light_create_obj(mrb_state *mrb, const char *pattern, mrb_int plen,
                         const char *flags, int flen)
{
  char *converted = convert_ruby_pattern(mrb, pattern, plen);

  picorb_regexp_light *re =
    (picorb_regexp_light *)mrb_malloc(mrb, sizeof(picorb_regexp_light));

  int r = regcomp(&re->regex, converted, 0);
  mrb_free(mrb, converted);

  if (r != 0) {
    mrb_free(mrb, re);
    mrb_raise(mrb, E_ARGUMENT_ERROR, "invalid regular expression");
  }

  re->source = mrb_str_new(mrb, pattern, plen);
  re->flags = flags ? build_cflags(flags, flen) : 0;

  struct RData *rdata =
    mrb_data_object_alloc(mrb, class_Regexp, re, &picorb_regexp_light_type);
  return mrb_obj_value(rdata);
}

/* Create MatchData object */
static mrb_value
match_data_light_create_obj(mrb_state *mrb, regmatch_t *pmatch, size_t nmatch,
                             mrb_value str, mrb_value regexp)
{
  picorb_match_data_light *md =
    (picorb_match_data_light *)mrb_malloc(mrb, sizeof(picorb_match_data_light));

  md->pmatch = (regmatch_t *)malloc(sizeof(regmatch_t) * nmatch);
  if (!md->pmatch) {
    mrb_free(mrb, md);
    mrb_raise(mrb, E_RUNTIME_ERROR, "out of memory");
  }
  memcpy(md->pmatch, pmatch, sizeof(regmatch_t) * nmatch);
  md->nmatch = nmatch;
  md->str = str;
  md->regexp = regexp;

  struct RData *rdata =
    mrb_data_object_alloc(mrb, class_MatchData, md, &picorb_match_data_light_type);
  return mrb_obj_value(rdata);
}

/* Run regex match; return MatchData or nil */
static mrb_value
do_match(mrb_state *mrb, mrb_value re_val, mrb_value str_val)
{
  picorb_regexp_light *re = (picorb_regexp_light *)DATA_PTR(re_val);
  if (!re) mrb_raise(mrb, E_RUNTIME_ERROR, "Regexp not initialized");

  const char *str = RSTRING_PTR(str_val);
  size_t nmatch = re->regex.re_nsub + 1;
  regmatch_t *pmatch = (regmatch_t *)malloc(sizeof(regmatch_t) * nmatch);
  if (!pmatch) mrb_raise(mrb, E_RUNTIME_ERROR, "out of memory");

  int r = regexec(&re->regex, str, nmatch, pmatch, 0);
  if (r != 0 || pmatch[0].rm_so < 0) {
    free(pmatch);
    return mrb_nil_value();
  }

  mrb_value frozen_str =
    mrb_str_new(mrb, RSTRING_PTR(str_val), RSTRING_LEN(str_val));
  mrb_obj_freeze(mrb, frozen_str);
  mrb_value md = match_data_light_create_obj(mrb, pmatch, nmatch, frozen_str, re_val);
  free(pmatch);
  return md;
}

/* Ensure argument is a Regexp; compile String if given */
static mrb_value
ensure_regexp(mrb_state *mrb, mrb_value pattern)
{
  if (mrb_obj_is_instance_of(mrb, pattern, class_Regexp)) {
    return pattern;
  }
  if (mrb_string_p(pattern)) {
    return regexp_light_create_obj(mrb,
      RSTRING_PTR(pattern), RSTRING_LEN(pattern), NULL, 0);
  }
  mrb_raise(mrb, E_TYPE_ERROR,
    "wrong argument type (expected Regexp or String)");
  return mrb_nil_value();
}

/* ---- Regexp class methods ---- */

/* Regexp.compile(pattern, flags=nil, encoding=nil) / Regexp.new */
static mrb_value
mrb_regexp_light_compile(mrb_state *mrb, mrb_value self)
{
  const char *pattern;
  mrb_int plen;
  mrb_value flags_val = mrb_nil_value();
  mrb_value enc_val = mrb_nil_value();

  mrb_get_args(mrb, "s|oo", &pattern, &plen, &flags_val, &enc_val);
  /* enc_val is accepted but ignored */

  const char *flags = NULL;
  int flen = 0;
  if (mrb_string_p(flags_val)) {
    flags = RSTRING_PTR(flags_val);
    flen = (int)RSTRING_LEN(flags_val);
  } else if (mrb_fixnum_p(flags_val)) {
    /* Integer flags: IGNORECASE=1, EXTENDED=2, MULTILINE=4 */
    mrb_int opts = mrb_fixnum(flags_val);
    char ruby_flags[4];
    int k = 0;
    if (opts & 1) ruby_flags[k++] = 'i';
    if (opts & 4) ruby_flags[k++] = 'm';
    ruby_flags[k] = '\0';
    return regexp_light_create_obj(mrb, pattern, plen, ruby_flags, k);
  }

  return regexp_light_create_obj(mrb, pattern, plen, flags, flen);
}

/* Regexp#match(str) -> MatchData or nil */
static mrb_value
mrb_regexp_light_match(mrb_state *mrb, mrb_value self)
{
  mrb_value str_val;
  mrb_get_args(mrb, "S", &str_val);
  return do_match(mrb, self, str_val);
}

/* Regexp#match?(str) -> bool */
static mrb_value
mrb_regexp_light_match_p(mrb_state *mrb, mrb_value self)
{
  mrb_value str_val;
  mrb_get_args(mrb, "S", &str_val);

  picorb_regexp_light *re = (picorb_regexp_light *)DATA_PTR(self);
  if (!re) mrb_raise(mrb, E_RUNTIME_ERROR, "Regexp not initialized");

  regmatch_t pmatch[1];
  int r = regexec(&re->regex, RSTRING_PTR(str_val), 1, pmatch, 0);
  return mrb_bool_value(r == 0);
}

/* Regexp#=~(str) -> Integer or nil */
static mrb_value
mrb_regexp_light_match_op(mrb_state *mrb, mrb_value self)
{
  mrb_value str_val;
  mrb_get_args(mrb, "S", &str_val);

  picorb_regexp_light *re = (picorb_regexp_light *)DATA_PTR(self);
  if (!re) mrb_raise(mrb, E_RUNTIME_ERROR, "Regexp not initialized");

  regmatch_t pmatch[1];
  int r = regexec(&re->regex, RSTRING_PTR(str_val), 1, pmatch, 0);
  if (r != 0 || pmatch[0].rm_so < 0) return mrb_nil_value();
  return mrb_fixnum_value(pmatch[0].rm_so);
}

/* Regexp#source -> String */
static mrb_value
mrb_regexp_light_source(mrb_state *mrb, mrb_value self)
{
  picorb_regexp_light *re = (picorb_regexp_light *)DATA_PTR(self);
  if (!re) mrb_raise(mrb, E_RUNTIME_ERROR, "Regexp not initialized");
  return re->source;
}

/* Regexp#to_s -> String "(? flags:source)" */
static mrb_value
mrb_regexp_light_to_s(mrb_state *mrb, mrb_value self)
{
  picorb_regexp_light *re = (picorb_regexp_light *)DATA_PTR(self);
  if (!re) mrb_raise(mrb, E_RUNTIME_ERROR, "Regexp not initialized");

  char flags[8];
  int fi = 0;
  if (re->flags & REG_ICASE) flags[fi++] = 'i';
  if (re->flags & REG_NEWLINE) flags[fi++] = 'm';
  flags[fi] = '\0';

  mrb_value result = mrb_str_new_cstr(mrb, "(?");
  mrb_str_cat_cstr(mrb, result, flags);
  mrb_str_cat_cstr(mrb, result, ":");
  mrb_str_cat(mrb, result,
    RSTRING_PTR(re->source), RSTRING_LEN(re->source));
  mrb_str_cat_cstr(mrb, result, ")");
  return result;
}

/* Regexp#inspect -> String "/source/flags" */
static mrb_value
mrb_regexp_light_inspect(mrb_state *mrb, mrb_value self)
{
  picorb_regexp_light *re = (picorb_regexp_light *)DATA_PTR(self);
  if (!re) mrb_raise(mrb, E_RUNTIME_ERROR, "Regexp not initialized");

  char flags[8];
  int fi = 0;
  if (re->flags & REG_ICASE) flags[fi++] = 'i';
  if (re->flags & REG_NEWLINE) flags[fi++] = 'm';
  flags[fi] = '\0';

  mrb_value result = mrb_str_new_cstr(mrb, "/");
  mrb_str_cat(mrb, result,
    RSTRING_PTR(re->source), RSTRING_LEN(re->source));
  mrb_str_cat_cstr(mrb, result, "/");
  mrb_str_cat_cstr(mrb, result, flags);
  return result;
}

/* Regexp#casefold? -> bool */
static mrb_value
mrb_regexp_light_casefold_p(mrb_state *mrb, mrb_value self)
{
  picorb_regexp_light *re = (picorb_regexp_light *)DATA_PTR(self);
  if (!re) mrb_raise(mrb, E_RUNTIME_ERROR, "Regexp not initialized");
  return mrb_bool_value((re->flags & REG_ICASE) != 0);
}

/* Regexp#options -> Integer */
static mrb_value
mrb_regexp_light_options(mrb_state *mrb, mrb_value self)
{
  picorb_regexp_light *re = (picorb_regexp_light *)DATA_PTR(self);
  if (!re) mrb_raise(mrb, E_RUNTIME_ERROR, "Regexp not initialized");
  int opts = 0;
  if (re->flags & REG_ICASE)   opts |= 1; /* IGNORECASE */
  if (re->flags & REG_NEWLINE) opts |= 4; /* MULTILINE  */
  return mrb_fixnum_value(opts);
}

/* ---- MatchData instance methods ---- */

/* MatchData#[](idx) -> String or nil */
static mrb_value
mrb_match_data_aref(mrb_state *mrb, mrb_value self)
{
  mrb_int idx;
  mrb_get_args(mrb, "i", &idx);

  picorb_match_data_light *md = (picorb_match_data_light *)DATA_PTR(self);
  if (!md) mrb_raise(mrb, E_RUNTIME_ERROR, "MatchData not initialized");

  if (idx < 0) idx += (mrb_int)md->nmatch;
  if (idx < 0 || (size_t)idx >= md->nmatch) return mrb_nil_value();

  regmatch_t *m = &md->pmatch[idx];
  if (m->rm_so < 0) return mrb_nil_value();

  const char *str = RSTRING_PTR(md->str);
  return mrb_str_new(mrb, str + m->rm_so, m->rm_eo - m->rm_so);
}

/* MatchData#to_a -> Array */
static mrb_value
mrb_match_data_to_a(mrb_state *mrb, mrb_value self)
{
  picorb_match_data_light *md = (picorb_match_data_light *)DATA_PTR(self);
  if (!md) mrb_raise(mrb, E_RUNTIME_ERROR, "MatchData not initialized");

  mrb_value ary = mrb_ary_new_capa(mrb, (mrb_int)md->nmatch);
  const char *str = RSTRING_PTR(md->str);

  for (size_t i = 0; i < md->nmatch; i++) {
    regmatch_t *m = &md->pmatch[i];
    if (m->rm_so < 0) {
      mrb_ary_push(mrb, ary, mrb_nil_value());
    } else {
      mrb_ary_push(mrb, ary,
        mrb_str_new(mrb, str + m->rm_so, m->rm_eo - m->rm_so));
    }
  }
  return ary;
}

/* MatchData#length / MatchData#size -> Integer */
static mrb_value
mrb_match_data_length(mrb_state *mrb, mrb_value self)
{
  picorb_match_data_light *md = (picorb_match_data_light *)DATA_PTR(self);
  if (!md) mrb_raise(mrb, E_RUNTIME_ERROR, "MatchData not initialized");
  return mrb_fixnum_value((mrb_int)md->nmatch);
}

/* MatchData#string -> String (frozen original) */
static mrb_value
mrb_match_data_string(mrb_state *mrb, mrb_value self)
{
  picorb_match_data_light *md = (picorb_match_data_light *)DATA_PTR(self);
  if (!md) mrb_raise(mrb, E_RUNTIME_ERROR, "MatchData not initialized");
  return md->str;
}

/* MatchData#regexp -> Regexp */
static mrb_value
mrb_match_data_regexp(mrb_state *mrb, mrb_value self)
{
  picorb_match_data_light *md = (picorb_match_data_light *)DATA_PTR(self);
  if (!md) mrb_raise(mrb, E_RUNTIME_ERROR, "MatchData not initialized");
  return md->regexp;
}

/* MatchData#pre_match -> String */
static mrb_value
mrb_match_data_pre_match(mrb_state *mrb, mrb_value self)
{
  picorb_match_data_light *md = (picorb_match_data_light *)DATA_PTR(self);
  if (!md) mrb_raise(mrb, E_RUNTIME_ERROR, "MatchData not initialized");

  regmatch_t *m = &md->pmatch[0];
  if (m->rm_so < 0) return mrb_str_new_cstr(mrb, "");
  return mrb_str_new(mrb, RSTRING_PTR(md->str), m->rm_so);
}

/* MatchData#post_match -> String */
static mrb_value
mrb_match_data_post_match(mrb_state *mrb, mrb_value self)
{
  picorb_match_data_light *md = (picorb_match_data_light *)DATA_PTR(self);
  if (!md) mrb_raise(mrb, E_RUNTIME_ERROR, "MatchData not initialized");

  regmatch_t *m = &md->pmatch[0];
  if (m->rm_eo < 0) return mrb_str_new_cstr(mrb, "");

  const char *str = RSTRING_PTR(md->str);
  mrb_int slen = RSTRING_LEN(md->str);
  return mrb_str_new(mrb, str + m->rm_eo, slen - m->rm_eo);
}

/* MatchData#begin(idx) -> Integer or nil */
static mrb_value
mrb_match_data_begin(mrb_state *mrb, mrb_value self)
{
  mrb_int idx;
  mrb_get_args(mrb, "i", &idx);

  picorb_match_data_light *md = (picorb_match_data_light *)DATA_PTR(self);
  if (!md) mrb_raise(mrb, E_RUNTIME_ERROR, "MatchData not initialized");

  if (idx < 0 || (size_t)idx >= md->nmatch) {
    mrb_raise(mrb, E_INDEX_ERROR, "index out of bounds");
  }

  regmatch_t *m = &md->pmatch[idx];
  if (m->rm_so < 0) return mrb_nil_value();
  return mrb_fixnum_value(m->rm_so);
}

/* MatchData#end(idx) -> Integer or nil */
static mrb_value
mrb_match_data_end(mrb_state *mrb, mrb_value self)
{
  mrb_int idx;
  mrb_get_args(mrb, "i", &idx);

  picorb_match_data_light *md = (picorb_match_data_light *)DATA_PTR(self);
  if (!md) mrb_raise(mrb, E_RUNTIME_ERROR, "MatchData not initialized");

  if (idx < 0 || (size_t)idx >= md->nmatch) {
    mrb_raise(mrb, E_INDEX_ERROR, "index out of bounds");
  }

  regmatch_t *m = &md->pmatch[idx];
  if (m->rm_eo < 0) return mrb_nil_value();
  return mrb_fixnum_value(m->rm_eo);
}

/* MatchData#captures -> Array (groups without index 0) */
static mrb_value
mrb_match_data_captures(mrb_state *mrb, mrb_value self)
{
  picorb_match_data_light *md = (picorb_match_data_light *)DATA_PTR(self);
  if (!md) mrb_raise(mrb, E_RUNTIME_ERROR, "MatchData not initialized");

  if (md->nmatch <= 1) return mrb_ary_new(mrb);

  mrb_value ary = mrb_ary_new_capa(mrb, (mrb_int)(md->nmatch - 1));
  const char *str = RSTRING_PTR(md->str);

  for (size_t i = 1; i < md->nmatch; i++) {
    regmatch_t *m = &md->pmatch[i];
    if (m->rm_so < 0) {
      mrb_ary_push(mrb, ary, mrb_nil_value());
    } else {
      mrb_ary_push(mrb, ary,
        mrb_str_new(mrb, str + m->rm_so, m->rm_eo - m->rm_so));
    }
  }
  return ary;
}

/* MatchData#to_s -> String (full match) */
static mrb_value
mrb_match_data_to_s(mrb_state *mrb, mrb_value self)
{
  picorb_match_data_light *md = (picorb_match_data_light *)DATA_PTR(self);
  if (!md) mrb_raise(mrb, E_RUNTIME_ERROR, "MatchData not initialized");

  regmatch_t *m = &md->pmatch[0];
  if (m->rm_so < 0) return mrb_str_new_cstr(mrb, "");

  const char *str = RSTRING_PTR(md->str);
  return mrb_str_new(mrb, str + m->rm_so, m->rm_eo - m->rm_so);
}

/* MatchData#inspect -> String */
static mrb_value
mrb_match_data_inspect(mrb_state *mrb, mrb_value self)
{
  picorb_match_data_light *md = (picorb_match_data_light *)DATA_PTR(self);
  if (!md) mrb_raise(mrb, E_RUNTIME_ERROR, "MatchData not initialized");

  regmatch_t *m = &md->pmatch[0];
  if (m->rm_so < 0) return mrb_str_new_cstr(mrb, "#<MatchData (no match)>");

  const char *str = RSTRING_PTR(md->str);
  mrb_value result = mrb_str_new_cstr(mrb, "#<MatchData \"");
  mrb_str_cat(mrb, result, str + m->rm_so, m->rm_eo - m->rm_so);
  mrb_str_cat_cstr(mrb, result, "\"");

  for (size_t i = 1; i < md->nmatch; i++) {
    m = &md->pmatch[i];
    char buf[32];
    snprintf(buf, sizeof(buf), " %zu:", i);
    mrb_str_cat_cstr(mrb, result, buf);
    if (m->rm_so < 0) {
      mrb_str_cat_cstr(mrb, result, "nil");
    } else {
      mrb_str_cat_cstr(mrb, result, "\"");
      mrb_str_cat(mrb, result, str + m->rm_so, m->rm_eo - m->rm_so);
      mrb_str_cat_cstr(mrb, result, "\"");
    }
  }
  mrb_str_cat_cstr(mrb, result, ">");
  return result;
}

/* ---- String extensions ---- */

/* String#match(regexp_or_str) -> MatchData or nil */
static mrb_value
mrb_string_match_light(mrb_state *mrb, mrb_value self)
{
  mrb_value pattern;
  mrb_get_args(mrb, "o", &pattern);
  mrb_value re = ensure_regexp(mrb, pattern);
  return do_match(mrb, re, self);
}

/* String#match?(regexp_or_str) -> bool */
static mrb_value
mrb_string_match_p_light(mrb_state *mrb, mrb_value self)
{
  mrb_value pattern;
  mrb_get_args(mrb, "o", &pattern);
  mrb_value re = ensure_regexp(mrb, pattern);

  picorb_regexp_light *re_data = (picorb_regexp_light *)DATA_PTR(re);
  regmatch_t pmatch[1];
  int r = regexec(&re_data->regex, RSTRING_PTR(self), 1, pmatch, 0);
  return mrb_bool_value(r == 0);
}

/* String#=~(regexp) -> Integer or nil */
static mrb_value
mrb_string_match_op_light(mrb_state *mrb, mrb_value self)
{
  mrb_value pattern;
  mrb_get_args(mrb, "o", &pattern);
  mrb_value re = ensure_regexp(mrb, pattern);

  picorb_regexp_light *re_data = (picorb_regexp_light *)DATA_PTR(re);
  regmatch_t pmatch[1];
  int r = regexec(&re_data->regex, RSTRING_PTR(self), 1, pmatch, 0);
  if (r != 0 || pmatch[0].rm_so < 0) return mrb_nil_value();
  return mrb_fixnum_value(pmatch[0].rm_so);
}

/* ---- Gem init ---- */

void
mrb_picoruby_regexp_light_gem_init(mrb_state *mrb)
{
  /* Regexp class */
  class_Regexp = mrb_define_class_id(mrb, MRB_SYM(Regexp), mrb->object_class);
  MRB_SET_INSTANCE_TT(class_Regexp, MRB_TT_DATA);

  mrb_define_class_method_id(mrb, class_Regexp, MRB_SYM(compile),
    mrb_regexp_light_compile, MRB_ARGS_ARG(1, 2));
  mrb_define_class_method_id(mrb, class_Regexp, MRB_SYM(new),
    mrb_regexp_light_compile, MRB_ARGS_ARG(1, 2));

  mrb_define_method_id(mrb, class_Regexp, MRB_SYM(match),
    mrb_regexp_light_match, MRB_ARGS_REQ(1));
  mrb_define_method_id(mrb, class_Regexp, MRB_SYM_Q(match),
    mrb_regexp_light_match_p, MRB_ARGS_REQ(1));
  mrb_define_method_id(mrb, class_Regexp, mrb_intern_lit(mrb, "=~"),
    mrb_regexp_light_match_op, MRB_ARGS_REQ(1));
  mrb_define_method_id(mrb, class_Regexp, MRB_SYM(source),
    mrb_regexp_light_source, MRB_ARGS_NONE());
  mrb_define_method_id(mrb, class_Regexp, MRB_SYM(to_s),
    mrb_regexp_light_to_s, MRB_ARGS_NONE());
  mrb_define_method_id(mrb, class_Regexp, MRB_SYM(inspect),
    mrb_regexp_light_inspect, MRB_ARGS_NONE());
  mrb_define_method_id(mrb, class_Regexp, MRB_SYM_Q(casefold),
    mrb_regexp_light_casefold_p, MRB_ARGS_NONE());
  mrb_define_method_id(mrb, class_Regexp, MRB_SYM(options),
    mrb_regexp_light_options, MRB_ARGS_NONE());

  /* MatchData class */
  class_MatchData = mrb_define_class_id(mrb, MRB_SYM(MatchData), mrb->object_class);
  MRB_SET_INSTANCE_TT(class_MatchData, MRB_TT_DATA);

  mrb_define_method_id(mrb, class_MatchData, MRB_OPSYM(aref),
    mrb_match_data_aref, MRB_ARGS_REQ(1));
  mrb_define_method_id(mrb, class_MatchData, MRB_SYM(to_a),
    mrb_match_data_to_a, MRB_ARGS_NONE());
  mrb_define_method_id(mrb, class_MatchData, MRB_SYM(length),
    mrb_match_data_length, MRB_ARGS_NONE());
  mrb_define_method_id(mrb, class_MatchData, MRB_SYM(size),
    mrb_match_data_length, MRB_ARGS_NONE());
  mrb_define_method_id(mrb, class_MatchData, MRB_SYM(string),
    mrb_match_data_string, MRB_ARGS_NONE());
  mrb_define_method_id(mrb, class_MatchData, MRB_SYM(regexp),
    mrb_match_data_regexp, MRB_ARGS_NONE());
  mrb_define_method_id(mrb, class_MatchData, MRB_SYM(pre_match),
    mrb_match_data_pre_match, MRB_ARGS_NONE());
  mrb_define_method_id(mrb, class_MatchData, MRB_SYM(post_match),
    mrb_match_data_post_match, MRB_ARGS_NONE());
  mrb_define_method_id(mrb, class_MatchData, MRB_SYM(begin),
    mrb_match_data_begin, MRB_ARGS_REQ(1));
  mrb_define_method_id(mrb, class_MatchData, MRB_SYM(end),
    mrb_match_data_end, MRB_ARGS_REQ(1));
  mrb_define_method_id(mrb, class_MatchData, MRB_SYM(captures),
    mrb_match_data_captures, MRB_ARGS_NONE());
  mrb_define_method_id(mrb, class_MatchData, MRB_SYM(to_s),
    mrb_match_data_to_s, MRB_ARGS_NONE());
  mrb_define_method_id(mrb, class_MatchData, MRB_SYM(inspect),
    mrb_match_data_inspect, MRB_ARGS_NONE());

  /* String extensions */
  struct RClass *string_class = mrb->string_class;
  mrb_define_method_id(mrb, string_class, MRB_SYM(match),
    mrb_string_match_light, MRB_ARGS_REQ(1));
  mrb_define_method_id(mrb, string_class, MRB_SYM_Q(match),
    mrb_string_match_p_light, MRB_ARGS_REQ(1));
  mrb_define_method_id(mrb, string_class, mrb_intern_lit(mrb, "=~"),
    mrb_string_match_op_light, MRB_ARGS_REQ(1));
}

void
mrb_picoruby_regexp_light_gem_final(mrb_state *mrb)
{
}
