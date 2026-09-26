#include "mruby.h"
#include "mruby/presym.h"
#include "mruby/class.h"
#include "mruby/data.h"
#include "mruby/string.h"
#include "mruby/variable.h"
#include "mruby/array.h"

#include <string.h>
#include <stdio.h>

/*
 * Regexp: a data object holding the compiled program and a scratch
 * block for the VM, sized once at compile time so a match allocates
 * nothing. The pattern text is the ivar @source, which the GC marks.
 *
 * MatchData: a data object holding the capture positions as byte
 * offsets, with the frozen subject as @string and the Regexp as
 * @regexp.
 */

typedef struct {
  uint32_t *prog;      /* engine words, mrb_malloc'd */
  uint32_t *scratch;   /* regex_scratch_words(prog) words */
  int options;         /* Ruby option bits */
} picorb_regexp;

typedef struct {
  int32_t *caps;       /* nsave byte offsets, -1 for a group not taken */
  int nsave;
} picorb_match_data;

/* RX_MAX_GROUPS in the engine is 16, so nsave is at most 32 */
#define PICORB_RX_MAX_NSAVE 32

static struct RClass *class_Regexp;
static struct RClass *class_MatchData;

/* The engine's compiler allocates through these names. Compilation
 * runs under one mrb_state at a time, set just before the call. The
 * VM allocates nothing. */
static mrb_state *rx_mrb;

void *
regex_malloc(size_t n)
{
  return mrb_malloc_simple(rx_mrb, n);
}

void *
regex_realloc(void *p, size_t n)
{
  return mrb_realloc_simple(rx_mrb, p, n);
}

void
regex_free(void *p)
{
  mrb_free(rx_mrb, p);
}

static void
regexp_free(mrb_state *mrb, void *ptr)
{
  picorb_regexp *re = (picorb_regexp *)ptr;
  if (re) {
    mrb_free(mrb, re->prog);
    mrb_free(mrb, re->scratch);
    mrb_free(mrb, re);
  }
}

static void
match_data_free(mrb_state *mrb, void *ptr)
{
  picorb_match_data *md = (picorb_match_data *)ptr;
  if (md) {
    mrb_free(mrb, md->caps);
    mrb_free(mrb, md);
  }
}

static const struct mrb_data_type regexp_type = { "picorb_regexp", regexp_free };
static const struct mrb_data_type match_data_type = { "picorb_match_data", match_data_free };

static void
raise_nomem(mrb_state *mrb)
{
  mrb_exc_raise(mrb, mrb_obj_value(mrb->nomem_err));
}

static picorb_regexp *
get_regexp(mrb_state *mrb, mrb_value obj)
{
  return (picorb_regexp *)mrb_data_get_ptr(mrb, obj, &regexp_type);
}

static picorb_match_data *
get_match_data(mrb_state *mrb, mrb_value obj)
{
  return (picorb_match_data *)mrb_data_get_ptr(mrb, obj, &match_data_type);
}

static mrb_bool
regexp_p(mrb_state *mrb, mrb_value obj)
{
  return mrb_obj_is_kind_of(mrb, obj, class_Regexp);
}

/* ---- Regexp ---- */

static mrb_value
regexp_new(mrb_state *mrb, const char *src, mrb_int slen, int options)
{
  size_t nwords;
  const char *err;
  size_t err_at;

  rx_mrb = mrb;
  uint32_t *prog = regex_compile_words((const uint8_t *)src, (size_t)slen,
                                          picorb_rx_engine_flags(options),
                                          &nwords, &err, &err_at);
  if (!prog) {
    mrb_raisef(mrb, E_REGEXP_ERROR, "%s at byte %i: /%l/",
               err, (mrb_int)err_at, src, (size_t)slen);
  }
  uint32_t *scratch = (uint32_t *)mrb_malloc_simple(mrb, sizeof(uint32_t) * regex_scratch_words(prog));
  picorb_regexp *re = (picorb_regexp *)mrb_malloc_simple(mrb, sizeof(picorb_regexp));
  if (!scratch || !re) {
    mrb_free(mrb, prog);
    mrb_free(mrb, scratch);
    mrb_free(mrb, re);
    raise_nomem(mrb);
  }
  re->prog = prog;
  re->scratch = scratch;
  re->options = options;

  struct RData *data = mrb_data_object_alloc(mrb, class_Regexp, re, &regexp_type);
  mrb_value obj = mrb_obj_value(data);
  mrb_value source = mrb_str_new(mrb, src, slen);
  mrb_obj_freeze(mrb, source);
  mrb_iv_set(mrb, obj, MRB_IVSYM(source), source);
  return obj;
}

/* the options an argument of Regexp.compile stands for */
static int
options_from_arg(mrb_state *mrb, mrb_value flags)
{
  if (mrb_nil_p(flags) || mrb_false_p(flags)) return 0;
  if (mrb_string_p(flags)) {
    int options = picorb_rx_options_from_letters(RSTRING_PTR(flags), (size_t)RSTRING_LEN(flags));
    if (options < 0) {
      mrb_raisef(mrb, E_ARGUMENT_ERROR, "unknown regexp option: %v", flags);
    }
    return options;
  }
  if (mrb_integer_p(flags)) {
    return (int)(mrb_integer(flags) & (PICORB_RX_IGNORECASE | PICORB_RX_EXTENDED | PICORB_RX_MULTILINE));
  }
  /* any other truthy value means IGNORECASE, as in CRuby */
  return PICORB_RX_IGNORECASE;
}

/* Regexp.compile(pattern, options = nil, encoding = nil), Regexp.new */
static mrb_value
mrb_regexp_compile(mrb_state *mrb, mrb_value self)
{
  mrb_value pattern;
  mrb_value flags = mrb_nil_value();
  mrb_value enc = mrb_nil_value();
  mrb_get_args(mrb, "o|oo", &pattern, &flags, &enc);
  /* enc is accepted and ignored: text is UTF-8 */

  if (regexp_p(mrb, pattern)) {
    picorb_regexp *re = get_regexp(mrb, pattern);
    mrb_value source = mrb_iv_get(mrb, pattern, MRB_IVSYM(source));
    return regexp_new(mrb, RSTRING_PTR(source), RSTRING_LEN(source), re->options);
  }
  mrb_ensure_string_type(mrb, pattern);
  return regexp_new(mrb, RSTRING_PTR(pattern), RSTRING_LEN(pattern), options_from_arg(mrb, flags));
}

/* Run re over str from character position cpos. caps gets the byte
 * offsets. False when cpos is outside the string or nothing matches. */
static mrb_bool
regexp_run(picorb_regexp *re, mrb_value str, mrb_int cpos, int32_t *caps, mrb_bool first_only)
{
  const uint8_t *s = (const uint8_t *)RSTRING_PTR(str);
  size_t len = (size_t)RSTRING_LEN(str);
  if (cpos < 0) {
    cpos += picorb_utf8_length(s, len);
    if (cpos < 0) return FALSE;
  }
  long b = picorb_utf8_char_to_byte(s, len, (long)cpos);
  if (b < 0) return FALSE;
  return regex_exec(re->prog, s, len, (size_t)b, caps, first_only, re->scratch);
}

static mrb_value
match_data_new(mrb_state *mrb, mrb_value re_obj, mrb_value str, const int32_t *caps, int nsave)
{
  int32_t *copy = (int32_t *)mrb_malloc_simple(mrb, sizeof(int32_t) * (size_t)nsave);
  picorb_match_data *md = (picorb_match_data *)mrb_malloc_simple(mrb, sizeof(picorb_match_data));
  if (!copy || !md) {
    mrb_free(mrb, copy);
    mrb_free(mrb, md);
    raise_nomem(mrb);
  }
  memcpy(copy, caps, sizeof(int32_t) * (size_t)nsave);
  md->caps = copy;
  md->nsave = nsave;

  struct RData *data = mrb_data_object_alloc(mrb, class_MatchData, md, &match_data_type);
  mrb_value obj = mrb_obj_value(data);
  mrb_value frozen = mrb_str_dup(mrb, str);
  mrb_obj_freeze(mrb, frozen);
  mrb_iv_set(mrb, obj, MRB_IVSYM(string), frozen);
  mrb_iv_set(mrb, obj, MRB_IVSYM(regexp), re_obj);
  return obj;
}

/* the whole match: MatchData or nil */
static mrb_value
regexp_match_at(mrb_state *mrb, mrb_value re_obj, mrb_value str, mrb_int cpos)
{
  picorb_regexp *re = get_regexp(mrb, re_obj);
  int32_t caps[PICORB_RX_MAX_NSAVE];
  if (!regexp_run(re, str, cpos, caps, FALSE)) return mrb_nil_value();
  return match_data_new(mrb, re_obj, str, caps, (int)re->prog[1]);
}

/* yes or no, without capture positions */
static mrb_bool
regexp_test_at(mrb_state *mrb, mrb_value re_obj, mrb_value str, mrb_int cpos)
{
  picorb_regexp *re = get_regexp(mrb, re_obj);
  int32_t caps[PICORB_RX_MAX_NSAVE];
  return regexp_run(re, str, cpos, caps, TRUE);
}

/* the character offset where the match starts, or nil */
static mrb_value
regexp_index_of(mrb_state *mrb, mrb_value re_obj, mrb_value str)
{
  picorb_regexp *re = get_regexp(mrb, re_obj);
  int32_t caps[PICORB_RX_MAX_NSAVE];
  if (!regexp_run(re, str, 0, caps, FALSE)) return mrb_nil_value();
  long c = picorb_utf8_byte_to_char((const uint8_t *)RSTRING_PTR(str), (size_t)RSTRING_LEN(str), caps[0]);
  return mrb_int_value(mrb, (mrb_int)c);
}

/* Regexp#match(str, pos = 0) -> MatchData or nil */
static mrb_value
mrb_regexp_match(mrb_state *mrb, mrb_value self)
{
  mrb_value str;
  mrb_int pos = 0;
  mrb_get_args(mrb, "o|i", &str, &pos);
  if (mrb_nil_p(str)) return mrb_nil_value();
  mrb_ensure_string_type(mrb, str);
  return regexp_match_at(mrb, self, str, pos);
}

/* Regexp#match?(str, pos = 0) -> true or false */
static mrb_value
mrb_regexp_match_p(mrb_state *mrb, mrb_value self)
{
  mrb_value str;
  mrb_int pos = 0;
  mrb_get_args(mrb, "o|i", &str, &pos);
  if (mrb_nil_p(str)) return mrb_false_value();
  mrb_ensure_string_type(mrb, str);
  return mrb_bool_value(regexp_test_at(mrb, self, str, pos));
}

/* Regexp#===(obj) -> true or false; a non-String is false */
static mrb_value
mrb_regexp_eqq(mrb_state *mrb, mrb_value self)
{
  mrb_value obj;
  mrb_get_args(mrb, "o", &obj);
  if (mrb_symbol_p(obj)) obj = mrb_sym_str(mrb, mrb_symbol(obj));
  if (!mrb_string_p(obj)) return mrb_false_value();
  return mrb_bool_value(regexp_test_at(mrb, self, obj, 0));
}

/* Regexp#=~(str) -> Integer or nil */
static mrb_value
mrb_regexp_match_op(mrb_state *mrb, mrb_value self)
{
  mrb_value str;
  mrb_get_args(mrb, "o", &str);
  if (mrb_nil_p(str)) return mrb_nil_value();
  mrb_ensure_string_type(mrb, str);
  return regexp_index_of(mrb, self, str);
}

/* Regexp#source -> String */
static mrb_value
mrb_regexp_source(mrb_state *mrb, mrb_value self)
{
  get_regexp(mrb, self);
  return mrb_iv_get(mrb, self, MRB_IVSYM(source));
}

/* Regexp#to_s -> "(?mix-:source)" */
static mrb_value
mrb_regexp_to_s(mrb_state *mrb, mrb_value self)
{
  picorb_regexp *re = get_regexp(mrb, self);
  char prefix[8];
  int n = picorb_rx_to_s_prefix(re->options, prefix);
  mrb_value source = mrb_iv_get(mrb, self, MRB_IVSYM(source));
  mrb_value result = mrb_str_new(mrb, prefix, n);
  mrb_str_cat_str(mrb, result, source);
  return mrb_str_cat_lit(mrb, result, ")");
}

/* Regexp#inspect -> "/source/mix" */
static mrb_value
mrb_regexp_inspect(mrb_state *mrb, mrb_value self)
{
  picorb_regexp *re = get_regexp(mrb, self);
  char letters[4];
  int n = picorb_rx_letters(re->options, letters);
  mrb_value source = mrb_iv_get(mrb, self, MRB_IVSYM(source));
  mrb_value result = mrb_str_new_lit(mrb, "/");
  mrb_str_cat_str(mrb, result, source);
  mrb_str_cat_lit(mrb, result, "/");
  return mrb_str_cat(mrb, result, letters, (size_t)n);
}

/* Regexp#casefold? -> true or false */
static mrb_value
mrb_regexp_casefold_p(mrb_state *mrb, mrb_value self)
{
  picorb_regexp *re = get_regexp(mrb, self);
  return mrb_bool_value((re->options & PICORB_RX_IGNORECASE) != 0);
}

/* Regexp#options -> Integer */
static mrb_value
mrb_regexp_options(mrb_state *mrb, mrb_value self)
{
  picorb_regexp *re = get_regexp(mrb, self);
  return mrb_int_value(mrb, re->options);
}

/* ---- MatchData ---- */

static mrb_value
md_string(mrb_state *mrb, mrb_value self)
{
  return mrb_iv_get(mrb, self, MRB_IVSYM(string));
}

/* the text of group idx, or nil when the group did not take part */
static mrb_value
md_group(mrb_state *mrb, mrb_value self, picorb_match_data *md, mrb_int idx)
{
  int32_t b = md->caps[2 * idx], e = md->caps[2 * idx + 1];
  if (b < 0) return mrb_nil_value();
  mrb_value str = md_string(mrb, self);
  return mrb_str_new(mrb, RSTRING_PTR(str) + b, e - b);
}

/* the character offset of byte offset boff in the subject */
static mrb_value
md_char_offset(mrb_state *mrb, mrb_value self, int32_t boff)
{
  mrb_value str = md_string(mrb, self);
  long c = picorb_utf8_byte_to_char((const uint8_t *)RSTRING_PTR(str), (size_t)RSTRING_LEN(str), boff);
  return mrb_int_value(mrb, (mrb_int)c);
}

/* group index from an argument: negative counts from the end;
 * out of range is nil for [] and IndexError for begin/end */
static mrb_bool
md_index(picorb_match_data *md, mrb_int *idx)
{
  mrb_int n = md->nsave / 2;
  if (*idx < 0) *idx += n;
  return 0 <= *idx && *idx < n;
}

/* MatchData#[](idx) -> String or nil */
static mrb_value
mrb_match_data_aref(mrb_state *mrb, mrb_value self)
{
  mrb_int idx;
  mrb_get_args(mrb, "i", &idx);
  picorb_match_data *md = get_match_data(mrb, self);
  if (!md_index(md, &idx)) return mrb_nil_value();
  return md_group(mrb, self, md, idx);
}

/* groups from..nsave/2-1 as an Array */
static mrb_value
md_groups(mrb_state *mrb, mrb_value self, mrb_int from)
{
  picorb_match_data *md = get_match_data(mrb, self);
  mrb_int n = md->nsave / 2;
  mrb_value ary = mrb_ary_new_capa(mrb, n - from);
  for (mrb_int i = from; i < n; i++) {
    mrb_ary_push(mrb, ary, md_group(mrb, self, md, i));
  }
  return ary;
}

/* MatchData#to_a -> Array */
static mrb_value
mrb_match_data_to_a(mrb_state *mrb, mrb_value self)
{
  return md_groups(mrb, self, 0);
}

/* MatchData#captures -> Array of the groups after 0 */
static mrb_value
mrb_match_data_captures(mrb_state *mrb, mrb_value self)
{
  return md_groups(mrb, self, 1);
}

/* MatchData#length, #size -> Integer */
static mrb_value
mrb_match_data_length(mrb_state *mrb, mrb_value self)
{
  picorb_match_data *md = get_match_data(mrb, self);
  return mrb_int_value(mrb, md->nsave / 2);
}

/* MatchData#string -> the frozen subject */
static mrb_value
mrb_match_data_string(mrb_state *mrb, mrb_value self)
{
  get_match_data(mrb, self);
  return md_string(mrb, self);
}

/* MatchData#regexp -> Regexp */
static mrb_value
mrb_match_data_regexp(mrb_state *mrb, mrb_value self)
{
  get_match_data(mrb, self);
  return mrb_iv_get(mrb, self, MRB_IVSYM(regexp));
}

/* MatchData#pre_match -> String before the match */
static mrb_value
mrb_match_data_pre_match(mrb_state *mrb, mrb_value self)
{
  picorb_match_data *md = get_match_data(mrb, self);
  mrb_value str = md_string(mrb, self);
  return mrb_str_new(mrb, RSTRING_PTR(str), md->caps[0]);
}

/* MatchData#post_match -> String after the match */
static mrb_value
mrb_match_data_post_match(mrb_state *mrb, mrb_value self)
{
  picorb_match_data *md = get_match_data(mrb, self);
  mrb_value str = md_string(mrb, self);
  int32_t e = md->caps[1];
  return mrb_str_new(mrb, RSTRING_PTR(str) + e, RSTRING_LEN(str) - e);
}

/* MatchData#begin(idx) -> character offset or nil */
static mrb_value
mrb_match_data_begin(mrb_state *mrb, mrb_value self)
{
  mrb_int idx;
  mrb_get_args(mrb, "i", &idx);
  picorb_match_data *md = get_match_data(mrb, self);
  if (!md_index(md, &idx)) mrb_raisef(mrb, E_INDEX_ERROR, "index %i out of matches", idx);
  int32_t b = md->caps[2 * idx];
  if (b < 0) return mrb_nil_value();
  return md_char_offset(mrb, self, b);
}

/* MatchData#end(idx) -> character offset or nil */
static mrb_value
mrb_match_data_end(mrb_state *mrb, mrb_value self)
{
  mrb_int idx;
  mrb_get_args(mrb, "i", &idx);
  picorb_match_data *md = get_match_data(mrb, self);
  if (!md_index(md, &idx)) mrb_raisef(mrb, E_INDEX_ERROR, "index %i out of matches", idx);
  int32_t e = md->caps[2 * idx + 1];
  if (e < 0) return mrb_nil_value();
  return md_char_offset(mrb, self, e);
}

/* MatchData#to_s -> the whole match */
static mrb_value
mrb_match_data_to_s(mrb_state *mrb, mrb_value self)
{
  picorb_match_data *md = get_match_data(mrb, self);
  return md_group(mrb, self, md, 0);
}

/* MatchData#inspect -> #<MatchData "ab" 1:"a" 2:nil> */
static mrb_value
mrb_match_data_inspect(mrb_state *mrb, mrb_value self)
{
  picorb_match_data *md = get_match_data(mrb, self);
  mrb_value result = mrb_str_new_lit(mrb, "#<MatchData ");
  mrb_str_cat_str(mrb, result, mrb_inspect(mrb, md_group(mrb, self, md, 0)));
  mrb_int n = md->nsave / 2;
  for (mrb_int i = 1; i < n; i++) {
    char buf[16];
    int len = snprintf(buf, sizeof(buf), " %d:", (int)i);
    mrb_str_cat(mrb, result, buf, (size_t)len);
    mrb_str_cat_str(mrb, result, mrb_inspect(mrb, md_group(mrb, self, md, i)));
  }
  return mrb_str_cat_lit(mrb, result, ">");
}

/* ---- String ---- */

/* the Regexp an argument stands for: a String is compiled */
static mrb_value
ensure_regexp(mrb_state *mrb, mrb_value pattern)
{
  if (regexp_p(mrb, pattern)) return pattern;
  if (mrb_string_p(pattern)) {
    return regexp_new(mrb, RSTRING_PTR(pattern), RSTRING_LEN(pattern), 0);
  }
  mrb_raisef(mrb, E_TYPE_ERROR, "wrong argument type %C (expected Regexp)", mrb_obj_class(mrb, pattern));
  return mrb_nil_value();
}

/* String#match(pattern, pos = 0) -> MatchData or nil */
static mrb_value
mrb_string_match(mrb_state *mrb, mrb_value self)
{
  mrb_value pattern;
  mrb_int pos = 0;
  mrb_get_args(mrb, "o|i", &pattern, &pos);
  return regexp_match_at(mrb, ensure_regexp(mrb, pattern), self, pos);
}

/* String#match?(pattern, pos = 0) -> true or false */
static mrb_value
mrb_string_match_p(mrb_state *mrb, mrb_value self)
{
  mrb_value pattern;
  mrb_int pos = 0;
  mrb_get_args(mrb, "o|i", &pattern, &pos);
  return mrb_bool_value(regexp_test_at(mrb, ensure_regexp(mrb, pattern), self, pos));
}

/* String#=~(pattern) -> Integer or nil */
static mrb_value
mrb_string_match_op(mrb_state *mrb, mrb_value self)
{
  mrb_value pattern;
  mrb_get_args(mrb, "o", &pattern);
  return regexp_index_of(mrb, ensure_regexp(mrb, pattern), self);
}

/* ---- init ---- */

void
mrb_picoruby_regexp_gem_init(mrb_state *mrb)
{
  class_Regexp = mrb_define_class_id(mrb, MRB_SYM(Regexp), mrb->object_class);
  MRB_SET_INSTANCE_TT(class_Regexp, MRB_TT_DATA);
  mrb_undef_class_method_id(mrb, class_Regexp, MRB_SYM(allocate));

  mrb_define_class_method_id(mrb, class_Regexp, MRB_SYM(compile), mrb_regexp_compile, MRB_ARGS_ARG(1, 2));
  mrb_define_class_method_id(mrb, class_Regexp, MRB_SYM(new), mrb_regexp_compile, MRB_ARGS_ARG(1, 2));

  mrb_define_method_id(mrb, class_Regexp, MRB_SYM(match), mrb_regexp_match, MRB_ARGS_ARG(1, 1));
  mrb_define_method_id(mrb, class_Regexp, MRB_SYM_Q(match), mrb_regexp_match_p, MRB_ARGS_ARG(1, 1));
  mrb_define_method_id(mrb, class_Regexp, mrb_intern_lit(mrb, "==="), mrb_regexp_eqq, MRB_ARGS_REQ(1));
  mrb_define_method_id(mrb, class_Regexp, mrb_intern_lit(mrb, "=~"), mrb_regexp_match_op, MRB_ARGS_REQ(1));
  mrb_define_method_id(mrb, class_Regexp, MRB_SYM(source), mrb_regexp_source, MRB_ARGS_NONE());
  mrb_define_method_id(mrb, class_Regexp, MRB_SYM(to_s), mrb_regexp_to_s, MRB_ARGS_NONE());
  mrb_define_method_id(mrb, class_Regexp, MRB_SYM(inspect), mrb_regexp_inspect, MRB_ARGS_NONE());
  mrb_define_method_id(mrb, class_Regexp, MRB_SYM_Q(casefold), mrb_regexp_casefold_p, MRB_ARGS_NONE());
  mrb_define_method_id(mrb, class_Regexp, MRB_SYM(options), mrb_regexp_options, MRB_ARGS_NONE());

  class_MatchData = mrb_define_class_id(mrb, MRB_SYM(MatchData), mrb->object_class);
  MRB_SET_INSTANCE_TT(class_MatchData, MRB_TT_DATA);
  mrb_undef_class_method_id(mrb, class_MatchData, MRB_SYM(new));

  mrb_define_method_id(mrb, class_MatchData, MRB_OPSYM(aref), mrb_match_data_aref, MRB_ARGS_REQ(1));
  mrb_define_method_id(mrb, class_MatchData, MRB_SYM(to_a), mrb_match_data_to_a, MRB_ARGS_NONE());
  mrb_define_method_id(mrb, class_MatchData, MRB_SYM(captures), mrb_match_data_captures, MRB_ARGS_NONE());
  mrb_define_method_id(mrb, class_MatchData, MRB_SYM(length), mrb_match_data_length, MRB_ARGS_NONE());
  mrb_define_method_id(mrb, class_MatchData, MRB_SYM(size), mrb_match_data_length, MRB_ARGS_NONE());
  mrb_define_method_id(mrb, class_MatchData, MRB_SYM(string), mrb_match_data_string, MRB_ARGS_NONE());
  mrb_define_method_id(mrb, class_MatchData, MRB_SYM(regexp), mrb_match_data_regexp, MRB_ARGS_NONE());
  mrb_define_method_id(mrb, class_MatchData, MRB_SYM(pre_match), mrb_match_data_pre_match, MRB_ARGS_NONE());
  mrb_define_method_id(mrb, class_MatchData, MRB_SYM(post_match), mrb_match_data_post_match, MRB_ARGS_NONE());
  mrb_define_method_id(mrb, class_MatchData, MRB_SYM(begin), mrb_match_data_begin, MRB_ARGS_REQ(1));
  mrb_define_method_id(mrb, class_MatchData, MRB_SYM(end), mrb_match_data_end, MRB_ARGS_REQ(1));
  mrb_define_method_id(mrb, class_MatchData, MRB_SYM(to_s), mrb_match_data_to_s, MRB_ARGS_NONE());
  mrb_define_method_id(mrb, class_MatchData, MRB_SYM(inspect), mrb_match_data_inspect, MRB_ARGS_NONE());

  struct RClass *string_class = mrb->string_class;
  mrb_define_method_id(mrb, string_class, MRB_SYM(match), mrb_string_match, MRB_ARGS_ARG(1, 1));
  mrb_define_method_id(mrb, string_class, MRB_SYM_Q(match), mrb_string_match_p, MRB_ARGS_ARG(1, 1));
  mrb_define_method_id(mrb, string_class, mrb_intern_lit(mrb, "=~"), mrb_string_match_op, MRB_ARGS_REQ(1));
}

void
mrb_picoruby_regexp_gem_final(mrb_state *mrb)
{
}
