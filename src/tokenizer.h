#ifndef MMRBC_TOKENIZER_H_
#define MMRBC_TOKENIZER_H_

#include <stdbool.h>

#include "token.h"

typedef enum mode
{
  MODE_NONE,
  MODE_COMMENT,
  MODE_QWORDS,
  MODE_WORDS,
  MODE_QSYMBOLS,
  MODE_SYMBOLS,
  MODE_TSTRING_DOUBLE,
  MODE_TSTRING_SINGLE,
} Mode;

typedef enum paren
{
  PAREN_NONE,
  PAREN_BRACE,
} Paren;

/* margin size to malloc tokenizer */
#define PAREN_STACK_SIZE 40

typedef struct stream_interface
{
  void *stream;
  char *(*fgets)(char *s, int n, FILE *stream);
  int (*feof)(FILE *stream);
} StreamInterface;

typedef struct tokenizer
{
  Mode mode;
  char *line;
  StreamInterface *si;
  Token *currentToken;
  int line_num;
  int pos;
  int paren_stack_num;
  char modeTerminater;
  State state;
  Paren paren_stack[PAREN_STACK_SIZE];
} Tokenizer;

Tokenizer* const Tokenizer_new(StreamInterface *si);

void Tokenizer_free(Tokenizer *self);

void Tokenizer_puts(Tokenizer* const self, char *line);

bool Tokenizer_hasMoreTokens(Tokenizer* const self);

int Tokenizer_advance(Tokenizer* const self,  bool recursive);

#endif
