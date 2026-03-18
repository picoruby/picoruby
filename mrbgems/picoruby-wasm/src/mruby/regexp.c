#include <emscripten.h>

#include "mruby.h"
#include "mruby/presym.h"
#include "mruby/class.h"
#include "mruby/data.h"
#include "mruby/string.h"
#include "mruby/variable.h"
#include "mruby/array.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Data structures */

typedef struct picorb_regexp {
  int ref_id;
} picorb_regexp;

typedef struct picorb_match_data {
  int ref_id;
  mrb_value str;
  mrb_value regexp;
} picorb_match_data;

static struct RClass *class_Regexp;
static struct RClass *class_MatchData;

/* Free functions */

static void
picorb_regexp_free(mrb_state *mrb, void *ptr)
{
  mrb_free(mrb, ptr);
}

static void
picorb_match_data_free(mrb_state *mrb, void *ptr)
{
  mrb_free(mrb, ptr);
}

static const struct mrb_data_type picorb_regexp_type = {
  "picorb_regexp", picorb_regexp_free
};

static const struct mrb_data_type picorb_match_data_type = {
  "picorb_match_data", picorb_match_data_free
};

/* EM_JS functions */

EM_JS(int, regexp_new, (const char *pattern, const char *flags), {
  try {
    var re = new RegExp(UTF8ToString(pattern), UTF8ToString(flags));
    return globalThis.picorubyRefs.push(re) - 1;
  } catch(e) {
    console.error('RegExp creation failed:', e);
    return -1;
  }
});

EM_JS(int, regexp_test, (int ref_id, const char *str), {
  try {
    var re = globalThis.picorubyRefs[ref_id];
    re.lastIndex = 0;
    return re.test(UTF8ToString(str)) ? 1 : 0;
  } catch(e) {
    return 0;
  }
});

EM_JS(int, regexp_exec, (int ref_id, const char *str), {
  try {
    var re = globalThis.picorubyRefs[ref_id];
    re.lastIndex = 0;
    var result = re.exec(UTF8ToString(str));
    if (result === null) return -1;
    return globalThis.picorubyRefs.push(result) - 1;
  } catch(e) {
    return -1;
  }
});

EM_JS(int, regexp_match_index, (int ref_id), {
  var result = globalThis.picorubyRefs[ref_id];
  return result.index;
});

EM_JS(int, regexp_match_length, (int ref_id), {
  var result = globalThis.picorubyRefs[ref_id];
  return result.length;
});

EM_JS(char *, regexp_match_item, (int ref_id, int idx), {
  var result = globalThis.picorubyRefs[ref_id];
  var item = result[idx];
  if (item === undefined) return 0;
  var len = lengthBytesUTF8(item) + 1;
  var ptr = _malloc(len);
  stringToUTF8(item, ptr, len);
  return ptr;
});

EM_JS(int, regexp_match_item_is_undefined, (int ref_id, int idx), {
  var result = globalThis.picorubyRefs[ref_id];
  return (result[idx] === undefined) ? 1 : 0;
});

EM_JS(char *, regexp_source, (int ref_id), {
  var re = globalThis.picorubyRefs[ref_id];
  var str = re.source;
  var len = lengthBytesUTF8(str) + 1;
  var ptr = _malloc(len);
  stringToUTF8(str, ptr, len);
  return ptr;
});

EM_JS(char *, regexp_flags, (int ref_id), {
  var re = globalThis.picorubyRefs[ref_id];
  var str = re.flags;
  var len = lengthBytesUTF8(str) + 1;
  var ptr = _malloc(len);
  stringToUTF8(str, ptr, len);
  return ptr;
});

/* Helper: build JS flags string from Ruby flags string */
static void
build_js_flags(const char *ruby_flags, mrb_int ruby_flags_len, char *js_flags, size_t js_flags_size)
{
  size_t j = 0;
  /* Always add JS 'm' flag (Ruby ^ and $ match line boundaries by default) */
  if (j < js_flags_size - 1) js_flags[j++] = 'm';

  mrb_int i = 0;
  while (i < ruby_flags_len && j < js_flags_size - 1) {
    switch (ruby_flags[i]) {
    case 'i':
      js_flags[j++] = 'i';
      break;
    case 'm':
      /* Ruby MULTILINE (. matches \n) = JS dotAll 's' */
      js_flags[j++] = 's';
      break;
    case 'x':
      /* JS has no extended mode; silently ignore */
      break;
    default:
      break;
    }
    i++;
  }
  js_flags[j] = '\0';
}

/* Helper: create a Regexp object with given ref_id */
static mrb_value
regexp_create_obj(mrb_state *mrb, int ref_id)
{
  picorb_regexp *data = (picorb_regexp *)mrb_malloc(mrb, sizeof(picorb_regexp));
  data->ref_id = ref_id;

  struct RData *rdata = mrb_data_object_alloc(mrb, class_Regexp, data, &picorb_regexp_type);
  return mrb_obj_value(rdata);
}

/* Helper: create a MatchData object */
static mrb_value
match_data_create_obj(mrb_state *mrb, int ref_id, mrb_value str, mrb_value regexp)
{
  picorb_match_data *data = (picorb_match_data *)mrb_malloc(mrb, sizeof(picorb_match_data));
  data->ref_id = ref_id;
  data->str = str;
  data->regexp = regexp;

  struct RData *rdata = mrb_data_object_alloc(mrb, class_MatchData, data, &picorb_match_data_type);
  return mrb_obj_value(rdata);
}

/* ---- Regexp class methods ---- */

/* Regexp.compile(pattern, flags=nil, encoding=nil) / Regexp.new */
static mrb_value
mrb_regexp_compile(mrb_state *mrb, mrb_value self)
{
  const char *pattern;
  mrb_int pattern_len;
  mrb_value flags_val = mrb_nil_value();
  mrb_value enc_val = mrb_nil_value();

  mrb_get_args(mrb, "s|oo", &pattern, &pattern_len, &flags_val, &enc_val);
  /* enc_val is accepted but ignored */

  char js_flags[16];
  if (mrb_string_p(flags_val)) {
    build_js_flags(RSTRING_PTR(flags_val), RSTRING_LEN(flags_val), js_flags, sizeof(js_flags));
  } else if (mrb_fixnum_p(flags_val)) {
    /* Integer flags: IGNORECASE=1, EXTENDED=2, MULTILINE=4 */
    mrb_int opts = mrb_fixnum(flags_val);
    char ruby_flags[4];
    int k = 0;
    if (opts & 1) ruby_flags[k++] = 'i';
    if (opts & 4) ruby_flags[k++] = 'm';
    ruby_flags[k] = '\0';
    build_js_flags(ruby_flags, k, js_flags, sizeof(js_flags));
  } else {
    build_js_flags("", 0, js_flags, sizeof(js_flags));
  }

  /* Create a null-terminated pattern string */
  char *pat_cstr = (char *)mrb_malloc(mrb, pattern_len + 1);
  memcpy(pat_cstr, pattern, pattern_len);
  pat_cstr[pattern_len] = '\0';

  int ref_id = regexp_new(pat_cstr, js_flags);
  mrb_free(mrb, pat_cstr);

  if (ref_id < 0) {
    mrb_raise(mrb, E_ARGUMENT_ERROR, "invalid regular expression");
  }

  return regexp_create_obj(mrb, ref_id);
}

/* Regexp#match(str) -> MatchData or nil */
static mrb_value
mrb_regexp_match(mrb_state *mrb, mrb_value self)
{
  mrb_value str_val;
  mrb_get_args(mrb, "S", &str_val);

  picorb_regexp *re = (picorb_regexp *)DATA_PTR(self);
  if (!re) {
    mrb_raise(mrb, E_RUNTIME_ERROR, "Regexp not initialized");
  }

  int match_ref = regexp_exec(re->ref_id, RSTRING_PTR(str_val));
  if (match_ref < 0) {
    return mrb_nil_value();
  }

  mrb_value frozen_str = mrb_str_new(mrb, RSTRING_PTR(str_val), RSTRING_LEN(str_val));
  mrb_obj_freeze(mrb, frozen_str);
  return match_data_create_obj(mrb, match_ref, frozen_str, self);
}

/* Regexp#match?(str) -> true/false */
static mrb_value
mrb_regexp_match_p(mrb_state *mrb, mrb_value self)
{
  mrb_value str_val;
  mrb_get_args(mrb, "S", &str_val);

  picorb_regexp *re = (picorb_regexp *)DATA_PTR(self);
  if (!re) {
    mrb_raise(mrb, E_RUNTIME_ERROR, "Regexp not initialized");
  }

  int result = regexp_test(re->ref_id, RSTRING_PTR(str_val));
  return mrb_bool_value(result);
}

/* Regexp#=~(str) -> Integer or nil */
static mrb_value
mrb_regexp_match_op(mrb_state *mrb, mrb_value self)
{
  mrb_value str_val;
  mrb_get_args(mrb, "S", &str_val);

  picorb_regexp *re = (picorb_regexp *)DATA_PTR(self);
  if (!re) {
    mrb_raise(mrb, E_RUNTIME_ERROR, "Regexp not initialized");
  }

  int match_ref = regexp_exec(re->ref_id, RSTRING_PTR(str_val));
  if (match_ref < 0) {
    return mrb_nil_value();
  }

  int idx = regexp_match_index(match_ref);
  return mrb_fixnum_value(idx);
}

/* Regexp#source -> String */
static mrb_value
mrb_regexp_source(mrb_state *mrb, mrb_value self)
{
  picorb_regexp *re = (picorb_regexp *)DATA_PTR(self);
  if (!re) {
    mrb_raise(mrb, E_RUNTIME_ERROR, "Regexp not initialized");
  }

  char *src = regexp_source(re->ref_id);
  if (!src) return mrb_str_new_lit(mrb, "");
  mrb_value str = mrb_str_new_cstr(mrb, src);
  free(src);
  return str;
}

/* Regexp#to_s -> "(?flags:source)" */
static mrb_value
mrb_regexp_to_s(mrb_state *mrb, mrb_value self)
{
  picorb_regexp *re = (picorb_regexp *)DATA_PTR(self);
  if (!re) {
    mrb_raise(mrb, E_RUNTIME_ERROR, "Regexp not initialized");
  }

  char *src = regexp_source(re->ref_id);
  char *flags = regexp_flags(re->ref_id);
  if (!src) src = strdup("");
  if (!flags) flags = strdup("");

  /* Convert JS flags back to Ruby flags for display */
  char ruby_flags[8];
  int j = 0;
  size_t i = 0;
  while (i < strlen(flags)) {
    switch (flags[i]) {
    case 'i': ruby_flags[j++] = 'i'; break;
    case 's': ruby_flags[j++] = 'm'; break;
    /* JS 'm' is implicit in Ruby; skip */
    default: break;
    }
    i++;
  }
  ruby_flags[j] = '\0';

  /* "(?flags:source)" */
  mrb_value result = mrb_str_new_lit(mrb, "(?");
  mrb_str_cat_cstr(mrb, result, ruby_flags);
  mrb_str_cat_lit(mrb, result, ":");
  mrb_str_cat_cstr(mrb, result, src);
  mrb_str_cat_lit(mrb, result, ")");

  free(src);
  free(flags);
  return result;
}

/* Regexp#inspect -> "/source/flags" */
static mrb_value
mrb_regexp_inspect(mrb_state *mrb, mrb_value self)
{
  picorb_regexp *re = (picorb_regexp *)DATA_PTR(self);
  if (!re) {
    mrb_raise(mrb, E_RUNTIME_ERROR, "Regexp not initialized");
  }

  char *src = regexp_source(re->ref_id);
  char *flags = regexp_flags(re->ref_id);
  if (!src) src = strdup("");
  if (!flags) flags = strdup("");

  char ruby_flags[8];
  int j = 0;
  size_t i = 0;
  while (i < strlen(flags)) {
    switch (flags[i]) {
    case 'i': ruby_flags[j++] = 'i'; break;
    case 's': ruby_flags[j++] = 'm'; break;
    default: break;
    }
    i++;
  }
  ruby_flags[j] = '\0';

  mrb_value result = mrb_str_new_lit(mrb, "/");
  mrb_str_cat_cstr(mrb, result, src);
  mrb_str_cat_lit(mrb, result, "/");
  mrb_str_cat_cstr(mrb, result, ruby_flags);

  free(src);
  free(flags);
  return result;
}

/* Regexp#casefold? -> true/false */
static mrb_value
mrb_regexp_casefold_p(mrb_state *mrb, mrb_value self)
{
  picorb_regexp *re = (picorb_regexp *)DATA_PTR(self);
  if (!re) {
    mrb_raise(mrb, E_RUNTIME_ERROR, "Regexp not initialized");
  }

  char *flags = regexp_flags(re->ref_id);
  if (!flags) return mrb_false_value();

  int casefold = 0;
  size_t i = 0;
  while (i < strlen(flags)) {
    if (flags[i] == 'i') { casefold = 1; break; }
    i++;
  }
  free(flags);
  return mrb_bool_value(casefold);
}

/* Regexp#options -> Integer */
static mrb_value
mrb_regexp_options(mrb_state *mrb, mrb_value self)
{
  picorb_regexp *re = (picorb_regexp *)DATA_PTR(self);
  if (!re) {
    mrb_raise(mrb, E_RUNTIME_ERROR, "Regexp not initialized");
  }

  char *flags = regexp_flags(re->ref_id);
  if (!flags) return mrb_fixnum_value(0);

  int opts = 0;
  size_t i = 0;
  while (i < strlen(flags)) {
    switch (flags[i]) {
    case 'i': opts |= 1; break; /* IGNORECASE */
    case 's': opts |= 4; break; /* JS dotAll = Ruby MULTILINE */
    default: break;
    }
    i++;
  }
  free(flags);
  return mrb_fixnum_value(opts);
}

/* ---- MatchData class methods ---- */

/* MatchData#[](idx) -> String or nil */
static mrb_value
mrb_match_data_aref(mrb_state *mrb, mrb_value self)
{
  mrb_int idx;
  mrb_get_args(mrb, "i", &idx);

  picorb_match_data *md = (picorb_match_data *)DATA_PTR(self);
  if (!md) {
    mrb_raise(mrb, E_RUNTIME_ERROR, "MatchData not initialized");
  }

  int len = regexp_match_length(md->ref_id);
  if (idx < 0) idx += len;
  if (idx < 0 || len <= idx) return mrb_nil_value();

  if (regexp_match_item_is_undefined(md->ref_id, (int)idx)) {
    return mrb_nil_value();
  }

  char *item = regexp_match_item(md->ref_id, (int)idx);
  if (!item) return mrb_nil_value();
  mrb_value str = mrb_str_new_cstr(mrb, item);
  free(item);
  return str;
}

/* MatchData#to_a -> Array */
static mrb_value
mrb_match_data_to_a(mrb_state *mrb, mrb_value self)
{
  picorb_match_data *md = (picorb_match_data *)DATA_PTR(self);
  if (!md) {
    mrb_raise(mrb, E_RUNTIME_ERROR, "MatchData not initialized");
  }

  int len = regexp_match_length(md->ref_id);
  mrb_value ary = mrb_ary_new_capa(mrb, len);

  int i = 0;
  while (i < len) {
    if (regexp_match_item_is_undefined(md->ref_id, i)) {
      mrb_ary_push(mrb, ary, mrb_nil_value());
    } else {
      char *item = regexp_match_item(md->ref_id, i);
      if (item) {
        mrb_ary_push(mrb, ary, mrb_str_new_cstr(mrb, item));
        free(item);
      } else {
        mrb_ary_push(mrb, ary, mrb_nil_value());
      }
    }
    i++;
  }
  return ary;
}

/* MatchData#length / MatchData#size -> Integer */
static mrb_value
mrb_match_data_length(mrb_state *mrb, mrb_value self)
{
  picorb_match_data *md = (picorb_match_data *)DATA_PTR(self);
  if (!md) {
    mrb_raise(mrb, E_RUNTIME_ERROR, "MatchData not initialized");
  }

  return mrb_fixnum_value(regexp_match_length(md->ref_id));
}

/* MatchData#string -> String (frozen) */
static mrb_value
mrb_match_data_string(mrb_state *mrb, mrb_value self)
{
  picorb_match_data *md = (picorb_match_data *)DATA_PTR(self);
  if (!md) {
    mrb_raise(mrb, E_RUNTIME_ERROR, "MatchData not initialized");
  }

  return md->str;
}

/* MatchData#regexp -> Regexp */
static mrb_value
mrb_match_data_regexp(mrb_state *mrb, mrb_value self)
{
  picorb_match_data *md = (picorb_match_data *)DATA_PTR(self);
  if (!md) {
    mrb_raise(mrb, E_RUNTIME_ERROR, "MatchData not initialized");
  }

  return md->regexp;
}

/* MatchData#pre_match -> String */
static mrb_value
mrb_match_data_pre_match(mrb_state *mrb, mrb_value self)
{
  picorb_match_data *md = (picorb_match_data *)DATA_PTR(self);
  if (!md) {
    mrb_raise(mrb, E_RUNTIME_ERROR, "MatchData not initialized");
  }

  int match_idx = regexp_match_index(md->ref_id);
  return mrb_str_new(mrb, RSTRING_PTR(md->str), match_idx);
}

/* MatchData#post_match -> String */
static mrb_value
mrb_match_data_post_match(mrb_state *mrb, mrb_value self)
{
  picorb_match_data *md = (picorb_match_data *)DATA_PTR(self);
  if (!md) {
    mrb_raise(mrb, E_RUNTIME_ERROR, "MatchData not initialized");
  }

  int match_idx = regexp_match_index(md->ref_id);
  char *full_match = regexp_match_item(md->ref_id, 0);
  if (!full_match) return mrb_str_new_lit(mrb, "");

  int full_len = (int)strlen(full_match);
  free(full_match);

  int post_start = match_idx + full_len;
  int str_len = RSTRING_LEN(md->str);
  if (str_len <= post_start) return mrb_str_new_lit(mrb, "");

  return mrb_str_new(mrb, RSTRING_PTR(md->str) + post_start, str_len - post_start);
}

/* MatchData#begin(idx) -> Integer */
static mrb_value
mrb_match_data_begin(mrb_state *mrb, mrb_value self)
{
  mrb_int idx;
  mrb_get_args(mrb, "i", &idx);

  picorb_match_data *md = (picorb_match_data *)DATA_PTR(self);
  if (!md) {
    mrb_raise(mrb, E_RUNTIME_ERROR, "MatchData not initialized");
  }

  if (idx == 0) {
    return mrb_fixnum_value(regexp_match_index(md->ref_id));
  }

  /* For captures (idx > 0), compute position by searching in original string */
  int match_start = regexp_match_index(md->ref_id);
  char *full_match = regexp_match_item(md->ref_id, 0);
  if (!full_match) return mrb_nil_value();

  if (regexp_match_item_is_undefined(md->ref_id, (int)idx)) {
    free(full_match);
    return mrb_nil_value();
  }

  char *capture = regexp_match_item(md->ref_id, (int)idx);
  if (!capture) {
    free(full_match);
    return mrb_nil_value();
  }

  /* Find capture within the original string starting from match_start */
  const char *str_ptr = RSTRING_PTR(md->str) + match_start;
  const char *found = strstr(str_ptr, capture);
  int result;
  if (found) {
    result = (int)(found - RSTRING_PTR(md->str));
  } else {
    result = -1;
  }

  free(full_match);
  free(capture);
  if (result < 0) return mrb_nil_value();
  return mrb_fixnum_value(result);
}

/* MatchData#end(idx) -> Integer */
static mrb_value
mrb_match_data_end(mrb_state *mrb, mrb_value self)
{
  mrb_int idx;
  mrb_get_args(mrb, "i", &idx);

  picorb_match_data *md = (picorb_match_data *)DATA_PTR(self);
  if (!md) {
    mrb_raise(mrb, E_RUNTIME_ERROR, "MatchData not initialized");
  }

  if (idx == 0) {
    int start = regexp_match_index(md->ref_id);
    char *full_match = regexp_match_item(md->ref_id, 0);
    if (!full_match) return mrb_nil_value();
    int end_pos = start + (int)strlen(full_match);
    free(full_match);
    return mrb_fixnum_value(end_pos);
  }

  if (regexp_match_item_is_undefined(md->ref_id, (int)idx)) {
    return mrb_nil_value();
  }

  char *capture = regexp_match_item(md->ref_id, (int)idx);
  if (!capture) return mrb_nil_value();

  int match_start = regexp_match_index(md->ref_id);
  const char *str_ptr = RSTRING_PTR(md->str) + match_start;
  const char *found = strstr(str_ptr, capture);
  int result;
  if (found) {
    result = (int)(found - RSTRING_PTR(md->str)) + (int)strlen(capture);
  } else {
    result = -1;
  }

  free(capture);
  if (result < 0) return mrb_nil_value();
  return mrb_fixnum_value(result);
}

/* MatchData#captures -> Array (excluding [0]) */
static mrb_value
mrb_match_data_captures(mrb_state *mrb, mrb_value self)
{
  picorb_match_data *md = (picorb_match_data *)DATA_PTR(self);
  if (!md) {
    mrb_raise(mrb, E_RUNTIME_ERROR, "MatchData not initialized");
  }

  int len = regexp_match_length(md->ref_id);
  mrb_value ary = mrb_ary_new_capa(mrb, len - 1);

  int i = 1;
  while (i < len) {
    if (regexp_match_item_is_undefined(md->ref_id, i)) {
      mrb_ary_push(mrb, ary, mrb_nil_value());
    } else {
      char *item = regexp_match_item(md->ref_id, i);
      if (item) {
        mrb_ary_push(mrb, ary, mrb_str_new_cstr(mrb, item));
        free(item);
      } else {
        mrb_ary_push(mrb, ary, mrb_nil_value());
      }
    }
    i++;
  }
  return ary;
}

/* MatchData#to_s -> String (full match) */
static mrb_value
mrb_match_data_to_s(mrb_state *mrb, mrb_value self)
{
  picorb_match_data *md = (picorb_match_data *)DATA_PTR(self);
  if (!md) {
    mrb_raise(mrb, E_RUNTIME_ERROR, "MatchData not initialized");
  }

  char *item = regexp_match_item(md->ref_id, 0);
  if (!item) return mrb_str_new_lit(mrb, "");
  mrb_value str = mrb_str_new_cstr(mrb, item);
  free(item);
  return str;
}

/* MatchData#inspect */
static mrb_value
mrb_match_data_inspect(mrb_state *mrb, mrb_value self)
{
  picorb_match_data *md = (picorb_match_data *)DATA_PTR(self);
  if (!md) {
    mrb_raise(mrb, E_RUNTIME_ERROR, "MatchData not initialized");
  }

  mrb_value result = mrb_str_new_lit(mrb, "#<MatchData");
  int len = regexp_match_length(md->ref_id);

  int i = 0;
  while (i < len) {
    if (i == 0) {
      mrb_str_cat_lit(mrb, result, " ");
    } else {
      char buf[32];
      snprintf(buf, sizeof(buf), " %d:", i);
      mrb_str_cat_cstr(mrb, result, buf);
    }

    if (regexp_match_item_is_undefined(md->ref_id, i)) {
      mrb_str_cat_lit(mrb, result, "nil");
    } else {
      char *item = regexp_match_item(md->ref_id, i);
      if (item) {
        mrb_str_cat_lit(mrb, result, "\"");
        mrb_str_cat_cstr(mrb, result, item);
        mrb_str_cat_lit(mrb, result, "\"");
        free(item);
      } else {
        mrb_str_cat_lit(mrb, result, "nil");
      }
    }
    i++;
  }
  mrb_str_cat_lit(mrb, result, ">");
  return result;
}

/* ---- String extension methods ---- */

/* Helper: get or create Regexp from value */
static mrb_value
ensure_regexp(mrb_state *mrb, mrb_value pattern)
{
  if (mrb_obj_is_instance_of(mrb, pattern, class_Regexp)) {
    return pattern;
  }
  if (mrb_string_p(pattern)) {
    /* Create Regexp from string */
    char js_flags[16];
    build_js_flags("", 0, js_flags, sizeof(js_flags));
    int ref_id = regexp_new(RSTRING_PTR(pattern), js_flags);
    if (ref_id < 0) {
      mrb_raise(mrb, E_ARGUMENT_ERROR, "invalid regular expression");
    }
    return regexp_create_obj(mrb, ref_id);
  }
  mrb_raise(mrb, E_TYPE_ERROR, "wrong argument type (expected Regexp or String)");
  return mrb_nil_value(); /* not reached */
}

/* String#match(regexp_or_str) -> MatchData or nil */
static mrb_value
mrb_string_match(mrb_state *mrb, mrb_value self)
{
  mrb_value pattern;
  mrb_get_args(mrb, "o", &pattern);

  mrb_value re = ensure_regexp(mrb, pattern);
  picorb_regexp *re_data = (picorb_regexp *)DATA_PTR(re);

  int match_ref = regexp_exec(re_data->ref_id, RSTRING_PTR(self));
  if (match_ref < 0) {
    return mrb_nil_value();
  }

  mrb_value frozen_str = mrb_str_new(mrb, RSTRING_PTR(self), RSTRING_LEN(self));
  mrb_obj_freeze(mrb, frozen_str);
  return match_data_create_obj(mrb, match_ref, frozen_str, re);
}

/* String#match?(regexp_or_str) -> true/false */
static mrb_value
mrb_string_match_p(mrb_state *mrb, mrb_value self)
{
  mrb_value pattern;
  mrb_get_args(mrb, "o", &pattern);

  mrb_value re = ensure_regexp(mrb, pattern);
  picorb_regexp *re_data = (picorb_regexp *)DATA_PTR(re);

  int result = regexp_test(re_data->ref_id, RSTRING_PTR(self));
  return mrb_bool_value(result);
}

/* String#=~(regexp) -> Integer or nil */
static mrb_value
mrb_string_match_op(mrb_state *mrb, mrb_value self)
{
  mrb_value pattern;
  mrb_get_args(mrb, "o", &pattern);

  mrb_value re = ensure_regexp(mrb, pattern);
  picorb_regexp *re_data = (picorb_regexp *)DATA_PTR(re);

  int match_ref = regexp_exec(re_data->ref_id, RSTRING_PTR(self));
  if (match_ref < 0) {
    return mrb_nil_value();
  }

  int idx = regexp_match_index(match_ref);
  return mrb_fixnum_value(idx);
}

/* ---- Init function ---- */

void
mrb_regexp_init(mrb_state *mrb)
{
  /* Regexp class */
  class_Regexp = mrb_define_class_id(mrb, MRB_SYM(Regexp), mrb->object_class);
  MRB_SET_INSTANCE_TT(class_Regexp, MRB_TT_DATA);

  mrb_define_class_method_id(mrb, class_Regexp, MRB_SYM(compile), mrb_regexp_compile, MRB_ARGS_ARG(1, 2));
  mrb_define_class_method_id(mrb, class_Regexp, MRB_SYM(new), mrb_regexp_compile, MRB_ARGS_ARG(1, 2));

  mrb_define_method_id(mrb, class_Regexp, MRB_SYM(match), mrb_regexp_match, MRB_ARGS_REQ(1));
  mrb_define_method_id(mrb, class_Regexp, MRB_SYM_Q(match), mrb_regexp_match_p, MRB_ARGS_REQ(1));
  mrb_define_method_id(mrb, class_Regexp, mrb_intern_lit(mrb, "=~"), mrb_regexp_match_op, MRB_ARGS_REQ(1));
  mrb_define_method_id(mrb, class_Regexp, MRB_SYM(source), mrb_regexp_source, MRB_ARGS_NONE());
  mrb_define_method_id(mrb, class_Regexp, MRB_SYM(to_s), mrb_regexp_to_s, MRB_ARGS_NONE());
  mrb_define_method_id(mrb, class_Regexp, MRB_SYM(inspect), mrb_regexp_inspect, MRB_ARGS_NONE());
  mrb_define_method_id(mrb, class_Regexp, MRB_SYM_Q(casefold), mrb_regexp_casefold_p, MRB_ARGS_NONE());
  mrb_define_method_id(mrb, class_Regexp, MRB_SYM(options), mrb_regexp_options, MRB_ARGS_NONE());

  mrb_define_const_id(mrb, class_Regexp, MRB_SYM(IGNORECASE), mrb_fixnum_value(1));
  mrb_define_const_id(mrb, class_Regexp, MRB_SYM(EXTENDED), mrb_fixnum_value(2));
  mrb_define_const_id(mrb, class_Regexp, MRB_SYM(MULTILINE), mrb_fixnum_value(4));

  /* MatchData class */
  class_MatchData = mrb_define_class_id(mrb, MRB_SYM(MatchData), mrb->object_class);
  MRB_SET_INSTANCE_TT(class_MatchData, MRB_TT_DATA);

  mrb_define_method_id(mrb, class_MatchData, MRB_OPSYM(aref), mrb_match_data_aref, MRB_ARGS_REQ(1));
  mrb_define_method_id(mrb, class_MatchData, MRB_SYM(to_a), mrb_match_data_to_a, MRB_ARGS_NONE());
  mrb_define_method_id(mrb, class_MatchData, MRB_SYM(length), mrb_match_data_length, MRB_ARGS_NONE());
  mrb_define_method_id(mrb, class_MatchData, MRB_SYM(size), mrb_match_data_length, MRB_ARGS_NONE());
  mrb_define_method_id(mrb, class_MatchData, MRB_SYM(string), mrb_match_data_string, MRB_ARGS_NONE());
  mrb_define_method_id(mrb, class_MatchData, MRB_SYM(regexp), mrb_match_data_regexp, MRB_ARGS_NONE());
  mrb_define_method_id(mrb, class_MatchData, MRB_SYM(pre_match), mrb_match_data_pre_match, MRB_ARGS_NONE());
  mrb_define_method_id(mrb, class_MatchData, MRB_SYM(post_match), mrb_match_data_post_match, MRB_ARGS_NONE());
  mrb_define_method_id(mrb, class_MatchData, MRB_SYM(begin), mrb_match_data_begin, MRB_ARGS_REQ(1));
  mrb_define_method_id(mrb, class_MatchData, MRB_SYM(end), mrb_match_data_end, MRB_ARGS_REQ(1));
  mrb_define_method_id(mrb, class_MatchData, MRB_SYM(captures), mrb_match_data_captures, MRB_ARGS_NONE());
  mrb_define_method_id(mrb, class_MatchData, MRB_SYM(to_s), mrb_match_data_to_s, MRB_ARGS_NONE());
  mrb_define_method_id(mrb, class_MatchData, MRB_SYM(inspect), mrb_match_data_inspect, MRB_ARGS_NONE());

  /* String extensions */
  struct RClass *string_class = mrb->string_class;
  mrb_define_method_id(mrb, string_class, MRB_SYM(match), mrb_string_match, MRB_ARGS_REQ(1));
  mrb_define_method_id(mrb, string_class, MRB_SYM_Q(match), mrb_string_match_p, MRB_ARGS_REQ(1));
  mrb_define_method_id(mrb, string_class, mrb_intern_lit(mrb, "=~"), mrb_string_match_op, MRB_ARGS_REQ(1));
}
