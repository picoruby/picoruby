#include <stdlib.h>
#include <stdbool.h>
#include <string.h>

#include "common.h"
#include "debug.h"
#include "scope.h"

#define IREP_HEADER_SIZE 14

Scope *Scope_new(Scope *upper)
{
  Scope *self = mmrbc_alloc(sizeof(Scope));
  self->upper = upper;
  self->first_lower = NULL;
  self->nlowers = 0;
  self->next = NULL;
  if (upper != NULL) {
    if (upper->first_lower == NULL) upper->first_lower = self;
    Scope *prev = upper->first_lower;
    for (int i = 0; i < upper->nlowers; i++) prev = prev->next;
    if (prev != self) prev->next = self;
    upper->nlowers++;
  }
  self->code_generated = false;
  self->code_snippet = NULL;
  self->last_snippet = NULL;
  self->nlocals = 1;
  self->symbol = NULL;
  self->lvar = NULL;
  self->literal = NULL;
  self->sp = 1;
  self->max_sp = 1;
  self->vm_code = NULL;
  self->vm_code_size = 0;
  self->break_stack = NULL;
  return self;
}

void freeLiteralRcsv(Literal *literal)
{
  if (literal == NULL) return;
  freeLiteralRcsv(literal->next);
  mmrbc_free(literal->value);
  mmrbc_free(literal);
}

void freeSymbolRcsv(Symbol *symbol)
{
  if (symbol == NULL) return;
  freeSymbolRcsv(symbol->next);
  mmrbc_free(symbol->value);
  mmrbc_free(symbol);
}

void freeLvarRcsv(Lvar *lvar)
{
  if (lvar == NULL) return;
  freeLvarRcsv(lvar->next);
  mmrbc_free(lvar->name);
  mmrbc_free(lvar);
}

void Scope_free(Scope *self)
{
  if (self == NULL) return;
  Scope_free(self->next);
  Scope_free(self->first_lower);
  freeLiteralRcsv(self->literal);
  freeSymbolRcsv(self->symbol);
  freeLvarRcsv(self->lvar);
  if (self->vm_code != NULL) {
    mmrbc_free(self->vm_code);
  }
  mmrbc_free(self);
}

void Scope_pushNCode_self(Scope *self, const uint8_t *str, int size)
{
  CodeSnippet *snippet = mmrbc_alloc(sizeof(CodeSnippet));
  snippet->value = mmrbc_alloc(size);
  memcpy(snippet->value, str, size);
  snippet->size = size;
  snippet->next = NULL;
  if (self->code_snippet == NULL) {
    self->code_snippet = snippet;
  } else {
    self->last_snippet->next = snippet;
  }
  self->last_snippet = snippet;
  self->vm_code_size = self->vm_code_size + size;
}


void Scope_pushCode_self(Scope *self, int val)
{
  uint8_t str[1];
  str[0] = (uint8_t)val;
  Scope_pushNCode_self(self, str, 1);
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

/*
 * returns -1 if literal was not found
 */
int literal_findIndex(Literal *literal, const char *value, LiteralType type)
{
  int i = 0;
  while (literal != NULL) {
    if (literal->type == type && strcmp(literal->value, value) == 0) {
      return i;
    }
    literal = literal->next;
    i++;
  }
  return -1;
}

int Scope_newLit(Scope *self, const char *value, LiteralType type){
  int index = literal_findIndex(self->literal, value, type);
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

Lvar *lvar_new(const char *name, int regnum)
{
  Lvar *lvar = mmrbc_alloc(sizeof(Lvar));
  lvar->regnum = regnum;
  lvar->next = NULL;
  size_t len = strlen(name) + 1;
  lvar->name = mmrbc_alloc(len);
  strsafecpy(lvar->name, name, len);
  return lvar;
}

/*
 * returns -1 if symbol was not found
 */
int symbol_findIndex(Symbol *symbol, const char *value)
{
  int i = 0;
  while (symbol != NULL) {
    if (strcmp(symbol->value, value) == 0) {
      return i;
    }
    symbol = symbol->next;
    i++;
  }
  return -1;
}

int Scope_newSym(Scope *self, const char *value){
  int index = symbol_findIndex(self->symbol, value);
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

/*
 * returns -1 if lvar was not found
 */
int Scope_lvar_findRegnum(Lvar *lvar, const char *name)
{
  while (lvar != NULL) {
    if (strcmp(lvar->name, name) == 0) {
      return lvar->regnum;
    }
    lvar = lvar->next;
  }
  return -1;
}

int Scope_newLvar(Scope *self, const char *name, int newRegnum){
  int regnum = Scope_lvar_findRegnum(self->lvar, name);
  if (regnum >= 0) return regnum;
  Lvar *newLvar = lvar_new(name, newRegnum);
  if (self->lvar == NULL) {
    self->lvar = newLvar;
  } else {
    Lvar *lvar = self->lvar;
    while (lvar->next) {
      lvar = lvar->next;
    }
    lvar->next = newLvar;
  }
  return newRegnum;
}

void Scope_push(Scope *self){
  self->sp++;
  if (self->max_sp < self->sp) self->max_sp = self->sp;
}

void Scope_pop(Scope *self){
  self->sp--;
}

int Scope_codeSize(CodeSnippet *code_snippet)
{
  int size = 0;
  while (code_snippet != NULL) {
    size += code_snippet->size;
    code_snippet = code_snippet->next;
  }
  return size;
}

void Scope_finish(Scope *scope)
{
  int op_size = scope->vm_code_size;
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
  Scope_pushCode((count >> 24) & 0xff);
  Scope_pushCode((count >> 16) & 0xff);
  Scope_pushCode((count >> 8) & 0xff);
  Scope_pushCode(count & 0xff);
  lit = scope->literal;
  while (lit != NULL) {
    Scope_pushCode(lit->type);
    len = strlen(lit->value);
    Scope_pushCode((len >>8) & 0xff);
    Scope_pushCode(len & 0xff);
    Scope_pushNCode((uint8_t *)lit->value, len);
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
  Scope_pushCode((count >> 24) & 0xff);
  Scope_pushCode((count >> 16) & 0xff);
  Scope_pushCode((count >> 8) & 0xff);
  Scope_pushCode(count & 0xff);
  sym = scope->symbol;
  while (sym != NULL) {
    len = strlen(sym->value);
    Scope_pushCode((len >>8) & 0xff);
    Scope_pushCode(len & 0xff);
    Scope_pushNCode((uint8_t *)sym->value, len);
    Scope_pushCode(0); // NULL terminate? FIXME
    sym = sym->next;
  }
  // irep header
  uint8_t *h = mmrbc_alloc(IREP_HEADER_SIZE);
  // record length. but whatever it works because of mruby's bug
  //memcpy(&h[0], "\0\0\0\0", 4);
  { // monkey patch for migrating mruby3. see mrubyc/src/load.c
    h[0] = 0xff;
    h[1] = 0xff;
    h[2] = 0xff;
    h[3] = 0xff;
  }
  int l = scope->nlocals;
  h[4] = (l >> 8) & 0xff;
  h[5] = l & 0xff;
  l = scope->max_sp + 1; // RIGHT? FIXME
  h[6] = (l >> 8) & 0xff;
  h[7] = l & 0xff;
  l = scope->nlowers;
  h[8] = (l >> 8) & 0xff;
  h[9] = l & 0xff;
  h[10] = (op_size >> 24) & 0xff;
  h[11] = (op_size >> 16) & 0xff;
  h[12] = (op_size >> 8) & 0xff;
  h[13] = op_size & 0xff;
  // insert h before op codes
  CodeSnippet *header = mmrbc_alloc(sizeof(CodeSnippet));
  header->value = h;
  header->size = IREP_HEADER_SIZE;
  header->next = scope->code_snippet;
  scope->code_snippet = header;
}

void freeCodeSnippetRcsv(CodeSnippet *snippet)
{
  if (snippet == NULL) return;
  freeCodeSnippetRcsv(snippet->next);
  mmrbc_free(snippet->value);
  mmrbc_free(snippet);
}

void Scope_freeCodeSnippets(Scope *self)
{
  if (self == NULL) return;
  Scope_freeCodeSnippets(self->next);
  Scope_freeCodeSnippets(self->first_lower);
  freeCodeSnippetRcsv(self->code_snippet);
  self->code_snippet = NULL;
}

CodeSnippet *Scope_reserveJmpLabel(Scope *scope)
{
  Scope_pushNCode("\0\0", 2);
  return scope->last_snippet;
}

void Scope_backpatchJmpLabel(CodeSnippet *label, int32_t position)
{
  label->value[0] = (position >> 8) & 0xff;
  label->value[1] = position & 0xff;
}

void Scope_pushBreakStack(Scope *self)
{
  BreakStack *break_stack = mmrbc_alloc(sizeof(BreakStack));
  break_stack->code_snippet = NULL;
  break_stack->next_pos = self->vm_code_size;
  if (self->break_stack) {
    break_stack->prev = self->break_stack;
  } else {
    break_stack->prev = NULL;
  }
  self->break_stack = break_stack;
}

void Scope_popBreakStack(Scope *self)
{
  BreakStack *memo = self->break_stack;
  if (self->break_stack->code_snippet)
    Scope_backpatchJmpLabel(self->break_stack->code_snippet, self->vm_code_size);
  self->break_stack = self->break_stack->prev;
  mmrbc_free(memo);
}

int Scope_updateVmCodeSizeThenReturnTotalSize(Scope *self)
{
  int totalSize = 0;
  if (self == NULL) return 0;
  totalSize += Scope_updateVmCodeSizeThenReturnTotalSize(self->first_lower);
  totalSize += Scope_updateVmCodeSizeThenReturnTotalSize(self->next);
  self->vm_code_size = Scope_codeSize(self->code_snippet);
  totalSize += self->vm_code_size;
  return totalSize;
}

