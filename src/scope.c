#include <stdlib.h>

#include "common.h"
#include "scope.h"

Scope *Scope_new(Scope *prev){
  Scope *self = mmrbc_alloc(sizeof(Scope));
  self->prev = prev;
  self->code = NULL;
  self->nlocals = 1;
  self->nirep = 0;
  self->symbol = NULL;
  self->literal = NULL;
  self->sp = 1;
  self->max_sp = 1;
  if (prev != NULL) self->nirep++;
  return self;
}

void Scope_free(Scope *self)
{
  /* TODO */
}

void Scope_pushCode(Scope *self, const char *value, int size){
  Code *snippet = mmrbc_alloc(sizeof(Code));
  snippet->value = mmrbc_alloc(strlen(value));
  memcpy(snippet->value, value, size);
  snippet->size = size;
  snippet->next = NULL;
  Code *code = self->code;
  for (;;) { // find the last code from top (RAM over CPU)
    if (code->next == NULL) break;
    code = code->next;
  }
  code->next = snippet;
}

Literal *literal_new(const char *value, LiteralType type)
{
  Literal *literal = mmrbc_alloc(sizeof(Literal));
  literal->next = NULL;
  literal->type = type;
  size_t len = strlen(value) + 1;
  literal->value = mmrbc_alloc(len);
  strsafecpy(literal->value, value, len);
  return literal;
}

int Scope_newLit(Scope *self, const char *value, LiteralType type){
  int index = 0;//literal_findIndex(self->literal, value);
  if (index > -1) return index;
  Literal *newLit = literal_new(value, type);
  Literal *lit = (Literal *)self->literal;
  if (lit == NULL) {
    lit = newLit;
    return 0;
  }
  for (index = 1; ; index++) {
    if (lit->next == NULL) break;
    lit = lit->next;
  }
  lit->next = newLit;
  return index;
}

Symbol *symbol_new(const char *value)
{
  Symbol *symbol = mmrbc_alloc(sizeof(Symbol));
  symbol->next = NULL;
  size_t len = strlen(value) + 1;
  symbol->value = mmrbc_alloc(len);
  strsafecpy(symbol->value, value, len);
  return symbol;
}

int Scope_newSym(Scope *self, const char *value){
  int index = 0;//symbol_findIndex(self->symbol, value);
  if (index > -1) return index;
  Symbol *newSym = symbol_new(value);
  Symbol *sym = (Symbol *)self->symbol;
  if (sym == NULL) {
    sym = newSym;
    return 0;
  }
  for (index = 1; ; index++) {
    if (sym->next == NULL) break;
    sym = sym->next;
  }
  sym->next = newSym;
  return index;
}

void Scope_push(Scope *self){
  self->sp++;
  if (self->max_sp < self->sp) self->max_sp = self->sp;
}

void Scope_pop(Scope *self){
  self->sp--;
}
