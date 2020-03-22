#ifndef MMRBC_SCOPE_H_
#define MMRBC_SCOPE_H_

#include <stdint.h>

#define IREP_HEADER_SIZE 26

typedef enum literal_type
{
  STRING_LITERAL  = 0,
  INTEGER_LITERAL = 1,
  FLOAT_LITERAL   = 2
} LiteralType;

typedef struct literal
{
  LiteralType type;
  char *value;
  struct literal *next;
} Literal;

typedef struct symbol
{
  char *value;
  struct symbol *next;
} Symbol;

typedef struct code_snippet
{
  int size;
  uint8_t *value;
  struct code_snippet *next;
} CodeSnippet;

typedef struct scope
{
  struct scope *prev;
  CodeSnippet *code_snippet;
  int nlocals;
  int nirep;
  Symbol *symbol;
  Literal *literal;
  int sp;
  int max_sp;
} Scope;

Scope *Scope_new(Scope *prev); 

void Scope_free(Scope *self); 

void Scope_pushNCode_self(Scope *self, const uint8_t *value, int size);
#define Scope_pushNCode(v, s) Scope_pushNCode_self(scope, (v), (s))

void Scope_pushCode_self(Scope *self, int val);
#define Scope_pushCode(v) Scope_pushCode_self(scope, (v))

int Scope_newLit(Scope *self, const char *value, LiteralType type);

int Scope_newSym(Scope *self, const char *value);

void Scope_push(Scope *self);

void Scope_pop(Scope *self);

int Code_size(CodeSnippet *code_snippet);

void Scope_finish(Scope *self);

#endif
