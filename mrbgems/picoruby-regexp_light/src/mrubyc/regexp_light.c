#include <mrubyc.h>
#include "regex.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

/*
 * Storage layout for Regexp instances:
 *   instance->data  : regex_t struct (compiled pattern, atoms on heap)
 *   ivar @source    : String  (original pattern)
 *   ivar @flags     : Integer (REG_ICASE | REG_NEWLINE bits)
 *
 * Note: regex_t.atoms is heap-allocated by regcomp() via stdlib malloc
 * (REGEX_USE_ALLOC_LIBC is defined). When the instance is freed by the GC
 * the atoms are NOT freed automatically. Call Regexp#free explicitly if
 * you need to reclaim that memory on a resource-constrained target.
 *
 * Storage layout for MatchData instances:
 *   ivar @str       : String  (frozen original string)
 *   ivar @regexp    : Regexp  (the pattern that produced this match)
 *   ivar @nmatch    : Integer (total number of entries including index 0)
 *   ivar @positions : Array   [so0, eo0, so1, eo1, ...] (flat int pairs)
 */

static mrbc_class *class_Regexp;
static mrbc_class *class_MatchData;

#define SYM(name) mrbc_str_to_symid(name)

/* ---- helpers ---- */

/*
 * Convert Ruby-style anchors to regex_light equivalents.
 * \A -> ^,  \z -> $,  \Z -> $
 * Caller must free() the returned buffer.
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
  mrbc_value re_obj = mrbc_instance_new(vm, class_Regexp, sizeof(regex_t));
  regex_t *re = (regex_t *)re_obj.instance->data;

  char *converted = convert_ruby_pattern(vm, pattern, plen);
  if (!converted) {
    mrbc_raise(vm, MRBC_CLASS(RuntimeError), "out of memory");
    return mrbc_nil_value();
  }
  int r = regcomp(re, converted, 0);
  mrbc_free(vm, converted);

  if (r != 0) {
    mrbc_raise(vm, MRBC_CLASS(ArgumentError), "invalid regular expression");
    return mrbc_nil_value();
  }

  mrbc_value src = mrbc_string_new(vm, pattern, plen);
  mrbc_instance_setiv(&re_obj, SYM("source"), &src);
  mrbc_value flg = mrbc_integer_value(flags ? build_cflags(flags, flen) : 0);
  mrbc_instance_setiv(&re_obj, SYM("flags"), &flg);

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
  mrbc_value md_obj = mrbc_instance_new(vm, class_MatchData, 0);

  mrbc_instance_setiv(&md_obj, SYM("str"),    &str_val);
  mrbc_instance_setiv(&md_obj, SYM("regexp"), &re_val);
  mrbc_value nm = mrbc_integer_value(nmatch);
  mrbc_instance_setiv(&md_obj, SYM("nmatch"), &nm);

  mrbc_value positions = mrbc_array_new(vm, nmatch * 2);
  for (int i = 0; i < nmatch; i++) {
    mrbc_value so = mrbc_integer_value(pmatch[i].rm_so);
    mrbc_value eo = mrbc_integer_value(pmatch[i].rm_eo);
    mrbc_array_push(&positions, &so);
    mrbc_array_push(&positions, &eo);
  }
  mrbc_instance_setiv(&md_obj, SYM("positions"), &positions);

  return md_obj;
}

/*
 * Run regexec and return a MatchData value or nil.
 * re_obj must be a Regexp instance, str_val a String.
 */
static mrbc_value
do_match(mrbc_vm *vm, mrbc_value re_obj, mrbc_value str_val)
{
  regex_t *re = (regex_t *)re_obj.instance->data;
  int nmatch = (int)(re->re_nsub + 1);
  regmatch_t *pmatch = (regmatch_t *)mrbc_alloc(vm, sizeof(regmatch_t) * (size_t)nmatch);
  if (!pmatch) {
    mrbc_raise(vm, MRBC_CLASS(RuntimeError), "out of memory");
    return mrbc_nil_value();
  }

  const char *str = (const char *)str_val.string->data;
  int r = regexec(re, str, (size_t)nmatch, pmatch, 0);
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
  regex_t *re = (regex_t *)v[0].instance->data;
  regmatch_t pmatch[1];
  int r = regexec(re, (const char *)v[1].string->data, 1, pmatch, 0);
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
  regex_t *re = (regex_t *)v[0].instance->data;
  regmatch_t pmatch[1];
  int r = regexec(re, (const char *)v[1].string->data, 1, pmatch, 0);
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
  regex_t *re = (regex_t *)v[0].instance->data;
  regmatch_t pmatch[1];
  int r = regexec(re, (const char *)v[1].string->data, 1, pmatch, 0);
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
  mrbc_value src = mrbc_instance_getiv(&v[0], SYM("source"));
  SET_RETURN(src);
}

/* Regexp#to_s -> String "(?flags:source)" */
static void
c_regexp_to_s(mrbc_vm *vm, mrbc_value v[], int argc)
{
  mrbc_value src = mrbc_instance_getiv(&v[0], SYM("source"));
  mrbc_int_t flg = mrbc_instance_getiv(&v[0], SYM("flags")).i;

  char flags[8];
  int fi = 0;
  if (flg & REG_ICASE)   flags[fi++] = 'i';
  if (flg & REG_NEWLINE) flags[fi++] = 'm';
  flags[fi] = '\0';

  /* Build "(?<flags>:<source>)" */
  char prefix[16];
  snprintf(prefix, sizeof(prefix), "(?%s:", flags);
  int prefix_len = (int)strlen(prefix);
  int src_len = src.string->size;

  mrbc_value result = mrbc_string_new(vm, NULL, prefix_len + src_len + 1);
  char *buf = (char *)result.string->data;
  memcpy(buf, prefix, (size_t)prefix_len);
  memcpy(buf + prefix_len, src.string->data, (size_t)src_len);
  buf[prefix_len + src_len] = ')';
  buf[prefix_len + src_len + 1] = '\0';
  result.string->size = prefix_len + src_len + 1;

  SET_RETURN(result);
}

/* Regexp#inspect -> String "/source/flags" */
static void
c_regexp_inspect(mrbc_vm *vm, mrbc_value v[], int argc)
{
  mrbc_value src = mrbc_instance_getiv(&v[0], SYM("source"));
  mrbc_int_t flg = mrbc_instance_getiv(&v[0], SYM("flags")).i;

  char flags[8];
  int fi = 0;
  if (flg & REG_ICASE)   flags[fi++] = 'i';
  if (flg & REG_NEWLINE) flags[fi++] = 'm';
  flags[fi] = '\0';

  int src_len = src.string->size;
  int flags_len = fi;
  /* "/" + source + "/" + flags */
  mrbc_value result = mrbc_string_new(vm, NULL, 1 + src_len + 1 + flags_len);
  char *buf = (char *)result.string->data;
  buf[0] = '/';
  memcpy(buf + 1, src.string->data, (size_t)src_len);
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
  mrbc_int_t flg = mrbc_instance_getiv(&v[0], SYM("flags")).i;
  if (flg & REG_ICASE) {
    SET_TRUE_RETURN();
  } else {
    SET_FALSE_RETURN();
  }
}

/* Regexp#options -> Integer */
static void
c_regexp_options(mrbc_vm *vm, mrbc_value v[], int argc)
{
  mrbc_int_t flg = mrbc_instance_getiv(&v[0], SYM("flags")).i;
  mrbc_int_t opts = 0;
  if (flg & REG_ICASE)   opts |= 1;
  if (flg & REG_NEWLINE) opts |= 4;
  SET_INT_RETURN(opts);
}

/* Regexp#free - explicitly free regex_t.atoms (optional on microcontrollers) */
static void
c_regexp_free(mrbc_vm *vm, mrbc_value v[], int argc)
{
  regex_t *re = (regex_t *)v[0].instance->data;
  if (re && re->atoms) {
    regfree(re);
    re->atoms = NULL;
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
  mrbc_int_t idx = v[1].i;
  mrbc_value nmatch_val = mrbc_instance_getiv(&v[0], SYM("nmatch"));
  mrbc_int_t nmatch = nmatch_val.i;

  if (idx < 0) idx += nmatch;
  if (idx < 0 || idx >= nmatch) {
    SET_NIL_RETURN();
    return;
  }

  mrbc_value positions = mrbc_instance_getiv(&v[0], SYM("positions"));
  mrbc_value so_val = mrbc_array_get(&positions, (int)(idx * 2));
  mrbc_value eo_val = mrbc_array_get(&positions, (int)(idx * 2 + 1));
  mrbc_int_t so = so_val.i;
  mrbc_int_t eo = eo_val.i;

  if (so < 0) {
    SET_NIL_RETURN();
    return;
  }

  mrbc_value str = mrbc_instance_getiv(&v[0], SYM("str"));
  mrbc_value result = mrbc_string_new(vm,
    (const char *)str.string->data + so, (int)(eo - so));
  SET_RETURN(result);
}

/* MatchData#to_a -> Array */
static void
c_match_data_to_a(mrbc_vm *vm, mrbc_value v[], int argc)
{
  mrbc_value nmatch_val = mrbc_instance_getiv(&v[0], SYM("nmatch"));
  mrbc_int_t nmatch = nmatch_val.i;
  mrbc_value positions = mrbc_instance_getiv(&v[0], SYM("positions"));
  mrbc_value str = mrbc_instance_getiv(&v[0], SYM("str"));

  mrbc_value ary = mrbc_array_new(vm, (int)nmatch);
  for (mrbc_int_t i = 0; i < nmatch; i++) {
    mrbc_value so_val = mrbc_array_get(&positions, (int)(i * 2));
    mrbc_value eo_val = mrbc_array_get(&positions, (int)(i * 2 + 1));
    mrbc_int_t so = so_val.i;
    mrbc_int_t eo = eo_val.i;
    if (so < 0) {
      mrbc_value nil = mrbc_nil_value();
      mrbc_array_push(&ary, &nil);
    } else {
      mrbc_value s = mrbc_string_new(vm,
        (const char *)str.string->data + so, (int)(eo - so));
      mrbc_array_push(&ary, &s);
    }
  }
  SET_RETURN(ary);
}

/* MatchData#length / MatchData#size -> Integer */
static void
c_match_data_length(mrbc_vm *vm, mrbc_value v[], int argc)
{
  mrbc_value nmatch_val = mrbc_instance_getiv(&v[0], SYM("nmatch"));
  SET_INT_RETURN(nmatch_val.i);
}

/* MatchData#string -> String (frozen original) */
static void
c_match_data_string(mrbc_vm *vm, mrbc_value v[], int argc)
{
  mrbc_value str = mrbc_instance_getiv(&v[0], SYM("str"));
  SET_RETURN(str);
}

/* MatchData#regexp -> Regexp */
static void
c_match_data_regexp(mrbc_vm *vm, mrbc_value v[], int argc)
{
  mrbc_value re = mrbc_instance_getiv(&v[0], SYM("regexp"));
  SET_RETURN(re);
}

/* MatchData#pre_match -> String */
static void
c_match_data_pre_match(mrbc_vm *vm, mrbc_value v[], int argc)
{
  mrbc_value positions = mrbc_instance_getiv(&v[0], SYM("positions"));
  mrbc_value str = mrbc_instance_getiv(&v[0], SYM("str"));
  mrbc_int_t so = mrbc_array_get(&positions, 0).i;

  if (so < 0) {
    mrbc_value empty = mrbc_string_new_cstr(vm, "");
    SET_RETURN(empty);
    return;
  }
  mrbc_value result = mrbc_string_new(vm, (const char *)str.string->data, (int)so);
  SET_RETURN(result);
}

/* MatchData#post_match -> String */
static void
c_match_data_post_match(mrbc_vm *vm, mrbc_value v[], int argc)
{
  mrbc_value positions = mrbc_instance_getiv(&v[0], SYM("positions"));
  mrbc_value str = mrbc_instance_getiv(&v[0], SYM("str"));
  mrbc_int_t eo = mrbc_array_get(&positions, 1).i;

  if (eo < 0) {
    mrbc_value empty = mrbc_string_new_cstr(vm, "");
    SET_RETURN(empty);
    return;
  }
  int slen = str.string->size;
  mrbc_value result = mrbc_string_new(vm,
    (const char *)str.string->data + eo, slen - (int)eo);
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
  mrbc_int_t idx = v[1].i;
  mrbc_int_t nmatch = mrbc_instance_getiv(&v[0], SYM("nmatch")).i;

  if (idx < 0 || idx >= nmatch) {
    mrbc_raise(vm, MRBC_CLASS(IndexError), "index out of bounds");
    return;
  }

  mrbc_value positions = mrbc_instance_getiv(&v[0], SYM("positions"));
  mrbc_int_t so = mrbc_array_get(&positions, (int)(idx * 2)).i;
  if (so < 0) {
    SET_NIL_RETURN();
  } else {
    SET_INT_RETURN(so);
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
  mrbc_int_t idx = v[1].i;
  mrbc_int_t nmatch = mrbc_instance_getiv(&v[0], SYM("nmatch")).i;

  if (idx < 0 || idx >= nmatch) {
    mrbc_raise(vm, MRBC_CLASS(IndexError), "index out of bounds");
    return;
  }

  mrbc_value positions = mrbc_instance_getiv(&v[0], SYM("positions"));
  mrbc_int_t eo = mrbc_array_get(&positions, (int)(idx * 2 + 1)).i;
  if (eo < 0) {
    SET_NIL_RETURN();
  } else {
    SET_INT_RETURN(eo);
  }
}

/* MatchData#captures -> Array (groups 1..) */
static void
c_match_data_captures(mrbc_vm *vm, mrbc_value v[], int argc)
{
  mrbc_int_t nmatch = mrbc_instance_getiv(&v[0], SYM("nmatch")).i;
  if (nmatch <= 1) {
    mrbc_value empty = mrbc_array_new(vm, 0);
    SET_RETURN(empty);
    return;
  }

  mrbc_value positions = mrbc_instance_getiv(&v[0], SYM("positions"));
  mrbc_value str = mrbc_instance_getiv(&v[0], SYM("str"));
  mrbc_value ary = mrbc_array_new(vm, (int)(nmatch - 1));

  for (mrbc_int_t i = 1; i < nmatch; i++) {
    mrbc_int_t so = mrbc_array_get(&positions, (int)(i * 2)).i;
    mrbc_int_t eo = mrbc_array_get(&positions, (int)(i * 2 + 1)).i;
    if (so < 0) {
      mrbc_value nil = mrbc_nil_value();
      mrbc_array_push(&ary, &nil);
    } else {
      mrbc_value s = mrbc_string_new(vm,
        (const char *)str.string->data + so, (int)(eo - so));
      mrbc_array_push(&ary, &s);
    }
  }
  SET_RETURN(ary);
}

/* MatchData#to_s -> String (full match) */
static void
c_match_data_to_s(mrbc_vm *vm, mrbc_value v[], int argc)
{
  mrbc_value positions = mrbc_instance_getiv(&v[0], SYM("positions"));
  mrbc_value str = mrbc_instance_getiv(&v[0], SYM("str"));
  mrbc_int_t so = mrbc_array_get(&positions, 0).i;
  mrbc_int_t eo = mrbc_array_get(&positions, 1).i;

  if (so < 0) {
    mrbc_value empty = mrbc_string_new_cstr(vm, "");
    SET_RETURN(empty);
    return;
  }
  mrbc_value result = mrbc_string_new(vm,
    (const char *)str.string->data + so, (int)(eo - so));
  SET_RETURN(result);
}

/* MatchData#inspect -> String "#<MatchData \"match\" n:\"group\"...>" */
static void
c_match_data_inspect(mrbc_vm *vm, mrbc_value v[], int argc)
{
  mrbc_value positions = mrbc_instance_getiv(&v[0], SYM("positions"));
  mrbc_value str = mrbc_instance_getiv(&v[0], SYM("str"));
  mrbc_int_t nmatch = mrbc_instance_getiv(&v[0], SYM("nmatch")).i;
  mrbc_int_t so = mrbc_array_get(&positions, 0).i;
  mrbc_int_t eo = mrbc_array_get(&positions, 1).i;

  if (so < 0) {
    mrbc_value result = mrbc_string_new_cstr(vm, "#<MatchData (no match)>");
    SET_RETURN(result);
    return;
  }

  const char *str_data = (const char *)str.string->data;
  int match_len = (int)(eo - so);

  /* Build "#<MatchData \"match\">" possibly with group info */
  /* Estimate buffer size generously */
  int buf_size = 20 + match_len + (int)(nmatch - 1) * 32;
  char *buf = (char *)mrbc_alloc(vm, (size_t)buf_size);
  if (!buf) {
    mrbc_value empty = mrbc_string_new_cstr(vm, "#<MatchData>");
    SET_RETURN(empty);
    return;
  }

  int pos = 0;
  pos += snprintf(buf + pos, (size_t)(buf_size - pos), "#<MatchData \"");
  if (pos + match_len < buf_size) {
    memcpy(buf + pos, str_data + so, (size_t)match_len);
    pos += match_len;
  }
  pos += snprintf(buf + pos, (size_t)(buf_size - pos), "\"");

  for (mrbc_int_t i = 1; i < nmatch; i++) {
    mrbc_int_t gso = mrbc_array_get(&positions, (int)(i * 2)).i;
    mrbc_int_t geo = mrbc_array_get(&positions, (int)(i * 2 + 1)).i;
    pos += snprintf(buf + pos, (size_t)(buf_size - pos), " %d:", (int)i);
    if (gso < 0) {
      pos += snprintf(buf + pos, (size_t)(buf_size - pos), "nil");
    } else {
      int glen = (int)(geo - gso);
      pos += snprintf(buf + pos, (size_t)(buf_size - pos), "\"");
      if (pos + glen < buf_size) {
        memcpy(buf + pos, str_data + gso, (size_t)glen);
        pos += glen;
      }
      pos += snprintf(buf + pos, (size_t)(buf_size - pos), "\"");
    }
  }
  snprintf(buf + pos, (size_t)(buf_size - pos), ">");

  mrbc_value result = mrbc_string_new_cstr(vm, buf);
  mrbc_free(vm ,buf);
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
  regex_t *regex = (regex_t *)re.instance->data;
  regmatch_t pmatch[1];
  int r = regexec(regex, (const char *)v[0].string->data, 1, pmatch, 0);
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
  regex_t *regex = (regex_t *)re.instance->data;
  regmatch_t pmatch[1];
  int r = regexec(regex, (const char *)v[0].string->data, 1, pmatch, 0);
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
