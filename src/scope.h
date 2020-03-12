#ifndef MMRBC_SCOPE_H_
#define MMRBC_SCOPE_H_

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
  char *value;
  struct code_snippet *next;
} Code;

typedef struct scope
{
  struct scope *prev;
  struct code_snippet *code;
  int nlocals;
  int nirep;
  struct Symbol *symbol;
  struct Literal *literal;
  int sp;
  int max_sp;
} Scope;

Scope *Scope_new(Scope *prev); 

void Scope_free(Scope *self); 

void Scope_pushCode(Scope *self, const char *value, int size);

int Scope_newLit(Scope *self, const char *value, LiteralType type);

int Scope_newSym(Scope *self, const char *value);

void Scope_push(Scope *self);

void Scope_pop(Scope *self);

#endif
