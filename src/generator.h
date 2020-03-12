#ifndef MMRBC_GENERATOR_H_
#define MMRBC_GENERATOR_H_

#include "scope.h"

#define HEADER_SIZE 22

typedef struct node Node;

Scope *Generator_generate(Node *root);

#endif
