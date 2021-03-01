#ifndef REGEX_LIGHT_H_
#define REGEX_LIGHT_H_

#include <stdint.h>

typedef struct re_atom ReAtom;

typedef struct {
  size_t re_nsub;  // number of parenthesized subexpressions ( )
  ReAtom *atoms;
} regex_t;

typedef struct {
  int16_t rm_so; // start position of match
  int16_t rm_eo; // end position of match
} regmatch_t;

/* regcomp() flags */
#define	REG_BASIC       0000
#define	REG_EXTENDED    0001
#define	REG_ICASE       0002
#define	REG_NOSUB       0004
#define	REG_NEWLINE     0010
#define	REG_NOSPEC      0020
#define	REG_PEND        0040
#define	REG_DUMP        0200

int regcomp(regex_t *preg, const char *pattern, int _cflags);
void regfree(regex_t *preg);
int regexec(regex_t *preg, const char *string, size_t nmatch, regmatch_t *pmatch, int eflags);

#ifndef REGEX_USE_ALLOC_LIBC
void RegexSetAllocProcs(void *(*mallocProc)(size_t), void (*freeProc)(void *));
#endif

#endif /* !REGEX_LIGHT_H_ */
