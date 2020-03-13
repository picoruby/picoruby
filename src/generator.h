#ifndef MMRBC_GENERATOR_H_
#define MMRBC_GENERATOR_H_

#include "scope.h"

#define HEADER_SIZE 22
#define FOOTER_SIZE 8

typedef struct mrb_code
{
  int size;
  char *body;
} MrbCode;

typedef struct node Node;

MrbCode *Generator_generate(Node *root);

#endif
