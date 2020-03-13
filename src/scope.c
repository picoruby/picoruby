#include <stdlib.h>
#include <string.h>

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

void Scope_pushCodeStr_self(Scope *self, const char *str, int size)
{
  Code *snippet = mmrbc_alloc(sizeof(Code));
  snippet->value = mmrbc_alloc(size);
  memcpy(snippet->value, str, size);
  snippet->size = size;
  snippet->next = NULL;
  if (self->code == NULL) {
    self->code = snippet;
  } else {
    Code *code = self->code;
    for (;;) { // find the last code from top (RAM over CPU)
      if (code->next == NULL) break;
      code = code->next;
    }
    code->next = snippet;
  }
}


void Scope_pushCode_self(Scope *self, int val)
{
  char str[1];
  str[0] = (unsigned char)val;
  Scope_pushCodeStr_self(self, str, 1);
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
  int index = -1;//literal_findIndex(self->literal, value);
  if (index >= 0) return index;
  Literal *newLit = literal_new(value, type);
  Literal *lit = self->literal;
  if (lit == NULL) {
    self->literal = newLit;
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
  int index = -1;//symbol_findIndex(self->symbol, value);
  if (index >= 0) return index;
  Symbol *newSym = symbol_new(value);
  Symbol *sym = self->symbol;
  if (sym == NULL) {
    self->symbol = newSym;
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

int Code_size(Code *code)
{
  int size = 0;
  while (code != NULL) {
    size += code->size;
    code = code->next;
  }
  return size;
}

void Scope_finish(Scope *scope)
{
  int op_size = Code_size(scope->code);
  int count;
  int len;
  // literal
  Literal *lit;
  count = 0;
  lit = (Literal *)scope->literal;
  while (lit != NULL) {
    count++;
    lit = lit->next;
  }
  Scope_pushCode((count & 0xff000000) >> 24);
  Scope_pushCode((count & 0x00ff0000) >> 16);
  Scope_pushCode((count & 0x0000ff00) >> 8);
  Scope_pushCode((count & 0x000000ff));
  lit = scope->literal;
  while (lit != NULL) {
    Scope_pushCode(lit->type);
    len = strlen(lit->value);
    Scope_pushCode(len & 0xff00 >> 8);
    Scope_pushCode(len & 0x00ff);
    Scope_pushCodeStr(lit->value, len);
    lit = lit->next;
  }
  // symbol
  Symbol *sym;
  count = 0;
  sym = scope->symbol;
  while (sym != NULL) {
    count++;
    sym = sym->next;
  }
  Scope_pushCode((count & 0xff000000) >> 24);
  Scope_pushCode((count & 0x00ff0000) >> 16);
  Scope_pushCode((count & 0x0000ff00) >> 8);
  Scope_pushCode((count & 0x000000ff));
  sym = scope->symbol;
  while (sym != NULL) {
    len = strlen(sym->value);
    Scope_pushCode(len & 0xff00 >> 8);
    Scope_pushCode(len & 0x00ff);
    Scope_pushCodeStr(sym->value, len);
    Scope_pushCode(0); // NULL terminate? FIXME
    sym = sym->next;
  }

}
