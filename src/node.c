#include <stdlib.h>
#include <string.h>

#include "common.h"
#include "node.h"
#include "ruby-lemon-parse/parse_header.h"

char *Node_valueName(Node *self)
{
  if (self->type == LITERAL) {
    return self->value.name;
  } else if (self->type == iLITERAL) {
    return self->iValue;
  }
}

void Node_setValue(Node *self, const char *s)
{
  if (strlen(s) < PTR_SIZE * 2) {
    self->type = iLITERAL;
    strcpy(self->iValue, s);
  } else {
    self->type = LITERAL;
    self->value.name = strdup(s);
  }
}

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
  return (self->type == LITERAL || self->type == iLITERAL);
}

AtomType Node_atomType(Node *self)
{
  if (self == NULL || self->cons.car == NULL || !Node_isAtom(self->cons.car)) {
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
    return Node_valueName(self->cons.car);
  }
}

NodeBox *Node_newBox(ParserState *p)
{
  int size = sizeof(NodeBox) + sizeof(Node) * p->node_box_size;
  NodeBox *node_box = (NodeBox *)mmrbc_alloc(size);
  memset(node_box, 0, size);
  if (p->current_node_box) p->current_node_box->next = node_box;
  p->current_node_box = node_box;
  node_box->next = NULL;
  node_box->size = p->node_box_size;
  node_box->index = 0;
  return node_box;
}

Node *Node_new(ParserState *p)
{
  NodeBox *box = p->current_node_box;
  box->index++;
  if (box->index == box->size) {
    box = Node_newBox(p);
  }
  return (Node *)((Node *)(&box->nodes) + box->index);
}

void Node_freeAllNode(NodeBox *box)
{
  Node *node;
  NodeBox *next;
  while (box) {
    next = box->next;
    for (int i = 0; i < box->index + 1; i++) {
      node = (Node *)((Node *)(&box->nodes) + i);
      if (node == NULL) break;
      if (node->type == LITERAL) mmrbc_free(node->value.name);
    }
    mmrbc_free(box);
    box = next;
  }
}
