#include <stdio.h>
#include <string.h>

#include "common.h"
#include "token.h"

void Token_new(Token* const self)
{
  char value[MAX_LINE_LENGTH] = {'\0'};
  self->value = value;
}

bool Token_exists(Token* const self)
{
  if (strlen(self->value) > 0)
    return true;
  return false;
}
