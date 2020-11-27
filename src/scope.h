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

/*
 * A symbol can be:
 *  @ivar, $gvar, puts(fname), :symbol, CONST
 */
typedef struct symbol
{
  char *value;
  struct symbol *next;
} Symbol;

typedef struct lvar
{
  char *name;
  int regnum;
  struct lvar *next;
} Lvar;

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
  CodeSnippet *last_snippet;
  int nlocals;
  int nirep;
  Symbol *symbol;
  Lvar *lvar;
  Literal *literal;
  int sp;
  int max_sp;
  int32_t vm_code_size;
  uint8_t *vm_code;
} Scope;

Scope *Scope_new(Scope *prev);

void Scope_free(Scope *self);

void Scope_pushNCode_self(Scope *self, const uint8_t *value, int size);
#define Scope_pushNCode(v, s) Scope_pushNCode_self(scope, (v), (s))

void Scope_pushCode_self(Scope *self, int val);
#define Scope_pushCode(v) Scope_pushCode_self(scope, (v))

int Scope_newLit(Scope *self, const char *value, LiteralType type);

int Scope_newSym(Scope *self, const char *value);

int Scope_lvar_findRegnum(Lvar *lvar, const char *name);

int Scope_newLvar(Scope *self, const char *name, int newRegnum);

void Scope_push(Scope *self);

void Scope_pop(Scope *self);

int Code_size(CodeSnippet *code_snippet);

void Scope_finish(Scope *self);

void Scope_freeCodeSnippets(Scope *self);

CodeSnippet *Scope_markJmpLabel(Scope *self);

void Scope_backpatchJmpLabel(CodeSnippet *label, int32_t position);

#endif
