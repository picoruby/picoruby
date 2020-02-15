#include <stdio.h>
#include <string.h>

#include "common.h"
#include "token.h"

void Token_new(Token* self)
{
  self->value = (char *)malloc(sizeof(char) * MAX_LINE_LENGTH);
  memset(self->value, '\0', MAX_LINE_LENGTH);
  self->type = ON_NONE;
  self->prev = NULL;
  self->next = NULL;
}

bool Token_exists(Token* const self)
{
  if (strlen(self->value) > 0)
    return true;
  return false;
}
