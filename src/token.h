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

typedef enum type
{
  ON_EMBDOC,
  ON_EMBDOC_END,
} Type;

typedef struct token_location
{
  int line;
  int pos;
} TokenLocation;

typedef struct token
{
  TokenLocation location;
  Type type;
  State state;
  char *value;
} Token;

void Token_new(Token* const self);

bool Token_exists(Token* const self);

#endif
