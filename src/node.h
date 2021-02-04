#ifndef MMRBC_NODE_H_
#define MMRBC_NODE_H_

#include <stdint.h>
#include <stdbool.h>
#include "ruby-lemon-parse/parse_header.h"

bool Node_isAtom(Node *self);

bool Node_isCons(Node *self);

bool Node_isLiteral(Node *self);

AtomType Node_atomType(Node *self);

char *Node_literalName(Node *self);

NodeBox *Node_newBox(ParserState *p);

Node *Node_new(ParserState *p);

void Node_freeAllNode(NodeBox *box);

#endif
