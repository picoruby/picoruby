#include <mrubyc.h>

#include <string.h>
#include <stdio.h>

/*
 * Regexp: instance->data holds picorb_regexp: the compiled program,
 * a scratch block for the VM sized once at compile time so a match
 * allocates nothing, the option bits and the pattern text (a String,
 * ref-counted). The destructor frees the program and the scratch and
 * releases the text.
 *
 * MatchData: instance->data holds picorb_match_data, a variable-size
 * struct with the frozen subject and the Regexp (both ref-counted)
 * and the capture positions as byte offsets.
 */

typedef struct {
  uint32_t *prog;
  uint32_t *scratch;
  int options;
  mrbc_value source;
} picorb_regexp;

typedef struct {
  int nsave;
  mrbc_value str;
  mrbc_value regexp;
  int32_t caps[];
} picorb_match_data;

static mrbc_class *class_Regexp;
static mrbc_class *class_MatchData;
static mrbc_class *class_RegexpError;

/* The engine's compiler allocates through these names; mruby/c's
 * allocator has no VM argument. The VM allocates nothing. */
void *
regex_malloc(size_t n)
{
  return mrbc_raw_alloc((unsigned int)n);
}

void *
regex_realloc(void *p, size_t n)
{
  return mrbc_raw_realloc(p, (unsigned int)n);
}

void
regex_free(void *p)
{
  mrbc_raw_free(p);
}

static void
regexp_destructor(mrbc_value *self)
{
  picorb_regexp *re = (picorb_regexp *)self->instance->data;
  mrbc_raw_free(re->prog);
  mrbc_raw_free(re->scratch);
  re->prog = NULL;
  re->scratch = NULL;
  mrbc_decref(&re->source);
}

static void
match_data_destructor(mrbc_value *self)
{
  picorb_match_data *md = (picorb_match_data *)self->instance->data;
  mrbc_decref(&md->str);
  mrbc_decref(&md->regexp);
}

static int
regexp_p(const mrbc_value *v)
{
  return v->tt == MRBC_TT_OBJECT && v->instance->cls == class_Regexp;
}

static picorb_regexp *
get_regexp(const mrbc_value *v)
{
  return (picorb_regexp *)v->instance->data;
}

static picorb_match_data *
get_match_data(const mrbc_value *v)
{
  return (picorb_match_data *)v->instance->data;
}

/* ---- Regexp ---- */

/* A new Regexp, or nil after raising */
static mrbc_value
regexp_new(mrbc_vm *vm, const char *src, int slen, int options)
{
  size_t nwords;
  const char *err;
  size_t err_at;

  uint32_t *prog = regex_compile_words((const uint8_t *)src, (size_t)slen,
                                          picorb_rx_engine_flags(options),
                                          &nwords, &err, &err_at);
  if (!prog) {
    mrbc_raisef(vm, class_RegexpError, "%s at byte %d: /%s/", err, (int)err_at, src);
    return mrbc_nil_value();
  }
  uint32_t *scratch = mrbc_raw_alloc((unsigned int)(sizeof(uint32_t) * regex_scratch_words(prog)));
  if (!scratch) {
    mrbc_raw_free(prog);
    mrbc_raise(vm, MRBC_CLASS(NoMemoryError), "regexp scratch");
    return mrbc_nil_value();
  }
  mrbc_value obj = mrbc_instance_new(vm, class_Regexp, sizeof(picorb_regexp));
  if (obj.tt != MRBC_TT_OBJECT) {
    mrbc_raw_free(prog);
    mrbc_raw_free(scratch);
    mrbc_raise(vm, MRBC_CLASS(NoMemoryError), "regexp object");
    return mrbc_nil_value();
  }
  picorb_regexp *re = get_regexp(&obj);
  re->prog = prog;
  re->scratch = scratch;
  re->options = options;
  re->source = mrbc_string_new(vm, src, slen);
  return obj;
}

/* the options an argument of Regexp.compile stands for; -1 after raising */
static int
options_from_arg(mrbc_vm *vm, const mrbc_value *flags)
{
  switch (flags->tt) {
  case MRBC_TT_NIL:
  case MRBC_TT_FALSE:
    return 0;
  case MRBC_TT_STRING: {
    int options = picorb_rx_options_from_letters(mrbc_string_cstr(flags), (size_t)mrbc_string_size(flags));
    if (options < 0) {
      mrbc_raise(vm, MRBC_CLASS(ArgumentError), "unknown regexp option");
      return -1;
    }
    return options;
  }
  case MRBC_TT_INTEGER:
    return (int)(flags->i & (PICORB_RX_IGNORECASE | PICORB_RX_EXTENDED | PICORB_RX_MULTILINE));
  default:
    /* any other truthy value means IGNORECASE, as in CRuby */
    return PICORB_RX_IGNORECASE;
  }
}

/* Regexp.compile(pattern, options = nil, encoding = nil), Regexp.new */
static void
c_regexp_compile(mrbc_vm *vm, mrbc_value v[], int argc)
{
  if (argc < 1) {
    mrbc_raise(vm, MRBC_CLASS(ArgumentError), "wrong number of arguments");
    return;
  }
  if (regexp_p(&v[1])) {
    picorb_regexp *re = get_regexp(&v[1]);
    mrbc_value obj = regexp_new(vm, mrbc_string_cstr(&re->source), mrbc_string_size(&re->source), re->options);
    SET_RETURN(obj);
    return;
  }
  if (v[1].tt != MRBC_TT_STRING) {
    mrbc_raise(vm, MRBC_CLASS(TypeError), "no implicit conversion into String");
    return;
  }
  int options = 0;
  if (argc >= 2) {
    options = options_from_arg(vm, &v[2]);
    if (options < 0) return;
  }
  mrbc_value obj = regexp_new(vm, mrbc_string_cstr(&v[1]), mrbc_string_size(&v[1]), options);
  SET_RETURN(obj);
}

/* Run re over str from character position cpos. caps gets the byte
 * offsets. 0 when cpos is outside the string or nothing matches. */
static int
regexp_run(picorb_regexp *re, const mrbc_value *str, mrbc_int_t cpos, int32_t *caps, int first_only)
{
  const uint8_t *s = (const uint8_t *)mrbc_string_cstr(str);
  size_t len = (size_t)mrbc_string_size(str);
  if (cpos < 0) {
    cpos += picorb_utf8_length(s, len);
    if (cpos < 0) return 0;
  }
  long b = picorb_utf8_char_to_byte(s, len, (long)cpos);
  if (b < 0) return 0;
  return regex_exec(re->prog, s, len, (size_t)b, caps, first_only != 0, re->scratch);
}

static mrbc_value
match_data_new(mrbc_vm *vm, mrbc_value *re_obj, const mrbc_value *str, const int32_t *caps, int nsave)
{
  int size = (int)(sizeof(picorb_match_data) + sizeof(int32_t) * (size_t)nsave);
  mrbc_value obj = mrbc_instance_new(vm, class_MatchData, size);
  if (obj.tt != MRBC_TT_OBJECT) {
    mrbc_raise(vm, MRBC_CLASS(NoMemoryError), "match data");
    return mrbc_nil_value();
  }
  picorb_match_data *md = get_match_data(&obj);
  md->nsave = nsave;
  md->str = mrbc_string_new(vm, mrbc_string_cstr(str), mrbc_string_size(str));
  md->regexp = *re_obj;
  mrbc_incref(&md->regexp);
  memcpy(md->caps, caps, sizeof(int32_t) * (size_t)nsave);
  return obj;
}

/* the whole match: MatchData or nil */
static mrbc_value
regexp_match_at(mrbc_vm *vm, mrbc_value *re_obj, const mrbc_value *str, mrbc_int_t cpos)
{
  picorb_regexp *re = get_regexp(re_obj);
  int32_t caps[PICORB_RX_MAX_NSAVE];
  if (!regexp_run(re, str, cpos, caps, 0)) return mrbc_nil_value();
  return match_data_new(vm, re_obj, str, caps, (int)re->prog[1]);
}

/* yes or no, without capture positions */
static int
regexp_test_at(const mrbc_value *re_obj, const mrbc_value *str, mrbc_int_t cpos)
{
  int32_t caps[PICORB_RX_MAX_NSAVE];
  return regexp_run(get_regexp(re_obj), str, cpos, caps, 1);
}

/* the character offset where the match starts, or -1 */
static long
regexp_index_of(const mrbc_value *re_obj, const mrbc_value *str)
{
  int32_t caps[PICORB_RX_MAX_NSAVE];
  if (!regexp_run(get_regexp(re_obj), str, 0, caps, 0)) return -1;
  return picorb_utf8_byte_to_char((const uint8_t *)mrbc_string_cstr(str), (size_t)mrbc_string_size(str), caps[0]);
}

/* the pos argument of match and match?: v[2] when given, else 0 */
static int
pos_arg(mrbc_vm *vm, mrbc_value v[], int argc, mrbc_int_t *pos)
{
  *pos = 0;
  if (argc < 2) return 1;
  if (v[2].tt != MRBC_TT_INTEGER) {
    mrbc_raise(vm, MRBC_CLASS(TypeError), "no implicit conversion into Integer");
    return 0;
  }
  *pos = v[2].i;
  return 1;
}

/* Regexp#match(str, pos = 0) -> MatchData or nil */
static void
c_regexp_match(mrbc_vm *vm, mrbc_value v[], int argc)
{
  mrbc_int_t pos;
  if (argc < 1 || v[1].tt == MRBC_TT_NIL) { SET_NIL_RETURN(); return; }
  if (v[1].tt != MRBC_TT_STRING) {
    mrbc_raise(vm, MRBC_CLASS(TypeError), "no implicit conversion into String");
    return;
  }
  if (!pos_arg(vm, v, argc, &pos)) return;
  mrbc_value md = regexp_match_at(vm, &v[0], &v[1], pos);
  SET_RETURN(md);
}

/* Regexp#match?(str, pos = 0) -> true or false */
static void
c_regexp_match_p(mrbc_vm *vm, mrbc_value v[], int argc)
{
  mrbc_int_t pos;
  if (argc < 1 || v[1].tt == MRBC_TT_NIL) { SET_FALSE_RETURN(); return; }
  if (v[1].tt != MRBC_TT_STRING) {
    mrbc_raise(vm, MRBC_CLASS(TypeError), "no implicit conversion into String");
    return;
  }
  if (!pos_arg(vm, v, argc, &pos)) return;
  if (regexp_test_at(&v[0], &v[1], pos)) SET_TRUE_RETURN(); else SET_FALSE_RETURN();
}

/* Regexp#===(obj) -> true or false; a non-String is false */
static void
c_regexp_eqq(mrbc_vm *vm, mrbc_value v[], int argc)
{
  if (argc < 1) { SET_FALSE_RETURN(); return; }
  if (v[1].tt == MRBC_TT_SYMBOL) {
    mrbc_value s = mrbc_string_new_cstr(vm, mrbc_symbol_cstr(&v[1]));
    int hit = regexp_test_at(&v[0], &s, 0);
    mrbc_decref(&s);
    if (hit) SET_TRUE_RETURN(); else SET_FALSE_RETURN();
    return;
  }
  if (v[1].tt != MRBC_TT_STRING) { SET_FALSE_RETURN(); return; }
  if (regexp_test_at(&v[0], &v[1], 0)) SET_TRUE_RETURN(); else SET_FALSE_RETURN();
}

/* Regexp#=~(str) -> Integer or nil */
static void
c_regexp_match_op(mrbc_vm *vm, mrbc_value v[], int argc)
{
  if (argc < 1 || v[1].tt == MRBC_TT_NIL) { SET_NIL_RETURN(); return; }
  if (v[1].tt != MRBC_TT_STRING) {
    mrbc_raise(vm, MRBC_CLASS(TypeError), "no implicit conversion into String");
    return;
  }
  long c = regexp_index_of(&v[0], &v[1]);
  if (c < 0) SET_NIL_RETURN(); else SET_INT_RETURN((mrbc_int_t)c);
}

/* Regexp#source -> String */
static void
c_regexp_source(mrbc_vm *vm, mrbc_value v[], int argc)
{
  picorb_regexp *re = get_regexp(&v[0]);
  mrbc_incref(&re->source);
  SET_RETURN(re->source);
}

/* Regexp#to_s -> "(?mix-:source)" */
static void
c_regexp_to_s(mrbc_vm *vm, mrbc_value v[], int argc)
{
  picorb_regexp *re = get_regexp(&v[0]);
  char prefix[8];
  int n = picorb_rx_to_s_prefix(re->options, prefix);
  mrbc_value result = mrbc_string_new(vm, prefix, n);
  mrbc_string_append(&result, &re->source);
  mrbc_string_append_cstr(&result, ")");
  SET_RETURN(result);
}

/* Regexp#inspect -> "/source/mix" */
static void
c_regexp_inspect(mrbc_vm *vm, mrbc_value v[], int argc)
{
  picorb_regexp *re = get_regexp(&v[0]);
  char letters[4];
  picorb_rx_letters(re->options, letters);
  mrbc_value result = mrbc_string_new_cstr(vm, "/");
  mrbc_string_append(&result, &re->source);
  mrbc_string_append_cstr(&result, "/");
  mrbc_string_append_cstr(&result, letters);
  SET_RETURN(result);
}

/* Regexp#casefold? -> true or false */
static void
c_regexp_casefold_p(mrbc_vm *vm, mrbc_value v[], int argc)
{
  picorb_regexp *re = get_regexp(&v[0]);
  if (re->options & PICORB_RX_IGNORECASE) SET_TRUE_RETURN(); else SET_FALSE_RETURN();
}

/* Regexp#options -> Integer */
static void
c_regexp_options(mrbc_vm *vm, mrbc_value v[], int argc)
{
  picorb_regexp *re = get_regexp(&v[0]);
  SET_INT_RETURN(re->options);
}

/* ---- MatchData ---- */

/* the text of group idx, or nil when the group did not take part */
static mrbc_value
md_group(mrbc_vm *vm, picorb_match_data *md, int idx)
{
  int32_t b = md->caps[2 * idx], e = md->caps[2 * idx + 1];
  if (b < 0) return mrbc_nil_value();
  return mrbc_string_new(vm, mrbc_string_cstr(&md->str) + b, e - b);
}

/* the character offset of byte offset boff in the subject */
static mrbc_int_t
md_char_offset(picorb_match_data *md, int32_t boff)
{
  return (mrbc_int_t)picorb_utf8_byte_to_char((const uint8_t *)mrbc_string_cstr(&md->str),
                                              (size_t)mrbc_string_size(&md->str), boff);
}

/* group index from an argument: negative counts from the end */
static int
md_index(mrbc_vm *vm, picorb_match_data *md, mrbc_value v[], int argc, mrbc_int_t *idx)
{
  if (argc < 1 || v[1].tt != MRBC_TT_INTEGER) {
    mrbc_raise(vm, MRBC_CLASS(TypeError), "no implicit conversion into Integer");
    return 0;
  }
  mrbc_int_t n = md->nsave / 2;
  *idx = v[1].i;
  if (*idx < 0) *idx += n;
  return 0 <= *idx && *idx < n;
}

/* MatchData#[](idx) -> String or nil */
static void
c_match_data_aref(mrbc_vm *vm, mrbc_value v[], int argc)
{
  picorb_match_data *md = get_match_data(&v[0]);
  mrbc_int_t idx;
  if (!md_index(vm, md, v, argc, &idx)) {
    if (!mrbc_israised(vm)) SET_NIL_RETURN();
    return;
  }
  mrbc_value s = md_group(vm, md, (int)idx);
  SET_RETURN(s);
}

/* groups from..nsave/2-1 as an Array */
static mrbc_value
md_groups(mrbc_vm *vm, picorb_match_data *md, int from)
{
  int n = md->nsave / 2;
  mrbc_value ary = mrbc_array_new(vm, n - from);
  for (int i = from; i < n; i++) {
    mrbc_value s = md_group(vm, md, i);
    mrbc_array_push(&ary, &s);
  }
  return ary;
}

/* MatchData#to_a -> Array */
static void
c_match_data_to_a(mrbc_vm *vm, mrbc_value v[], int argc)
{
  mrbc_value ary = md_groups(vm, get_match_data(&v[0]), 0);
  SET_RETURN(ary);
}

/* MatchData#captures -> Array of the groups after 0 */
static void
c_match_data_captures(mrbc_vm *vm, mrbc_value v[], int argc)
{
  mrbc_value ary = md_groups(vm, get_match_data(&v[0]), 1);
  SET_RETURN(ary);
}

/* MatchData#length, #size -> Integer */
static void
c_match_data_length(mrbc_vm *vm, mrbc_value v[], int argc)
{
  SET_INT_RETURN(get_match_data(&v[0])->nsave / 2);
}

/* MatchData#string -> the subject */
static void
c_match_data_string(mrbc_vm *vm, mrbc_value v[], int argc)
{
  picorb_match_data *md = get_match_data(&v[0]);
  mrbc_incref(&md->str);
  SET_RETURN(md->str);
}

/* MatchData#regexp -> Regexp */
static void
c_match_data_regexp(mrbc_vm *vm, mrbc_value v[], int argc)
{
  picorb_match_data *md = get_match_data(&v[0]);
  mrbc_incref(&md->regexp);
  SET_RETURN(md->regexp);
}

/* MatchData#pre_match -> String before the match */
static void
c_match_data_pre_match(mrbc_vm *vm, mrbc_value v[], int argc)
{
  picorb_match_data *md = get_match_data(&v[0]);
  mrbc_value s = mrbc_string_new(vm, mrbc_string_cstr(&md->str), md->caps[0]);
  SET_RETURN(s);
}

/* MatchData#post_match -> String after the match */
static void
c_match_data_post_match(mrbc_vm *vm, mrbc_value v[], int argc)
{
  picorb_match_data *md = get_match_data(&v[0]);
  int32_t e = md->caps[1];
  mrbc_value s = mrbc_string_new(vm, mrbc_string_cstr(&md->str) + e, mrbc_string_size(&md->str) - e);
  SET_RETURN(s);
}

/* MatchData#begin(idx), #end(idx) -> character offset or nil */
static void
md_offset(mrbc_vm *vm, mrbc_value v[], int argc, int slot)
{
  picorb_match_data *md = get_match_data(&v[0]);
  mrbc_int_t idx;
  if (!md_index(vm, md, v, argc, &idx)) {
    if (!mrbc_israised(vm)) mrbc_raise(vm, MRBC_CLASS(IndexError), "index out of matches");
    return;
  }
  int32_t off = md->caps[2 * idx + slot];
  if (off < 0) SET_NIL_RETURN(); else SET_INT_RETURN(md_char_offset(md, off));
}

static void
c_match_data_begin(mrbc_vm *vm, mrbc_value v[], int argc)
{
  md_offset(vm, v, argc, 0);
}

static void
c_match_data_end(mrbc_vm *vm, mrbc_value v[], int argc)
{
  md_offset(vm, v, argc, 1);
}

/* MatchData#to_s -> the whole match */
static void
c_match_data_to_s(mrbc_vm *vm, mrbc_value v[], int argc)
{
  mrbc_value s = md_group(vm, get_match_data(&v[0]), 0);
  SET_RETURN(s);
}

/* append the inspect form of one group: "text" or nil */
static void
md_inspect_group(mrbc_vm *vm, mrbc_value *result, picorb_match_data *md, int idx)
{
  mrbc_value s = md_group(vm, md, idx);
  if (s.tt == MRBC_TT_NIL) {
    mrbc_string_append_cstr(result, "nil");
    return;
  }
  mrbc_string_append_cstr(result, "\"");
  mrbc_string_append(result, &s);
  mrbc_string_append_cstr(result, "\"");
  mrbc_decref(&s);
}

/* MatchData#inspect -> #<MatchData "ab" 1:"a" 2:nil> */
static void
c_match_data_inspect(mrbc_vm *vm, mrbc_value v[], int argc)
{
  picorb_match_data *md = get_match_data(&v[0]);
  mrbc_value result = mrbc_string_new_cstr(vm, "#<MatchData ");
  md_inspect_group(vm, &result, md, 0);
  int n = md->nsave / 2;
  for (int i = 1; i < n; i++) {
    char buf[16];
    snprintf(buf, sizeof(buf), " %d:", i);
    mrbc_string_append_cstr(&result, buf);
    md_inspect_group(vm, &result, md, i);
  }
  mrbc_string_append_cstr(&result, ">");
  SET_RETURN(result);
}

/* ---- String ---- */

/* The Regexp an argument stands for. A String is compiled into a
 * temporary the caller releases with release_regexp; *temp says so.
 * nil after raising. */
static mrbc_value
ensure_regexp(mrbc_vm *vm, const mrbc_value *pattern, int *temp)
{
  *temp = 0;
  if (regexp_p(pattern)) return *pattern;
  if (pattern->tt == MRBC_TT_STRING) {
    *temp = 1;
    return regexp_new(vm, mrbc_string_cstr(pattern), mrbc_string_size(pattern), 0);
  }
  mrbc_raise(vm, MRBC_CLASS(TypeError), "wrong argument type (expected Regexp)");
  return mrbc_nil_value();
}

static void
release_regexp(mrbc_value *re, int temp)
{
  if (temp && re->tt == MRBC_TT_OBJECT) mrbc_decref(re);
}

/* String#match(pattern, pos = 0) -> MatchData or nil */
static void
c_string_match(mrbc_vm *vm, mrbc_value v[], int argc)
{
  mrbc_int_t pos;
  int temp;
  if (argc < 1) { mrbc_raise(vm, MRBC_CLASS(ArgumentError), "wrong number of arguments"); return; }
  if (!pos_arg(vm, v, argc, &pos)) return;
  mrbc_value re = ensure_regexp(vm, &v[1], &temp);
  if (re.tt != MRBC_TT_OBJECT) return;
  mrbc_value md = regexp_match_at(vm, &re, &v[0], pos);
  release_regexp(&re, temp);
  SET_RETURN(md);
}

/* String#match?(pattern, pos = 0) -> true or false */
static void
c_string_match_p(mrbc_vm *vm, mrbc_value v[], int argc)
{
  mrbc_int_t pos;
  int temp;
  if (argc < 1) { mrbc_raise(vm, MRBC_CLASS(ArgumentError), "wrong number of arguments"); return; }
  if (!pos_arg(vm, v, argc, &pos)) return;
  mrbc_value re = ensure_regexp(vm, &v[1], &temp);
  if (re.tt != MRBC_TT_OBJECT) return;
  int hit = regexp_test_at(&re, &v[0], pos);
  release_regexp(&re, temp);
  if (hit) SET_TRUE_RETURN(); else SET_FALSE_RETURN();
}

/* String#=~(pattern) -> Integer or nil */
static void
c_string_match_op(mrbc_vm *vm, mrbc_value v[], int argc)
{
  int temp;
  if (argc < 1) { mrbc_raise(vm, MRBC_CLASS(ArgumentError), "wrong number of arguments"); return; }
  mrbc_value re = ensure_regexp(vm, &v[1], &temp);
  if (re.tt != MRBC_TT_OBJECT) return;
  long c = regexp_index_of(&re, &v[0]);
  release_regexp(&re, temp);
  if (c < 0) SET_NIL_RETURN(); else SET_INT_RETURN((mrbc_int_t)c);
}

/* ---- sub, scan, split: the C side of mrblib/regexp.rb ---- */

/* Run re over str from byte offset bpos. */
static int
regexp_run_bytes(picorb_regexp *re, const mrbc_value *str, mrbc_int_t bpos, int32_t *caps, int first_only)
{
  size_t len = (size_t)mrbc_string_size(str);
  if (bpos < 0 || (size_t)bpos > len) return 0;
  return regex_exec(re->prog, (const uint8_t *)mrbc_string_cstr(str), len, (size_t)bpos, caps, first_only != 0, re->scratch);
}

/* Regexp#__bmatch(str, byte_pos) -> MatchData or nil */
static void
c_regexp_bmatch(mrbc_vm *vm, mrbc_value v[], int argc)
{
  if (argc < 2 || v[1].tt != MRBC_TT_STRING || v[2].tt != MRBC_TT_INTEGER) {
    mrbc_raise(vm, MRBC_CLASS(TypeError), "__bmatch wants a String and an Integer");
    return;
  }
  picorb_regexp *re = get_regexp(&v[0]);
  int32_t caps[PICORB_RX_MAX_NSAVE];
  if (!regexp_run_bytes(re, &v[1], v[2].i, caps, 0)) { SET_NIL_RETURN(); return; }
  mrbc_value md = match_data_new(vm, &v[0], &v[1], caps, (int)re->prog[1]);
  SET_RETURN(md);
}

/* MatchData#byteoffset(idx) -> [begin, end] in bytes, or nil */
static void
c_match_data_byteoffset(mrbc_vm *vm, mrbc_value v[], int argc)
{
  picorb_match_data *md = get_match_data(&v[0]);
  mrbc_int_t idx;
  if (!md_index(vm, md, v, argc, &idx)) {
    if (!mrbc_israised(vm)) mrbc_raise(vm, MRBC_CLASS(IndexError), "index out of matches");
    return;
  }
  int32_t b = md->caps[2 * idx];
  if (b < 0) { SET_NIL_RETURN(); return; }
  mrbc_value pair = mrbc_array_new(vm, 2);
  mrbc_value vb = mrbc_integer_value(b), ve = mrbc_integer_value(md->caps[2 * idx + 1]);
  mrbc_array_push(&pair, &vb);
  mrbc_array_push(&pair, &ve);
  SET_RETURN(pair);
}

typedef struct {
  mrbc_vm *vm;
  mrbc_value out;            /* the String or Array being built */
  const mrbc_value *subject; /* the String scanned */
} build_ctx;

static void
emit_to_str(void *p, const uint8_t *s, size_t n)
{
  build_ctx *c = (build_ctx *)p;
  if (n) mrbc_string_append_cbuf(&c->out, s, (int)n);
}

/* Regexp.escape(str) -> String */
static void
c_regexp_escape(mrbc_vm *vm, mrbc_value v[], int argc)
{
  if (argc < 1 || v[1].tt != MRBC_TT_STRING) {
    mrbc_raise(vm, MRBC_CLASS(TypeError), "no implicit conversion into String");
    return;
  }
  build_ctx c = { vm, mrbc_string_new(vm, NULL, 0), &v[1] };
  picorb_rx_escape((const uint8_t *)mrbc_string_cstr(&v[1]), (size_t)mrbc_string_size(&v[1]), emit_to_str, &c);
  SET_RETURN(c.out);
}

/* the Regexp in v[1], or NULL after raising */
static picorb_regexp *
regexp_arg(mrbc_vm *vm, mrbc_value v[], int argc, int need)
{
  if (argc < need || !regexp_p(&v[1])) {
    mrbc_raise(vm, MRBC_CLASS(TypeError), "wrong argument type (expected Regexp)");
    return NULL;
  }
  return get_regexp(&v[1]);
}

/* String#__sub(re, replacement, global) -> String, or nil without a match */
static void
c_string_sub_c(mrbc_vm *vm, mrbc_value v[], int argc)
{
  picorb_regexp *re = regexp_arg(vm, v, argc, 3);
  if (!re) return;
  if (v[2].tt != MRBC_TT_STRING) {
    mrbc_raise(vm, MRBC_CLASS(TypeError), "no implicit conversion into String");
    return;
  }
  int global = v[3].tt == MRBC_TT_TRUE;
  build_ctx c = { vm, mrbc_string_new(vm, NULL, 0), &v[0] };
  int n = picorb_rx_sub(re->prog, re->scratch, (const uint8_t *)mrbc_string_cstr(&v[0]), (size_t)mrbc_string_size(&v[0]),
                        (const uint8_t *)mrbc_string_cstr(&v[2]), (size_t)mrbc_string_size(&v[2]), global != 0,
                        emit_to_str, &c);
  if (n) {
    SET_RETURN(c.out);
  } else {
    mrbc_decref(&c.out);
    SET_NIL_RETURN();
  }
}

/* one scan match: the text, or the Array of groups when there are any */
static void
scan_match(void *p, const int32_t *caps, int nsave)
{
  build_ctx *c = (build_ctx *)p;
  const char *s = mrbc_string_cstr(c->subject);
  if (nsave <= 2) {
    mrbc_value m = mrbc_string_new(c->vm, s + caps[0], caps[1] - caps[0]);
    mrbc_array_push(&c->out, &m);
    return;
  }
  mrbc_value groups = mrbc_array_new(c->vm, nsave / 2 - 1);
  for (int g = 1; 2 * g + 1 < nsave; g++) {
    int32_t b = caps[2 * g], e = caps[2 * g + 1];
    mrbc_value item = b < 0 ? mrbc_nil_value() : mrbc_string_new(c->vm, s + b, e - b);
    mrbc_array_push(&groups, &item);
  }
  mrbc_array_push(&c->out, &groups);
}

/* String#__scan(re) -> Array */
static void
c_string_scan_c(mrbc_vm *vm, mrbc_value v[], int argc)
{
  picorb_regexp *re = regexp_arg(vm, v, argc, 1);
  if (!re) return;
  build_ctx c = { vm, mrbc_array_new(vm, 0), &v[0] };
  picorb_rx_scan(re->prog, re->scratch, (const uint8_t *)mrbc_string_cstr(&v[0]), (size_t)mrbc_string_size(&v[0]), scan_match, &c);
  SET_RETURN(c.out);
}

static void
split_piece(void *p, size_t b, size_t e)
{
  build_ctx *c = (build_ctx *)p;
  mrbc_value piece = mrbc_string_new(c->vm, mrbc_string_cstr(c->subject) + b, (int)(e - b));
  mrbc_array_push(&c->out, &piece);
}

/* String#__split(re, limit) -> Array */
static void
c_string_split_c(mrbc_vm *vm, mrbc_value v[], int argc)
{
  picorb_regexp *re = regexp_arg(vm, v, argc, 2);
  if (!re) return;
  if (v[2].tt != MRBC_TT_INTEGER) {
    mrbc_raise(vm, MRBC_CLASS(TypeError), "no implicit conversion into Integer");
    return;
  }
  mrbc_int_t limit = v[2].i;
  build_ctx c = { vm, mrbc_array_new(vm, 0), &v[0] };
  picorb_rx_split(re->prog, re->scratch, (const uint8_t *)mrbc_string_cstr(&v[0]), (size_t)mrbc_string_size(&v[0]), (long)limit, split_piece, &c);
  if (limit == 0) {
    /* trailing empty pieces go, as in CRuby */
    while (mrbc_array_size(&c.out) > 0) {
      mrbc_value *last = mrbc_array_get_p(&c.out, mrbc_array_size(&c.out) - 1);
      if (last->tt != MRBC_TT_STRING || mrbc_string_size(last) != 0) break;
      mrbc_value gone = mrbc_array_pop(&c.out);
      mrbc_decref(&gone);
    }
  }
  SET_RETURN(c.out);
}

/* String#replace(str) -> self; mruby/c has none of its own */
static void
c_string_replace(mrbc_vm *vm, mrbc_value v[], int argc)
{
  if (argc < 1 || v[1].tt != MRBC_TT_STRING) {
    mrbc_raise(vm, MRBC_CLASS(TypeError), "no implicit conversion into String");
    return;
  }
  mrbc_string_clear(&v[0]);
  mrbc_string_append(&v[0], &v[1]);
  /* v[0] stays the return value: self */
}

/* ---- init ---- */

void
mrbc_regexp_init(mrbc_vm *vm)
{
  class_RegexpError = mrbc_define_class(vm, "RegexpError", MRBC_CLASS(StandardError));

  class_Regexp = mrbc_define_class(vm, "Regexp", mrbc_class_object);
  mrbc_define_destructor(class_Regexp, regexp_destructor);
  mrbc_define_method(vm, class_Regexp, "compile",   c_regexp_compile);
  mrbc_define_method(vm, class_Regexp, "new",       c_regexp_compile);
  mrbc_define_method(vm, class_Regexp, "match",     c_regexp_match);
  mrbc_define_method(vm, class_Regexp, "match?",    c_regexp_match_p);
  mrbc_define_method(vm, class_Regexp, "===",       c_regexp_eqq);
  mrbc_define_method(vm, class_Regexp, "=~",        c_regexp_match_op);
  mrbc_define_method(vm, class_Regexp, "source",    c_regexp_source);
  mrbc_define_method(vm, class_Regexp, "to_s",      c_regexp_to_s);
  mrbc_define_method(vm, class_Regexp, "inspect",   c_regexp_inspect);
  mrbc_define_method(vm, class_Regexp, "casefold?", c_regexp_casefold_p);
  mrbc_define_method(vm, class_Regexp, "options",   c_regexp_options);

  class_MatchData = mrbc_define_class(vm, "MatchData", mrbc_class_object);
  mrbc_define_destructor(class_MatchData, match_data_destructor);
  mrbc_define_method(vm, class_MatchData, "[]",         c_match_data_aref);
  mrbc_define_method(vm, class_MatchData, "to_a",       c_match_data_to_a);
  mrbc_define_method(vm, class_MatchData, "captures",   c_match_data_captures);
  mrbc_define_method(vm, class_MatchData, "length",     c_match_data_length);
  mrbc_define_method(vm, class_MatchData, "size",       c_match_data_length);
  mrbc_define_method(vm, class_MatchData, "string",     c_match_data_string);
  mrbc_define_method(vm, class_MatchData, "regexp",     c_match_data_regexp);
  mrbc_define_method(vm, class_MatchData, "pre_match",  c_match_data_pre_match);
  mrbc_define_method(vm, class_MatchData, "post_match", c_match_data_post_match);
  mrbc_define_method(vm, class_MatchData, "begin",      c_match_data_begin);
  mrbc_define_method(vm, class_MatchData, "end",        c_match_data_end);
  mrbc_define_method(vm, class_MatchData, "to_s",       c_match_data_to_s);
  mrbc_define_method(vm, class_MatchData, "inspect",    c_match_data_inspect);

  mrbc_class *string_class = MRBC_CLASS(String);
  mrbc_define_method(vm, string_class, "match",  c_string_match);
  mrbc_define_method(vm, string_class, "match?", c_string_match_p);
  mrbc_define_method(vm, string_class, "=~",     c_string_match_op);

  /* the C side of mrblib/regexp.rb */
  mrbc_define_method(vm, class_Regexp, "escape", c_regexp_escape);
  mrbc_define_method(vm, class_Regexp, "__bmatch", c_regexp_bmatch);
  mrbc_define_method(vm, class_MatchData, "byteoffset", c_match_data_byteoffset);
  mrbc_define_method(vm, string_class, "__sub",   c_string_sub_c);
  mrbc_define_method(vm, string_class, "__scan",  c_string_scan_c);
  mrbc_define_method(vm, string_class, "__split", c_string_split_c);
  mrbc_define_method(vm, string_class, "replace", c_string_replace);
}
