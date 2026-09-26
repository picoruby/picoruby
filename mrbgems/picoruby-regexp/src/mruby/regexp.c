#include "mruby.h"
#include "mruby/presym.h"
#include "mruby/class.h"
#include "mruby/data.h"
#include "mruby/string.h"
#include "mruby/variable.h"
#include "mruby/array.h"
#include "mruby/proc.h"

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

/* $~: one global, not per frame as in CRuby. The compiler reads $&,
 * $1, $` and $' through it (MatchData#__group and friends below). */
static void
set_last_match(mrb_state *mrb, mrb_value md)
{
  mrb_gv_set(mrb, mrb_intern_lit(mrb, "$~"), md);
}

/* the whole match: MatchData or nil, and $~ follows */
static mrb_value
regexp_match_at(mrb_state *mrb, mrb_value re_obj, mrb_value str, mrb_int cpos)
{
  picorb_regexp *re = get_regexp(mrb, re_obj);
  int32_t caps[PICORB_RX_MAX_NSAVE];
  mrb_value md = mrb_nil_value();
  if (regexp_run(re, str, cpos, caps, FALSE)) {
    md = match_data_new(mrb, re_obj, str, caps, (int)re->prog[1]);
  }
  set_last_match(mrb, md);
  return md;
}

/* yes or no, without capture positions */
static mrb_bool
regexp_test_at(mrb_state *mrb, mrb_value re_obj, mrb_value str, mrb_int cpos)
{
  picorb_regexp *re = get_regexp(mrb, re_obj);
  int32_t caps[PICORB_RX_MAX_NSAVE];
  return regexp_run(re, str, cpos, caps, TRUE);
}

/* the character offset where the match starts, or nil; $~ follows */
static mrb_value
regexp_index_of(mrb_state *mrb, mrb_value re_obj, mrb_value str)
{
  mrb_value md = regexp_match_at(mrb, re_obj, str, 0);
  if (mrb_nil_p(md)) return md;
  picorb_match_data *m = get_match_data(mrb, md);
  long c = picorb_utf8_byte_to_char((const uint8_t *)RSTRING_PTR(str), (size_t)RSTRING_LEN(str), m->caps[0]);
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
  return mrb_bool_value(!mrb_nil_p(regexp_match_at(mrb, self, obj, 0)));
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

/* ---- sub, scan, split: the C side of mrblib/regexp.rb ---- */

/* Run re over str from byte offset bpos. */
static mrb_bool
regexp_run_bytes(picorb_regexp *re, mrb_value str, mrb_int bpos, int32_t *caps, mrb_bool first_only)
{
  size_t len = (size_t)RSTRING_LEN(str);
  if (bpos < 0 || (size_t)bpos > len) return FALSE;
  return regex_exec(re->prog, (const uint8_t *)RSTRING_PTR(str), len, (size_t)bpos, caps, first_only, re->scratch);
}

/* Regexp#__bmatch(str, byte_pos) -> MatchData or nil */
static mrb_value
mrb_regexp_bmatch(mrb_state *mrb, mrb_value self)
{
  mrb_value str;
  mrb_int bpos;
  mrb_get_args(mrb, "Si", &str, &bpos);
  picorb_regexp *re = get_regexp(mrb, self);
  int32_t caps[PICORB_RX_MAX_NSAVE];
  if (!regexp_run_bytes(re, str, bpos, caps, FALSE)) return mrb_nil_value();
  return match_data_new(mrb, self, str, caps, (int)re->prog[1]);
}

/* MatchData#byteoffset(idx) -> [begin, end] in bytes, or nil */
static mrb_value
mrb_match_data_byteoffset(mrb_state *mrb, mrb_value self)
{
  mrb_int idx;
  mrb_get_args(mrb, "i", &idx);
  picorb_match_data *md = get_match_data(mrb, self);
  if (!md_index(md, &idx)) mrb_raisef(mrb, E_INDEX_ERROR, "index %i out of matches", idx);
  int32_t b = md->caps[2 * idx];
  if (b < 0) return mrb_nil_value();
  mrb_value pair[2] = { mrb_int_value(mrb, b), mrb_int_value(mrb, md->caps[2 * idx + 1]) };
  return mrb_ary_new_from_values(mrb, 2, pair);
}

typedef struct {
  mrb_state *mrb;
  mrb_value out;      /* the String or Array being built */
  mrb_value subject;  /* the String scanned */
} build_ctx;

static void
emit_to_str(void *p, const uint8_t *s, size_t n)
{
  build_ctx *c = (build_ctx *)p;
  mrb_str_cat(c->mrb, c->out, (const char *)s, n);
}

/* Regexp.escape(str) -> String */
static mrb_value
mrb_regexp_escape(mrb_state *mrb, mrb_value self)
{
  mrb_value str;
  mrb_get_args(mrb, "S", &str);
  build_ctx c = { mrb, mrb_str_new_capa(mrb, RSTRING_LEN(str)), str };
  picorb_rx_escape((const uint8_t *)RSTRING_PTR(str), (size_t)RSTRING_LEN(str), emit_to_str, &c);
  return c.out;
}

/* String#__sub(re, replacement, global) -> String, or nil without a match */
static mrb_value
mrb_string_sub_c(mrb_state *mrb, mrb_value self)
{
  mrb_value re_obj, repl;
  mrb_bool global;
  mrb_get_args(mrb, "oSb", &re_obj, &repl, &global);
  picorb_regexp *re = get_regexp(mrb, re_obj);
  build_ctx c = { mrb, mrb_str_new_capa(mrb, RSTRING_LEN(self)), self };
  int32_t last[PICORB_RX_MAX_NSAVE];
  int n = picorb_rx_sub(re->prog, re->scratch, (const uint8_t *)RSTRING_PTR(self), (size_t)RSTRING_LEN(self),
                        (const uint8_t *)RSTRING_PTR(repl), (size_t)RSTRING_LEN(repl), global, last,
                        emit_to_str, &c);
  set_last_match(mrb, n ? match_data_new(mrb, re_obj, self, last, (int)re->prog[1]) : mrb_nil_value());
  return n ? c.out : mrb_nil_value();
}

/* one scan match: the text, or the Array of groups when there are any */
static void
scan_match(void *p, const int32_t *caps, int nsave)
{
  build_ctx *c = (build_ctx *)p;
  const char *s = RSTRING_PTR(c->subject);
  if (nsave <= 2) {
    mrb_ary_push(c->mrb, c->out, mrb_str_new(c->mrb, s + caps[0], caps[1] - caps[0]));
    return;
  }
  mrb_value groups = mrb_ary_new_capa(c->mrb, nsave / 2 - 1);
  for (int g = 1; 2 * g + 1 < nsave; g++) {
    int32_t b = caps[2 * g], e = caps[2 * g + 1];
    mrb_ary_push(c->mrb, groups, b < 0 ? mrb_nil_value() : mrb_str_new(c->mrb, s + b, e - b));
  }
  mrb_ary_push(c->mrb, c->out, groups);
}

/* String#__scan(re) -> Array */
static mrb_value
mrb_string_scan_c(mrb_state *mrb, mrb_value self)
{
  mrb_value re_obj;
  mrb_get_args(mrb, "o", &re_obj);
  picorb_regexp *re = get_regexp(mrb, re_obj);
  build_ctx c = { mrb, mrb_ary_new(mrb), self };
  int32_t last[PICORB_RX_MAX_NSAVE];
  int n = picorb_rx_scan(re->prog, re->scratch, (const uint8_t *)RSTRING_PTR(self), (size_t)RSTRING_LEN(self), last, scan_match, &c);
  set_last_match(mrb, n ? match_data_new(mrb, re_obj, self, last, (int)re->prog[1]) : mrb_nil_value());
  return c.out;
}

static void
split_piece(void *p, size_t b, size_t e)
{
  build_ctx *c = (build_ctx *)p;
  mrb_ary_push(c->mrb, c->out, mrb_str_new(c->mrb, RSTRING_PTR(c->subject) + b, (mrb_int)(e - b)));
}

/* String#__split(re, limit) -> Array */
static mrb_value
mrb_string_split_c(mrb_state *mrb, mrb_value self)
{
  mrb_value re_obj;
  mrb_int limit;
  mrb_get_args(mrb, "oi", &re_obj, &limit);
  picorb_regexp *re = get_regexp(mrb, re_obj);
  build_ctx c = { mrb, mrb_ary_new(mrb), self };
  picorb_rx_split(re->prog, re->scratch, (const uint8_t *)RSTRING_PTR(self), (size_t)RSTRING_LEN(self), (long)limit, split_piece, &c);
  if (limit == 0) {
    /* trailing empty pieces go, as in CRuby */
    while (RARRAY_LEN(c.out) > 0 && RSTRING_LEN(RARRAY_PTR(c.out)[RARRAY_LEN(c.out) - 1]) == 0) {
      mrb_ary_pop(mrb, c.out);
    }
  }
  return c.out;
}

/* ---- the readings of $~ the compiler emits: $& $1 $` $' $+ ---- */

/* MatchData#__group(n) -> String or nil */
static mrb_value
mrb_match_data_group_ref(mrb_state *mrb, mrb_value self)
{
  mrb_int idx;
  mrb_get_args(mrb, "i", &idx);
  picorb_match_data *md = get_match_data(mrb, self);
  if (!md_index(md, &idx)) return mrb_nil_value();
  return md_group(mrb, self, md, idx);
}

/* MatchData#__last_group -> the highest group that took part, or nil */
static mrb_value
mrb_match_data_last_group(mrb_state *mrb, mrb_value self)
{
  picorb_match_data *md = get_match_data(mrb, self);
  for (mrb_int i = md->nsave / 2 - 1; i >= 1; i--) {
    if (md->caps[2 * i] >= 0) return md_group(mrb, self, md, i);
  }
  return mrb_nil_value();
}

/* ---- index, rindex, [], slice, partition, rpartition ---- */

/* The VM's own method, kept under another name at init, takes every
 * pattern that is not a Regexp. */
static mrb_value
fallback(mrb_state *mrb, mrb_value self, mrb_sym name)
{
  const mrb_value *argv;
  mrb_int argc;
  mrb_value blk;
  mrb_get_args(mrb, "*&", &argv, &argc, &blk);
  return mrb_funcall_with_block(mrb, self, name, argc, argv, blk);
}

/* true and *re when the first argument is a Regexp */
static mrb_bool
regexp_first(mrb_state *mrb, mrb_value *re)
{
  const mrb_value *argv;
  mrb_int argc;
  mrb_get_args(mrb, "*", &argv, &argc);
  if (argc < 1 || !regexp_p(mrb, argv[0])) return FALSE;
  *re = argv[0];
  return TRUE;
}

/* the character offset of byte offset boff in str */
static mrb_value
char_offset(mrb_state *mrb, mrb_value str, int32_t boff)
{
  long c = picorb_utf8_byte_to_char((const uint8_t *)RSTRING_PTR(str), (size_t)RSTRING_LEN(str), boff);
  return mrb_int_value(mrb, (mrb_int)c);
}

/* String#index(re, pos = 0) -> character offset or nil */
static mrb_value
mrb_string_index(mrb_state *mrb, mrb_value self)
{
  mrb_value re;
  if (!regexp_first(mrb, &re)) return fallback(mrb, self, MRB_SYM(__index_str));
  mrb_value pat;
  mrb_int pos = 0;
  mrb_get_args(mrb, "o|i", &pat, &pos);
  mrb_value md = regexp_match_at(mrb, re, self, pos);
  if (mrb_nil_p(md)) return md;
  return char_offset(mrb, self, get_match_data(mrb, md)->caps[0]);
}

/* the last match starting at character offset cpos or before: MatchData or nil; $~ follows */
static mrb_value
regexp_rmatch_at(mrb_state *mrb, mrb_value re_obj, mrb_value str, mrb_int cpos)
{
  picorb_regexp *re = get_regexp(mrb, re_obj);
  const uint8_t *s = (const uint8_t *)RSTRING_PTR(str);
  size_t len = (size_t)RSTRING_LEN(str);
  long chars = picorb_utf8_length(s, len);
  if (cpos < 0) cpos += chars;
  mrb_value md = mrb_nil_value();
  if (cpos >= 0) {
    if (cpos > chars) cpos = chars;
    long b = picorb_utf8_char_to_byte(s, len, (long)cpos);
    int32_t caps[PICORB_RX_MAX_NSAVE];
    if (picorb_rx_rindex(re->prog, re->scratch, s, len, (size_t)b, caps)) {
      md = match_data_new(mrb, re_obj, str, caps, (int)re->prog[1]);
    }
  }
  set_last_match(mrb, md);
  return md;
}

/* String#rindex(re, pos = length) -> character offset or nil */
static mrb_value
mrb_string_rindex(mrb_state *mrb, mrb_value self)
{
  mrb_value re;
  if (!regexp_first(mrb, &re)) return fallback(mrb, self, MRB_SYM(__rindex_str));
  mrb_value pat;
  mrb_int pos = RSTRING_LEN(self);
  mrb_get_args(mrb, "o|i", &pat, &pos);
  mrb_value md = regexp_rmatch_at(mrb, re, self, pos);
  if (mrb_nil_p(md)) return md;
  return char_offset(mrb, self, get_match_data(mrb, md)->caps[0]);
}

/* String#[](re, capture = 0), #slice -> String or nil */
static mrb_value
aref_common(mrb_state *mrb, mrb_value self, mrb_sym fallback_name)
{
  mrb_value re;
  if (!regexp_first(mrb, &re)) return fallback(mrb, self, fallback_name);
  mrb_value pat;
  mrb_value cap = mrb_nil_value();
  mrb_get_args(mrb, "o|o", &pat, &cap);
  mrb_int idx = 0;
  if (!mrb_nil_p(cap)) {
    if (!mrb_integer_p(cap)) mrb_raise(mrb, E_INDEX_ERROR, "named groups are not supported");
    idx = mrb_integer(cap);
  }
  mrb_value md = regexp_match_at(mrb, re, self, 0);
  if (mrb_nil_p(md)) return md;
  picorb_match_data *m = get_match_data(mrb, md);
  if (!md_index(m, &idx)) return mrb_nil_value();
  return md_group(mrb, md, m, idx);
}

static mrb_value
mrb_string_aref(mrb_state *mrb, mrb_value self)
{
  return aref_common(mrb, self, MRB_SYM(__aref_str));
}

static mrb_value
mrb_string_slice(mrb_state *mrb, mrb_value self)
{
  return aref_common(mrb, self, MRB_SYM(__slice_str));
}

/* [before, match, after] for the match at bytes b..e, or the no-match
 * triple [str, "", ""] (["", "", str] from the right) */
static mrb_value
partition_result(mrb_state *mrb, mrb_value str, mrb_int b, mrb_int e, mrb_bool from_right)
{
  mrb_value parts[3];
  if (b < 0) {
    parts[0] = from_right ? mrb_str_new_lit(mrb, "") : mrb_str_dup(mrb, str);
    parts[1] = mrb_str_new_lit(mrb, "");
    parts[2] = from_right ? mrb_str_dup(mrb, str) : mrb_str_new_lit(mrb, "");
  } else {
    parts[0] = mrb_str_new(mrb, RSTRING_PTR(str), b);
    parts[1] = mrb_str_new(mrb, RSTRING_PTR(str) + b, e - b);
    parts[2] = mrb_str_new(mrb, RSTRING_PTR(str) + e, RSTRING_LEN(str) - e);
  }
  return mrb_ary_new_from_values(mrb, 3, parts);
}

/* the last occurrence of needle in str as a byte offset, or -1 */
static mrb_int
rindex_bytes(mrb_value str, mrb_value needle)
{
  mrb_int nlen = RSTRING_LEN(needle);
  for (mrb_int b = RSTRING_LEN(str) - nlen; b >= 0; b--) {
    if (memcmp(RSTRING_PTR(str) + b, RSTRING_PTR(needle), (size_t)nlen) == 0) return b;
  }
  return -1;
}

/* String#partition(pattern), #rpartition(pattern), for a Regexp or a
 * String. Both are implemented here in full: mruby has them only with
 * mruby-string-ext, which a build may leave out. */
static mrb_value
partition_common(mrb_state *mrb, mrb_value self, mrb_bool from_right)
{
  mrb_value pat;
  mrb_get_args(mrb, "o", &pat);
  mrb_int b = -1, e = -1;
  if (regexp_p(mrb, pat)) {
    mrb_value md = from_right ? regexp_rmatch_at(mrb, pat, self, RSTRING_LEN(self))
                              : regexp_match_at(mrb, pat, self, 0);
    if (!mrb_nil_p(md)) {
      picorb_match_data *m = get_match_data(mrb, md);
      b = m->caps[0];
      e = m->caps[1];
    }
  } else {
    mrb_ensure_string_type(mrb, pat);
    b = from_right ? rindex_bytes(self, pat)
                   : mrb_str_index(mrb, self, RSTRING_PTR(pat), RSTRING_LEN(pat), 0);
    if (b >= 0) e = b + RSTRING_LEN(pat);
  }
  return partition_result(mrb, self, b, e, from_right);
}

static mrb_value
mrb_string_partition(mrb_state *mrb, mrb_value self)
{
  return partition_common(mrb, self, FALSE);
}

static mrb_value
mrb_string_rpartition(mrb_state *mrb, mrb_value self)
{
  return partition_common(mrb, self, TRUE);
}

/* keep the VM's method under alias_name when it exists */
static void
keep_original(mrb_state *mrb, struct RClass *c, mrb_sym alias_name, mrb_sym name)
{
  struct RClass *found = c;
  mrb_method_t m = mrb_method_search_vm(mrb, &found, name);
  if (MRB_METHOD_UNDEF_P(m)) return;
  mrb_alias_method(mrb, c, alias_name, name);
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

  /* the C side of mrblib/regexp.rb */
  mrb_define_class_method_id(mrb, class_Regexp, MRB_SYM(escape), mrb_regexp_escape, MRB_ARGS_REQ(1));
  mrb_define_method_id(mrb, class_Regexp, MRB_SYM(__bmatch), mrb_regexp_bmatch, MRB_ARGS_REQ(2));
  mrb_define_method_id(mrb, class_MatchData, MRB_SYM(byteoffset), mrb_match_data_byteoffset, MRB_ARGS_REQ(1));
  mrb_define_method_id(mrb, string_class, MRB_SYM(__sub), mrb_string_sub_c, MRB_ARGS_REQ(3));
  mrb_define_method_id(mrb, string_class, MRB_SYM(__scan), mrb_string_scan_c, MRB_ARGS_REQ(1));
  mrb_define_method_id(mrb, string_class, MRB_SYM(__split), mrb_string_split_c, MRB_ARGS_REQ(2));

  /* $& $1 $` $' $+ read $~ through these */
  mrb_define_method_id(mrb, class_MatchData, MRB_SYM(__group), mrb_match_data_group_ref, MRB_ARGS_REQ(1));
  mrb_define_method_id(mrb, class_MatchData, MRB_SYM(__pre_match), mrb_match_data_pre_match, MRB_ARGS_NONE());
  mrb_define_method_id(mrb, class_MatchData, MRB_SYM(__post_match), mrb_match_data_post_match, MRB_ARGS_NONE());
  mrb_define_method_id(mrb, class_MatchData, MRB_SYM(__last_group), mrb_match_data_last_group, MRB_ARGS_NONE());

  /* index, rindex, [], slice, partition and rpartition take a Regexp;
   * the VM's own versions stay behind for every other pattern */
  keep_original(mrb, string_class, MRB_SYM(__index_str), MRB_SYM(index));
  keep_original(mrb, string_class, MRB_SYM(__rindex_str), MRB_SYM(rindex));
  keep_original(mrb, string_class, MRB_SYM(__aref_str), MRB_OPSYM(aref));
  keep_original(mrb, string_class, MRB_SYM(__slice_str), MRB_SYM(slice));
  mrb_define_method_id(mrb, string_class, MRB_SYM(index), mrb_string_index, MRB_ARGS_ARG(1, 1));
  mrb_define_method_id(mrb, string_class, MRB_SYM(rindex), mrb_string_rindex, MRB_ARGS_ARG(1, 1));
  mrb_define_method_id(mrb, string_class, MRB_OPSYM(aref), mrb_string_aref, MRB_ARGS_ARG(1, 1));
  mrb_define_method_id(mrb, string_class, MRB_SYM(slice), mrb_string_slice, MRB_ARGS_ARG(1, 1));
  mrb_define_method_id(mrb, string_class, MRB_SYM(partition), mrb_string_partition, MRB_ARGS_REQ(1));
  mrb_define_method_id(mrb, string_class, MRB_SYM(rpartition), mrb_string_rpartition, MRB_ARGS_REQ(1));
}

void
mrb_picoruby_regexp_gem_final(mrb_state *mrb)
{
}
