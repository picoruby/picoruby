#ifndef MMRBC_GENERATOR_H_
#define MMRBC_GENERATOR_H_

#include <stdint.h>

#include "scope.h"

#define HEADER_SIZE 22
#define FOOTER_SIZE 8

typedef struct mrb_code
{
  int32_t codeSize;
  uint8_t *vmCode;
} MrbCode;

typedef struct node Node;

MrbCode *Generator_generate(Node *root);

#endif
