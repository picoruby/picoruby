#ifndef MMRBC_TOKEN_H_
#define MMRBC_TOKEN_H_

#include <stdbool.h>

typedef enum state
{
 EXPR_NONE    = 0b0000000000000,
 EXPR_BEG     = 0b0000000000001,
 EXPR_VALUE   = 0b0000000000001,
 EXPR_END     = 0b0000000000010,
 EXPR_ENDARG  = 0b0000000000100,
 EXPR_ENDFN   = 0b0000000001000,
 EXPR_END_ANY = 0b0000000001110,
 EXPR_ARG     = 0b0000000010000,
 EXPR_CMDARG  = 0b0000000100000,
 EXPR_ARG_ANY = 0b0000000110000,
 EXPR_MID     = 0b0000001000000,
 EXPR_FNAME   = 0b0000010000000,
 EXPR_DOT     = 0b0000100000000,
 EXPR_CLASS   = 0b0001000000000,
 EXPR_BEG_ANY = 0b0001001000001,
 EXPR_LABEL   = 0b0010000000000,
 EXPR_LABELED = 0b0100000000000,
 EXPR_FITEM   = 0b1000000000000
} State;

typedef unsigned int Type; //defined in parse.h

typedef struct token
{
  int line_num;
  int pos;
  Type type;
  State state;
  struct token *prev;
  struct token *next;
  char *value;
  int refCount;
} Token;

Token *Token_new(void);

void Token_free(Token *self);

void Token_GC(Token *currentToken);

bool Token_exists(Token* const self);

#endif
