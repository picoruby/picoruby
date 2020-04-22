#include <stdbool.h>
#include <string.h>
#include "regex_light.h"

#include "common.h"
#define  REGEX_ALLOC(size)  mmrbc_alloc(size)
#define  REGEX_FREE(ptr)    mmrbc_free(ptr)


typedef enum {
  RE_TYPE_TERM,     // sentinel of finishing expression
  RE_TYPE_LIT,      // literal
  RE_TYPE_DOT,      // .
  RE_TYPE_QUESTION, // ?
  RE_TYPE_STAR,     // *
  RE_TYPE_PLUS,     // +
  RE_TYPE_BEGIN,    // ^
  RE_TYPE_END,      // $
  RE_TYPE_BRACKET,  // [ ]
  RE_TYPE_LPAREN,   // (
  RE_TYPE_RPAREN,   // )
} ReType;

/*
 * An element of Regular Expression Tree
 */
typedef struct re_atom {
  ReType type;
  union {
    unsigned char ch;   // literal in RE_TYPE_LIT
    unsigned char *ccl; // pointer to content in [ ] RE_TYPE_BRACKET
  };
} ReAtom;

typedef struct re_state {
  char *original_text_top_addr;
  int current_re_nsub;
  int max_re_nsub;
  signed char *match_index_data;
} ReState;

static int match(ReState rs, ReAtom **regexp, const char *text);
static int matchstar(ReState rs, ReAtom c, ReAtom **regexp, const char *text);
static int matchhere(ReState rs, ReAtom **regexp, const char *text);
static int matchone(ReState rs, ReAtom p, const char *text);
static int matchquestion(ReState rs, ReAtom **regexp, const char *text);
static int matchchars(ReState rs, const unsigned char *s, const char *text);
static int matchbetween(ReState rs, const unsigned char *s, const char *text);

/*
 * constractor of ReAtom
 */
static ReAtom
*re_atom_new(ReType type)
{
  ReAtom *atom = REGEX_ALLOC(sizeof(ReAtom));
  atom->type = type;
  return atom;
}

/*
 * report nsub
 */
static void
re_report_nsub(ReState rs, const char *text)
{
  rs.match_index_data[(int)((long)&text[0] - (long)&rs.original_text_top_addr[0])] = rs.current_re_nsub;
}
#define REPORT_WITHOUT_RETURN (re_report_nsub(rs, text))
#define REPORT \
  do { \
    re_report_nsub(rs, text); \
    return 1; \
  } while (0)
/*
 * matcher functions
 */
static int
//matchone(ReAtom p, int c)
matchone(ReState rs, ReAtom p, const char *text)
{
  if ((p.type == RE_TYPE_LIT && p.ch == *text) || (p.type == RE_TYPE_DOT))
    REPORT;
  if (p.type == RE_TYPE_BRACKET) return matchchars(rs, p.ccl, text);
  return 0;
}

static int
matchquestion(ReState rs, ReAtom **regexp, const char *text)
{
  if ((matchone(rs, *regexp[0], text) && matchhere(rs, (regexp + 2), text + 1))
      || matchhere(rs, (regexp + 2), text)) {
    return 1; // do not REPORT
  } else {
    return 0;
  }
}

/* matchhere: search for regexp at beginning of text */
static int
matchhere(ReState rs, ReAtom **regexp, const char *text)
{
  do {
    if (regexp[0]->type == RE_TYPE_TERM)
      return 1; // do not REPORT;
    if (regexp[0]->type == RE_TYPE_LPAREN) {
      rs.max_re_nsub++;
      rs.current_re_nsub = rs.max_re_nsub;
      return matchhere(rs, (regexp + 1), text);
    }
    if (regexp[0]->type == RE_TYPE_RPAREN) {
      rs.current_re_nsub--;
      return matchhere(rs, (regexp + 1), text);
    }
    if ((regexp + 1)[0]->type == RE_TYPE_QUESTION)
      return matchquestion(rs, regexp, text);
    if ((regexp + 1)[0]->type == RE_TYPE_STAR)
      return matchstar(rs, *regexp[0], (regexp + 2), text);
    if ((regexp + 1)[0]->type == RE_TYPE_PLUS)
      return matchone(rs, *regexp[0], text) && matchstar(rs, *regexp[0], (regexp + 2), text + 1);
    if (regexp[0]->type == RE_TYPE_END && (regexp + 1)[0]->type == RE_TYPE_TERM)
      return *text == '\0';
    if (*text != '\0' && (regexp[0]->type == RE_TYPE_DOT || (regexp[0]->type == RE_TYPE_LIT && regexp[0]->ch == *text))) {
      REPORT_WITHOUT_RETURN;
      return matchhere(rs, (regexp + 1), text + 1);
    }
  } while (text[0] != '\0' && matchone(rs, **regexp++, text++));
  return 0;
}

static int
matchstar(ReState rs, ReAtom c, ReAtom **regexp, const char *text_)
{
  char *text;
  /* leftmost && longest */
  for (text = (char *)text_;
       *text != '\0' && (
         (c.type == RE_TYPE_LIT && *text == c.ch) ||
         (c.type == RE_TYPE_BRACKET && matchchars(rs, c.ccl, text)) ||
         c.type == RE_TYPE_DOT
       );
       text++)
    REPORT_WITHOUT_RETURN;
  do {  /* * matches zero or more */
    if (matchhere(rs, regexp, text))
      return 1; //REPORT;
  } while (text-- > text_);
  return 0;
}

static int
matchbetween(ReState rs, const unsigned char* s, const char *text)
{
  if ((text[0] != '-') && (s[0] != '\0') && (s[0] != '-') &&
      (s[1] == '-') && (s[1] != '\0') &&
      (s[2] != '\0') && ((text[0] >= s[0]) && (text[0] <= s[2]))) {
    REPORT;
  } else {
    return 0;
  }
}

static int
matchchars(ReState rs, const unsigned char* s, const char *text)
{
  do {
    if (matchbetween(rs, s, text)) {
      REPORT;
    } else if (s[0] == '\\') {
      s += 1;
      if (text[0] == s[0]) {
        REPORT;
      }
    } else if (text[0] == s[0]) {
      if (text[0] == '-') {
        return (s[-1] == '\0') || (s[1] == '\0');
      } else {
        REPORT;
      }
    }
  } while (*s++ != '\0');
  return 0;
}

static int
match(ReState rs, ReAtom **regexp, const char *text)
{
  if (regexp[0]->type == RE_TYPE_BEGIN)
    return (matchhere(rs, (regexp + 1), text));
  do {    /* must look even if string is empty */
    if (matchhere(rs, regexp, text))
      REPORT;
  } while (*text++ != '\0');
  return 0;
}

void
set_match_data(ReState rs, size_t nmatch, regmatch_t pmatch[], size_t len)
{
  int i;
  for (i = 0; i < nmatch; i++) {
    pmatch[i].rm_so = -1;
    pmatch[i].rm_eo = -1;
  }
  bool scanning = false;
  for (i = len - 1; i > -1; i--) {
    if (rs.match_index_data[i] < 0) {
      if (scanning) break;
      continue;
    } else {
      scanning = true;
    }
    if (pmatch[0].rm_eo < 0) pmatch[0].rm_eo = i + 1;
    if (pmatch[(int)rs.match_index_data[i]].rm_eo < 0) pmatch[(int)rs.match_index_data[i]].rm_eo = i + 1;
    pmatch[(int)rs.match_index_data[i]].rm_so = i;
  }
  pmatch[0].rm_so = i + 1;
}

/*
 * public functions
 */
int
regexec(regex_t *preg, const char *text, size_t nmatch, regmatch_t pmatch[], int _eflags)
{
  ReState rs;
  rs.original_text_top_addr = (void *)text;
  rs.current_re_nsub = 0;
  rs.max_re_nsub = 0;
  size_t len = strlen(text);
  rs.match_index_data = REGEX_ALLOC(len);
  memset(rs.match_index_data, -1, len);
  if (match(rs, preg->atoms, text)) {
    set_match_data(rs, nmatch, pmatch, len);
    REGEX_FREE(rs.match_index_data);
    return 0; /* success */
  } else {
    REGEX_FREE(rs.match_index_data);
    return -1; /* to be correct, it should be a thing like REG_NOMATCH */
  }
}

/*
 * compile regular expression pattern
 * _cflags is dummy
 */
#define REGEX_DEF_w "a-zA-Z0-9_"
#define REGEX_DEF_s " \t\f\r\n"
#define REGEX_DEF_d "0-9"
int
regcomp(regex_t *preg, const char *pattern, int _cflags)
{
  for (int i = 0; i < MAX_ATOM_SIZE; i++)
    preg->atoms[i] = NULL; /* initialize */
  preg->re_nsub = 0;
  char c; // current char in pattern
  int i = 0; // current position in pattern
  int j = 0; // index of atom
  unsigned char *ccl;
  while (pattern[i] != '\0') {
    c = pattern[i];
    switch (c) {
      case '.':
        preg->atoms[j] = re_atom_new(RE_TYPE_DOT);
        break;
      case '?':
        preg->atoms[j] = re_atom_new(RE_TYPE_QUESTION);
        break;
      case '*':
        preg->atoms[j] = re_atom_new(RE_TYPE_STAR);
        break;
      case '+':
        preg->atoms[j] = re_atom_new(RE_TYPE_PLUS);
        break;
      case '^':
        preg->atoms[j] = re_atom_new(RE_TYPE_BEGIN);
        break;
      case '$':
        preg->atoms[j] = re_atom_new(RE_TYPE_END);
        break;
      case '(':
        preg->atoms[j] = re_atom_new(RE_TYPE_LPAREN);
        preg->re_nsub++;
        break;
      case ')':
        preg->atoms[j] = re_atom_new(RE_TYPE_RPAREN);
        break;
      case '\\':
        switch (pattern[i + 1]) {
          case '\0':
            preg->atoms[j] = re_atom_new(RE_TYPE_LIT);
            preg->atoms[j]->ch = '\\';
            break;
          case 'w':
            i++;
            preg->atoms[j] = re_atom_new(RE_TYPE_BRACKET);
            ccl = REGEX_ALLOC(strlen(REGEX_DEF_w) + 1);
            memcpy(ccl, REGEX_DEF_w, strlen(REGEX_DEF_w));
            ccl[strlen(REGEX_DEF_w)] = '\0';
            preg->atoms[j]->ccl = ccl;
            break;
          case 's':
            i++;
            preg->atoms[j] = re_atom_new(RE_TYPE_BRACKET);
            ccl = REGEX_ALLOC(strlen(REGEX_DEF_s) + 1);
            memcpy(ccl, REGEX_DEF_s, strlen(REGEX_DEF_s));
            ccl[strlen(REGEX_DEF_s)] = '\0';
            preg->atoms[j]->ccl = ccl;
            break;
          case 'd':
            i++;
            preg->atoms[j] = re_atom_new(RE_TYPE_BRACKET);
            ccl = REGEX_ALLOC(strlen(REGEX_DEF_d) + 1);
            memcpy(ccl, REGEX_DEF_d, strlen(REGEX_DEF_d));
            ccl[strlen(REGEX_DEF_d)] = '\0';
            preg->atoms[j]->ccl = ccl;
            break;
          default:
            i++;
            preg->atoms[j] = re_atom_new(RE_TYPE_LIT);
            preg->atoms[j]->ch = pattern[i];
        }
        break;
      case '[':
        i++;
        int len;
         /*
          * pattern [] must contain at least one letter.
          * first letter of the content should be ']' if you want to match literal ']'
          */
        for (len = 1;
            pattern[i + len] != '\0' && (pattern[i + len] != ']');
            len++)
          ;
        ccl = REGEX_ALLOC(len + 1); // possibly longer than actual
        memcpy(ccl, pattern + i, len);
        ccl[len] = '\0';
        preg->atoms[j] = re_atom_new(RE_TYPE_BRACKET);
        preg->atoms[j]->ccl = ccl;
        i += len;
        break;
      default:
        preg->atoms[j] = re_atom_new(RE_TYPE_LIT);
        preg->atoms[j]->ch = c;
        break;
    }
    i++;
    j++;
    if (j >= MAX_ATOM_SIZE) return 1;
  }
  preg->atoms[j] = re_atom_new(RE_TYPE_TERM);
  return 0;
}

/*
 * free regex_t object
 */
void
regfree(regex_t *preg)
{
  for (int i = 0; i < MAX_ATOM_SIZE; i++) {
    if (preg->atoms[i] == NULL) break;
    if (preg->atoms[i]->type == RE_TYPE_BRACKET) REGEX_FREE(preg->atoms[i]->ccl);
    REGEX_FREE(preg->atoms[i]);
  }
}
