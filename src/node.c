#include <stdlib.h>

#include "node.h"
#include "ruby-lemon-parse/parse_header.h"

bool Node_isAtom(Node *self)
{
  if (self->type == ATOM) return true;
  return false;
}

bool Node_isCons(Node *self)
{
  if (self->type == CONS) return true;
  return false;
}

bool Node_isLiteral(Node *self)
{
  if (self->type == LITERAL) return true;
  return false;
}

AtomType Node_atomType(Node *self)
{
  if (self->cons.car == NULL || !Node_isAtom(self->cons.car)) {
    return ATOM_NONE;
  } else {
    if (self->cons.car->type != ATOM) {
      return ATOM_NONE;
    } else {
      return self->cons.car->atom.type;
    }
  }
}

char *Node_literalName(Node *self)
{
  if (self->cons.car == NULL || !Node_isLiteral(self->cons.car)) {
    return NULL;
  } else {
    return self->cons.car->value.name;
  }
}
