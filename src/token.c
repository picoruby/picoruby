#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "debug.h"
#include "common.h"
#include "token.h"

Token *Token_new(void)
{
  Token *self = mmrbc_alloc(sizeof(Token));
  self->value = NULL;
  self->type = ON_NONE;
  self->prev = NULL;
  self->next = NULL;
  self->refCount = 1;
  return self;
}

void Token_free(Token* self)
{
  DEBUG("free Token->value: `%s`", self->value);
  mmrbc_free(self->value);
  DEBUG("free Token: %p", self);
  mmrbc_free(self);
}

bool Token_exists(Token* const self)
{
  if (strlen(self->value) > 0)
    return true;
  return false;
}

void Token_GC(Token* currentToken)
{
  DEBUG("GC start");
  Token *prev;
  while (currentToken->prev) {
    prev = currentToken->prev;
    if (currentToken->prev->refCount == 0) {
      Token_free(currentToken->prev);
      currentToken->prev = NULL;
    }
    currentToken = prev;
  }
  DEBUG("GC end");
}
