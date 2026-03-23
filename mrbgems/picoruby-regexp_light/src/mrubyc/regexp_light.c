#include <mrubyc.h>
#include "regex.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

/*
 * Storage layout for Regexp instances:
 *   instance->data  : picorbc_regexp_t
 *     .regex        : compiled regex_t (atoms heap-allocated by regcomp)
 *     .flags        : integer flags (REG_ICASE | REG_NEWLINE)
 *     .source       : mrbc_value String (original pattern, ref-counted)
 *
 * Storage layout for MatchData instances:
 *   instance->data  : picorbc_match_data_t (variable size)
 *     .nmatch       : number of capture groups + 1
 *     .str          : mrbc_value String (original string, ref-counted)
 *     .regexp       : mrbc_value Regexp (ref-counted)
 *     .pmatch[]     : flexible array of regmatch_t
 */

typedef struct {
  regex_t regex;
  int flags;
  mrbc_value source;
} picorbc_regexp_t;

typedef struct {
  int nmatch;
  mrbc_value str;
  mrbc_value regexp;
  regmatch_t pmatch[];
} picorbc_match_data_t;

static mrbc_class *class_Regexp;
static mrbc_class *class_MatchData;

static void *
mrbc_regex_alloc_fn(void *ctx, size_t size)
{
  return mrbc_alloc((mrbc_vm *)ctx, size);
}

static void
mrbc_regex_free_fn(void *ctx, void *ptr)
{
  mrbc_free((mrbc_vm *)ctx, ptr);
}

/* ---- destructors ---- */

static void
regexp_destructor(mrbc_value *self)
{
  picorbc_regexp_t *re = (picorbc_regexp_t *)self->instance->data;
  if (re->regex.atoms) {
    regfree(&re->regex);
    re->regex.atoms = NULL;
  }
  mrbc_decref(&re->source);
}

static void
match_data_destructor(mrbc_value *self)
{
  picorbc_match_data_t *md = (picorbc_match_data_t *)self->instance->data;
  mrbc_decref(&md->str);
  mrbc_decref(&md->regexp);
}

/* ---- helpers ---- */

/*
 * Convert Ruby-style anchors to regex_light equivalents.
 * \A -> ^,  \z -> $,  \Z -> $
 * Caller must mrbc_free the returned buffer.
 */
static char *
convert_ruby_pattern(mrbc_vm *vm, const char *pattern, int len)
{
  char *out = (char *)mrbc_alloc(vm, (size_t)len + 1);
  if (!out) return NULL;
  int i = 0, j = 0;
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

/* Build cflags from Ruby flags string */
static int
build_cflags(const char *flags, int len)
{
  int cflags = 0;
  int i = 0;
  while (i < len) {
    if (flags[i] == 'i') cflags |= REG_ICASE;
    if (flags[i] == 'm') cflags |= REG_NEWLINE;
    i++;
  }
  return cflags;
}

/*
 * Create and return a Regexp instance.
 * pattern/plen: source pattern bytes
 * flags/flen:   flag string (may be NULL)
 */
static mrbc_value
create_regexp_obj(mrbc_vm *vm, const char *pattern, int plen,
                   const char *flags, int flen)
{
  mrbc_value re_obj = mrbc_instance_new(vm, class_Regexp, sizeof(picorbc_regexp_t));
  picorbc_regexp_t *re = (picorbc_regexp_t *)re_obj.instance->data;

  char *converted = convert_ruby_pattern(vm, pattern, plen);
  if (!converted) {
    mrbc_raise(vm, MRBC_CLASS(RuntimeError), "out of memory");
    return mrbc_nil_value();
  }
  int r = regcomp(&re->regex, converted, 0, (void *)vm, mrbc_regex_alloc_fn, mrbc_regex_free_fn);
  mrbc_free(vm, converted);

  if (r != 0) {
    mrbc_raise(vm, MRBC_CLASS(ArgumentError), "invalid regular expression");
    return mrbc_nil_value();
  }

  re->flags = flags ? build_cflags(flags, flen) : 0;
  re->source = mrbc_string_new(vm, pattern, plen);
  mrbc_incref(&re->source);

  return re_obj;
}

/*
 * Create and return a MatchData instance from a regexec() result.
 * pmatch/nmatch: match positions array
 * str_val:       original string mrbc_value
 * re_val:        Regexp mrbc_value
 */
static mrbc_value
create_match_data_obj(mrbc_vm *vm, const regmatch_t *pmatch, int nmatch,
                       mrbc_value str_val, mrbc_value re_val)
{
  size_t data_size = sizeof(picorbc_match_data_t) + sizeof(regmatch_t) * (size_t)nmatch;
  mrbc_value md_obj = mrbc_instance_new(vm, class_MatchData, data_size);
  picorbc_match_data_t *md = (picorbc_match_data_t *)md_obj.instance->data;

  md->nmatch = nmatch;
  mrbc_incref(&str_val);
  md->str = str_val;
  mrbc_incref(&re_val);
  md->regexp = re_val;
  memcpy(md->pmatch, pmatch, sizeof(regmatch_t) * (size_t)nmatch);

  return md_obj;
}

/*
 * Run regexec and return a MatchData value or nil.
 * re_obj must be a Regexp instance, str_val a String.
 */
static mrbc_value
do_match(mrbc_vm *vm, mrbc_value re_obj, mrbc_value str_val)
{
  picorbc_regexp_t *re = (picorbc_regexp_t *)re_obj.instance->data;
  int nmatch = (int)(re->regex.re_nsub + 1);
  regmatch_t *pmatch = (regmatch_t *)mrbc_alloc(vm, sizeof(regmatch_t) * (size_t)nmatch);
  if (!pmatch) {
    mrbc_raise(vm, MRBC_CLASS(RuntimeError), "out of memory");
    return mrbc_nil_value();
  }

  const char *str = (const char *)str_val.string->data;
  int r = regexec(&re->regex, str, (size_t)nmatch, pmatch, 0);
  if (r != 0) {
    mrbc_free(vm, pmatch);
    return mrbc_nil_value();
  }

  mrbc_value md = create_match_data_obj(vm, pmatch, nmatch, str_val, re_obj);
  mrbc_free(vm, pmatch);
  return md;
}

/* Ensure pattern is a Regexp; compile String if needed */
static mrbc_value
ensure_regexp(mrbc_vm *vm, mrbc_value pattern)
{
  if (pattern.tt == MRBC_TT_OBJECT &&
      pattern.instance->cls == class_Regexp) {
    return pattern;
  }
  if (pattern.tt == MRBC_TT_STRING) {
    return create_regexp_obj(vm,
      (const char *)pattern.string->data, pattern.string->size,
      NULL, 0);
  }
  mrbc_raise(vm, MRBC_CLASS(TypeError),
    "wrong argument type (expected Regexp or String)");
  return mrbc_nil_value();
}

/* ---- Regexp class methods ---- */

/* Regexp.compile(pattern, flags=nil, encoding=nil) / Regexp.new */
static void
c_regexp_compile(mrbc_vm *vm, mrbc_value v[], int argc)
{
  if (argc < 1 || v[1].tt != MRBC_TT_STRING) {
    mrbc_raise(vm, MRBC_CLASS(ArgumentError), "wrong number of arguments");
    return;
  }

  const char *pattern = (const char *)v[1].string->data;
  int plen = v[1].string->size;
  const char *flags = NULL;
  int flen = 0;

  if (argc >= 2) {
    if (v[2].tt == MRBC_TT_STRING) {
      flags = (const char *)v[2].string->data;
      flen = v[2].string->size;
    } else if (v[2].tt == MRBC_TT_INTEGER) {
      /* Integer flags: IGNORECASE=1, EXTENDED=2, MULTILINE=4 */
      mrbc_int_t opts = v[2].i;
      char ruby_flags[4];
      int k = 0;
      if (opts & 1) ruby_flags[k++] = 'i';
      if (opts & 4) ruby_flags[k++] = 'm';
      ruby_flags[k] = '\0';
      mrbc_value re = create_regexp_obj(vm, pattern, plen, ruby_flags, k);
      SET_RETURN(re);
      return;
    }
  }

  mrbc_value re = create_regexp_obj(vm, pattern, plen, flags, flen);
  SET_RETURN(re);
}

/* Regexp#match(str) -> MatchData or nil */
static void
c_regexp_match(mrbc_vm *vm, mrbc_value v[], int argc)
{
  if (argc < 1 || v[1].tt != MRBC_TT_STRING) {
    mrbc_raise(vm, MRBC_CLASS(ArgumentError), "wrong number of arguments");
    return;
  }
  mrbc_value md = do_match(vm, v[0], v[1]);
  SET_RETURN(md);
}

/* Regexp#match?(str) -> bool */
static void
c_regexp_match_p(mrbc_vm *vm, mrbc_value v[], int argc)
{
  if (argc < 1 || v[1].tt != MRBC_TT_STRING) {
    mrbc_raise(vm, MRBC_CLASS(ArgumentError), "wrong number of arguments");
    return;
  }
  picorbc_regexp_t *re = (picorbc_regexp_t *)v[0].instance->data;
  regmatch_t pmatch[1];
  int r = regexec(&re->regex, (const char *)v[1].string->data, 1, pmatch, 0);
  if (r == 0) {
    SET_TRUE_RETURN();
  } else {
    SET_FALSE_RETURN();
  }
}

/* Regexp#===(str) -> bool (for case-when) */
static void
c_regexp_case_eq(mrbc_vm *vm, mrbc_value v[], int argc)
{
  if (argc < 1 || v[1].tt != MRBC_TT_STRING) {
    SET_FALSE_RETURN();
    return;
  }
  picorbc_regexp_t *re = (picorbc_regexp_t *)v[0].instance->data;
  regmatch_t pmatch[1];
  int r = regexec(&re->regex, (const char *)v[1].string->data, 1, pmatch, 0);
  if (r == 0) {
    SET_TRUE_RETURN();
  } else {
    SET_FALSE_RETURN();
  }
}

/* Regexp#=~(str) -> Integer or nil */
static void
c_regexp_match_op(mrbc_vm *vm, mrbc_value v[], int argc)
{
  if (argc < 1 || v[1].tt != MRBC_TT_STRING) {
    SET_NIL_RETURN();
    return;
  }
  picorbc_regexp_t *re = (picorbc_regexp_t *)v[0].instance->data;
  regmatch_t pmatch[1];
  int r = regexec(&re->regex, (const char *)v[1].string->data, 1, pmatch, 0);
  if (r != 0) {
    SET_NIL_RETURN();
  } else {
    SET_INT_RETURN(pmatch[0].rm_so);
  }
}

/* Regexp#source -> String */
static void
c_regexp_source(mrbc_vm *vm, mrbc_value v[], int argc)
{
  picorbc_regexp_t *re = (picorbc_regexp_t *)v[0].instance->data;
  mrbc_incref(&re->source);
  SET_RETURN(re->source);
}

/* Regexp#to_s -> String "(?flags:source)" */
static void
c_regexp_to_s(mrbc_vm *vm, mrbc_value v[], int argc)
{
  picorbc_regexp_t *re = (picorbc_regexp_t *)v[0].instance->data;

  char flags[8];
  int fi = 0;
  if (re->flags & REG_ICASE)   flags[fi++] = 'i';
  if (re->flags & REG_NEWLINE) flags[fi++] = 'm';
  flags[fi] = '\0';

  char prefix[16];
  snprintf(prefix, sizeof(prefix), "(?%s:", flags);
  int prefix_len = (int)strlen(prefix);
  int src_len = re->source.string->size;

  mrbc_value result = mrbc_string_new(vm, NULL, prefix_len + src_len + 1);
  char *buf = (char *)result.string->data;
  memcpy(buf, prefix, (size_t)prefix_len);
  memcpy(buf + prefix_len, re->source.string->data, (size_t)src_len);
  buf[prefix_len + src_len] = ')';
  buf[prefix_len + src_len + 1] = '\0';
  result.string->size = prefix_len + src_len + 1;

  SET_RETURN(result);
}

/* Regexp#inspect -> String "/source/flags" */
static void
c_regexp_inspect(mrbc_vm *vm, mrbc_value v[], int argc)
{
  picorbc_regexp_t *re = (picorbc_regexp_t *)v[0].instance->data;

  char flags[8];
  int fi = 0;
  if (re->flags & REG_ICASE)   flags[fi++] = 'i';
  if (re->flags & REG_NEWLINE) flags[fi++] = 'm';
  flags[fi] = '\0';

  int src_len = re->source.string->size;
  int flags_len = fi;
  mrbc_value result = mrbc_string_new(vm, NULL, 1 + src_len + 1 + flags_len);
  char *buf = (char *)result.string->data;
  buf[0] = '/';
  memcpy(buf + 1, re->source.string->data, (size_t)src_len);
  buf[1 + src_len] = '/';
  memcpy(buf + 1 + src_len + 1, flags, (size_t)flags_len);
  buf[1 + src_len + 1 + flags_len] = '\0';
  result.string->size = 1 + src_len + 1 + flags_len;

  SET_RETURN(result);
}

/* Regexp#casefold? -> bool */
static void
c_regexp_casefold_p(mrbc_vm *vm, mrbc_value v[], int argc)
{
  picorbc_regexp_t *re = (picorbc_regexp_t *)v[0].instance->data;
  if (re->flags & REG_ICASE) {
    SET_TRUE_RETURN();
  } else {
    SET_FALSE_RETURN();
  }
}

/* Regexp#options -> Integer */
static void
c_regexp_options(mrbc_vm *vm, mrbc_value v[], int argc)
{
  picorbc_regexp_t *re = (picorbc_regexp_t *)v[0].instance->data;
  mrbc_int_t opts = 0;
  if (re->flags & REG_ICASE)   opts |= 1;
  if (re->flags & REG_NEWLINE) opts |= 4;
  SET_INT_RETURN(opts);
}

/* Regexp#free - explicitly free regex_t.atoms (optional on microcontrollers) */
static void
c_regexp_free(mrbc_vm *vm, mrbc_value v[], int argc)
{
  picorbc_regexp_t *re = (picorbc_regexp_t *)v[0].instance->data;
  if (re->regex.atoms) {
    regfree(&re->regex);
    re->regex.atoms = NULL;
  }
  SET_NIL_RETURN();
}

/* ---- MatchData instance methods ---- */

/* MatchData#[](idx) -> String or nil */
static void
c_match_data_aref(mrbc_vm *vm, mrbc_value v[], int argc)
{
  if (argc < 1 || v[1].tt != MRBC_TT_INTEGER) {
    SET_NIL_RETURN();
    return;
  }
  picorbc_match_data_t *md = (picorbc_match_data_t *)v[0].instance->data;
  mrbc_int_t idx = v[1].i;

  if (idx < 0) idx += md->nmatch;
  if (idx < 0 || idx >= md->nmatch) {
    SET_NIL_RETURN();
    return;
  }

  regmatch_t *m = &md->pmatch[idx];
  if (m->rm_so < 0) {
    SET_NIL_RETURN();
    return;
  }

  mrbc_value result = mrbc_string_new(vm,
    (const char *)md->str.string->data + m->rm_so, (int)(m->rm_eo - m->rm_so));
  SET_RETURN(result);
}

/* MatchData#to_a -> Array */
static void
c_match_data_to_a(mrbc_vm *vm, mrbc_value v[], int argc)
{
  picorbc_match_data_t *md = (picorbc_match_data_t *)v[0].instance->data;
  mrbc_value ary = mrbc_array_new(vm, md->nmatch);

  for (int i = 0; i < md->nmatch; i++) {
    regmatch_t *m = &md->pmatch[i];
    if (m->rm_so < 0) {
      mrbc_value nil = mrbc_nil_value();
      mrbc_array_push(&ary, &nil);
    } else {
      mrbc_value s = mrbc_string_new(vm,
        (const char *)md->str.string->data + m->rm_so, (int)(m->rm_eo - m->rm_so));
      mrbc_array_push(&ary, &s);
    }
  }
  SET_RETURN(ary);
}

/* MatchData#length / MatchData#size -> Integer */
static void
c_match_data_length(mrbc_vm *vm, mrbc_value v[], int argc)
{
  picorbc_match_data_t *md = (picorbc_match_data_t *)v[0].instance->data;
  SET_INT_RETURN(md->nmatch);
}

/* MatchData#string -> String (original) */
static void
c_match_data_string(mrbc_vm *vm, mrbc_value v[], int argc)
{
  picorbc_match_data_t *md = (picorbc_match_data_t *)v[0].instance->data;
  mrbc_incref(&md->str);
  SET_RETURN(md->str);
}

/* MatchData#regexp -> Regexp */
static void
c_match_data_regexp(mrbc_vm *vm, mrbc_value v[], int argc)
{
  picorbc_match_data_t *md = (picorbc_match_data_t *)v[0].instance->data;
  mrbc_incref(&md->regexp);
  SET_RETURN(md->regexp);
}

/* MatchData#pre_match -> String */
static void
c_match_data_pre_match(mrbc_vm *vm, mrbc_value v[], int argc)
{
  picorbc_match_data_t *md = (picorbc_match_data_t *)v[0].instance->data;
  regmatch_t *m = &md->pmatch[0];

  if (m->rm_so < 0) {
    mrbc_value empty = mrbc_string_new_cstr(vm, "");
    SET_RETURN(empty);
    return;
  }
  mrbc_value result = mrbc_string_new(vm,
    (const char *)md->str.string->data, (int)m->rm_so);
  SET_RETURN(result);
}

/* MatchData#post_match -> String */
static void
c_match_data_post_match(mrbc_vm *vm, mrbc_value v[], int argc)
{
  picorbc_match_data_t *md = (picorbc_match_data_t *)v[0].instance->data;
  regmatch_t *m = &md->pmatch[0];

  if (m->rm_eo < 0) {
    mrbc_value empty = mrbc_string_new_cstr(vm, "");
    SET_RETURN(empty);
    return;
  }
  int slen = md->str.string->size;
  mrbc_value result = mrbc_string_new(vm,
    (const char *)md->str.string->data + m->rm_eo, slen - (int)m->rm_eo);
  SET_RETURN(result);
}

/* MatchData#begin(idx) -> Integer or nil */
static void
c_match_data_begin(mrbc_vm *vm, mrbc_value v[], int argc)
{
  if (argc < 1 || v[1].tt != MRBC_TT_INTEGER) {
    SET_NIL_RETURN();
    return;
  }
  picorbc_match_data_t *md = (picorbc_match_data_t *)v[0].instance->data;
  mrbc_int_t idx = v[1].i;

  if (idx < 0 || idx >= md->nmatch) {
    mrbc_raise(vm, MRBC_CLASS(IndexError), "index out of bounds");
    return;
  }

  regmatch_t *m = &md->pmatch[idx];
  if (m->rm_so < 0) {
    SET_NIL_RETURN();
  } else {
    SET_INT_RETURN(m->rm_so);
  }
}

/* MatchData#end(idx) -> Integer or nil */
static void
c_match_data_end(mrbc_vm *vm, mrbc_value v[], int argc)
{
  if (argc < 1 || v[1].tt != MRBC_TT_INTEGER) {
    SET_NIL_RETURN();
    return;
  }
  picorbc_match_data_t *md = (picorbc_match_data_t *)v[0].instance->data;
  mrbc_int_t idx = v[1].i;

  if (idx < 0 || idx >= md->nmatch) {
    mrbc_raise(vm, MRBC_CLASS(IndexError), "index out of bounds");
    return;
  }

  regmatch_t *m = &md->pmatch[idx];
  if (m->rm_eo < 0) {
    SET_NIL_RETURN();
  } else {
    SET_INT_RETURN(m->rm_eo);
  }
}

/* MatchData#captures -> Array (groups 1..) */
static void
c_match_data_captures(mrbc_vm *vm, mrbc_value v[], int argc)
{
  picorbc_match_data_t *md = (picorbc_match_data_t *)v[0].instance->data;
  if (md->nmatch <= 1) {
    mrbc_value empty = mrbc_array_new(vm, 0);
    SET_RETURN(empty);
    return;
  }

  mrbc_value ary = mrbc_array_new(vm, md->nmatch - 1);
  for (int i = 1; i < md->nmatch; i++) {
    regmatch_t *m = &md->pmatch[i];
    if (m->rm_so < 0) {
      mrbc_value nil = mrbc_nil_value();
      mrbc_array_push(&ary, &nil);
    } else {
      mrbc_value s = mrbc_string_new(vm,
        (const char *)md->str.string->data + m->rm_so, (int)(m->rm_eo - m->rm_so));
      mrbc_array_push(&ary, &s);
    }
  }
  SET_RETURN(ary);
}

/* MatchData#to_s -> String (full match) */
static void
c_match_data_to_s(mrbc_vm *vm, mrbc_value v[], int argc)
{
  picorbc_match_data_t *md = (picorbc_match_data_t *)v[0].instance->data;
  regmatch_t *m = &md->pmatch[0];

  if (m->rm_so < 0) {
    mrbc_value empty = mrbc_string_new_cstr(vm, "");
    SET_RETURN(empty);
    return;
  }
  mrbc_value result = mrbc_string_new(vm,
    (const char *)md->str.string->data + m->rm_so, (int)(m->rm_eo - m->rm_so));
  SET_RETURN(result);
}

/* MatchData#inspect -> String "#<MatchData \"match\" n:\"group\"...>" */
static void
c_match_data_inspect(mrbc_vm *vm, mrbc_value v[], int argc)
{
  picorbc_match_data_t *md = (picorbc_match_data_t *)v[0].instance->data;
  regmatch_t *m = &md->pmatch[0];

  if (m->rm_so < 0) {
    mrbc_value result = mrbc_string_new_cstr(vm, "#<MatchData (no match)>");
    SET_RETURN(result);
    return;
  }

  const char *str_data = (const char *)md->str.string->data;
  int match_len = (int)(m->rm_eo - m->rm_so);
  int buf_size = 20 + match_len + (md->nmatch - 1) * 32;
  char *buf = (char *)mrbc_alloc(vm, (size_t)buf_size);
  if (!buf) {
    mrbc_value empty = mrbc_string_new_cstr(vm, "#<MatchData>");
    SET_RETURN(empty);
    return;
  }

  int pos = 0;
  pos += snprintf(buf + pos, (size_t)(buf_size - pos), "#<MatchData \"");
  if (pos + match_len < buf_size) {
    memcpy(buf + pos, str_data + m->rm_so, (size_t)match_len);
    pos += match_len;
  }
  pos += snprintf(buf + pos, (size_t)(buf_size - pos), "\"");

  for (int i = 1; i < md->nmatch; i++) {
    regmatch_t *g = &md->pmatch[i];
    pos += snprintf(buf + pos, (size_t)(buf_size - pos), " %d:", i);
    if (g->rm_so < 0) {
      pos += snprintf(buf + pos, (size_t)(buf_size - pos), "nil");
    } else {
      int glen = (int)(g->rm_eo - g->rm_so);
      pos += snprintf(buf + pos, (size_t)(buf_size - pos), "\"");
      if (pos + glen < buf_size) {
        memcpy(buf + pos, str_data + g->rm_so, (size_t)glen);
        pos += glen;
      }
      pos += snprintf(buf + pos, (size_t)(buf_size - pos), "\"");
    }
  }
  snprintf(buf + pos, (size_t)(buf_size - pos), ">");

  mrbc_value result = mrbc_string_new_cstr(vm, buf);
  mrbc_free(vm, buf);
  SET_RETURN(result);
}

/* ---- String extensions ---- */

/* String#match(regexp_or_str) -> MatchData or nil */
static void
c_string_match(mrbc_vm *vm, mrbc_value v[], int argc)
{
  if (argc < 1) {
    SET_NIL_RETURN();
    return;
  }
  mrbc_value re = ensure_regexp(vm, v[1]);
  if (re.tt != MRBC_TT_OBJECT) {
    SET_NIL_RETURN();
    return;
  }
  mrbc_value md = do_match(vm, re, v[0]);
  SET_RETURN(md);
}

/* String#match?(regexp_or_str) -> bool */
static void
c_string_match_p(mrbc_vm *vm, mrbc_value v[], int argc)
{
  if (argc < 1) {
    SET_FALSE_RETURN();
    return;
  }
  mrbc_value re = ensure_regexp(vm, v[1]);
  if (re.tt != MRBC_TT_OBJECT) {
    SET_FALSE_RETURN();
    return;
  }
  picorbc_regexp_t *regex = (picorbc_regexp_t *)re.instance->data;
  regmatch_t pmatch[1];
  int r = regexec(&regex->regex, (const char *)v[0].string->data, 1, pmatch, 0);
  if (r == 0) {
    SET_TRUE_RETURN();
  } else {
    SET_FALSE_RETURN();
  }
}

/* String#=~(regexp) -> Integer or nil */
static void
c_string_match_op(mrbc_vm *vm, mrbc_value v[], int argc)
{
  if (argc < 1) {
    SET_NIL_RETURN();
    return;
  }
  mrbc_value re = ensure_regexp(vm, v[1]);
  if (re.tt != MRBC_TT_OBJECT) {
    SET_NIL_RETURN();
    return;
  }
  picorbc_regexp_t *regex = (picorbc_regexp_t *)re.instance->data;
  regmatch_t pmatch[1];
  int r = regexec(&regex->regex, (const char *)v[0].string->data, 1, pmatch, 0);
  if (r != 0) {
    SET_NIL_RETURN();
  } else {
    SET_INT_RETURN(pmatch[0].rm_so);
  }
}

/* ---- Gem init ---- */

void
mrbc_regexp_light_init(mrbc_vm *vm)
{
  /* Regexp class */
  class_Regexp = mrbc_define_class(vm, "Regexp", mrbc_class_object);
  mrbc_define_destructor(class_Regexp, regexp_destructor);

  mrbc_define_method(vm, class_Regexp, "compile",   c_regexp_compile);
  mrbc_define_method(vm, class_Regexp, "new",       c_regexp_compile);
  mrbc_define_method(vm, class_Regexp, "match",     c_regexp_match);
  mrbc_define_method(vm, class_Regexp, "match?",    c_regexp_match_p);
  mrbc_define_method(vm, class_Regexp, "===",       c_regexp_case_eq);
  mrbc_define_method(vm, class_Regexp, "=~",        c_regexp_match_op);
  mrbc_define_method(vm, class_Regexp, "source",    c_regexp_source);
  mrbc_define_method(vm, class_Regexp, "to_s",      c_regexp_to_s);
  mrbc_define_method(vm, class_Regexp, "inspect",   c_regexp_inspect);
  mrbc_define_method(vm, class_Regexp, "casefold?", c_regexp_casefold_p);
  mrbc_define_method(vm, class_Regexp, "options",   c_regexp_options);
  mrbc_define_method(vm, class_Regexp, "free",      c_regexp_free);

  /* MatchData class */
  class_MatchData = mrbc_define_class(vm, "MatchData", mrbc_class_object);
  mrbc_define_destructor(class_MatchData, match_data_destructor);

  mrbc_define_method(vm, class_MatchData, "[]",         c_match_data_aref);
  mrbc_define_method(vm, class_MatchData, "to_a",       c_match_data_to_a);
  mrbc_define_method(vm, class_MatchData, "length",     c_match_data_length);
  mrbc_define_method(vm, class_MatchData, "size",       c_match_data_length);
  mrbc_define_method(vm, class_MatchData, "string",     c_match_data_string);
  mrbc_define_method(vm, class_MatchData, "regexp",     c_match_data_regexp);
  mrbc_define_method(vm, class_MatchData, "pre_match",  c_match_data_pre_match);
  mrbc_define_method(vm, class_MatchData, "post_match", c_match_data_post_match);
  mrbc_define_method(vm, class_MatchData, "begin",      c_match_data_begin);
  mrbc_define_method(vm, class_MatchData, "end",        c_match_data_end);
  mrbc_define_method(vm, class_MatchData, "captures",   c_match_data_captures);
  mrbc_define_method(vm, class_MatchData, "to_s",       c_match_data_to_s);
  mrbc_define_method(vm, class_MatchData, "inspect",    c_match_data_inspect);

  /* String extensions */
  mrbc_class *string_class = MRBC_CLASS(String);
  mrbc_define_method(vm, string_class, "match",  c_string_match);
  mrbc_define_method(vm, string_class, "match?", c_string_match_p);
  mrbc_define_method(vm, string_class, "=~",     c_string_match_op);
}
