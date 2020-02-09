#ifndef MMRBC_TOKENIZER_H_
#define MMRBC_TOKENIZER_H_

#include <stdbool.h>

#include "token.h"

typedef enum mode
{
  MODE_NONE,
  MODE_COMMENT,
  qwords
} Mode;

typedef enum paren
{
  PAREN_NONE,
  PAREN_brace,
} Paren;

typedef struct tokenizer
{
  Mode mode;
  char* line;
  int line_num;
  int pos;
  int paren_stack_num;
  Paren paren_stack[];
} Tokenizer;

void Tokenizer_new(Tokenizer* const self);

void Tokenizer_puts(Tokenizer* const self, char *line);

bool Tokenizer_hasMoreTokens(Tokenizer* const self);

void Tokenizer_advance(Tokenizer* const self, bool recursive);

static struct {
  char *string;
} KEYWORDS[] = {
  {"BEGIN"},
  {"class"},
  {"ensure"},
  {"nil"},
  {"self"},
  {"when"},
  {"END"},
  {"def"},
  {"false"},
  {"not"},
  {"super"},
  {"while"},
  {"alias"},
  {"defined?"},
  {"for"},
  {"or"},
  {"then"},
  {"yield"},
  {"and"},
  {"do"},
  {"if"},
  {"redo"},
  {"true"},
  {"__LINE__"},
  {"begin"},
  {"else"},
  {"in"},
  {"rescue"},
  {"undef"},
  {"__FILE__"},
  {"break"},
  {"elsif"},
  {"module"},
  {"retry"},
  {"unless"},
  {"__ENCODING__"},
  {"case"},
  {"end"},
  {"next"},
  {"return"},
  {"until"},
  {NULL}
};

#endif
