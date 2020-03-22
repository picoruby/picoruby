#include <stdio.h>
#include <stdint.h>

#include "mmrbc.h"
#include "common.h"
#include "scope.h"
#include "node.h"
#include "generator.h"
#include "mrubyc/src/opcode.h"
#include "ruby-lemon-parse/parse_header.h"
#include "ruby-lemon-parse/crc.c"

#define END_SECTION_SIZE 8

void codegen(Scope *scope, Node *tree);

Scope *new_scope(Scope *prev)
{
  Scope *scope = Scope_new(prev);
  if (prev != NULL) prev->nirep++;
  return scope;
}

void gen_self(Scope *scope)
{
  Scope_pushCode(OP_LOADSELF);
  Scope_pushCode(scope->sp);
  Scope_push(scope);
}

int gen_values(Scope *scope, Node *tree)
{
  int nargs = 0;
  Node *node = tree;
  while (node != NULL) {
    if (hasCdr(node) && hasCar(node->cons.cdr) && Node_atomType(node->cons.cdr->cons.car) == ATOM_args_add) {
      nargs++;
    }
    if (node->cons.cdr != NULL) {
      node = node->cons.cdr->cons.car;
    } else {
      node = NULL;
    }
  }
  codegen(scope, tree->cons.cdr->cons.car);
  return nargs;
}

void gen_str(Scope *scope, Node *node)
{
  Scope_pushCode(OP_STRING);
  Scope_pushCode(scope->sp);
  int litIndex = Scope_newLit(scope, Node_literalName(node), STRING_LITERAL);
  Scope_pushCode(litIndex);
  Scope_push(scope);
}

void gen_call(Scope *scope, Node *tree)
{
  int nargs = gen_values(scope, tree->cons.cdr->cons.cdr->cons.car);
  for (int i = 0; i < nargs; i++) {
    Scope_pop(scope);
  }
  Scope_pushCode(OP_SEND);
  Scope_pushCode(scope->sp - nargs);
  int symIndex = Scope_newSym(scope, Node_literalName(tree->cons.cdr->cons.car->cons.cdr));
  Scope_pushCode(symIndex);
  Scope_pushCode(nargs);
  Scope_pop(scope);
}

void codegen(Scope *scope, Node *tree)
{
  if (tree == NULL || Node_isAtom(tree)) return;
  switch (Node_atomType(tree)) {
    case ATOM_NONE:
      codegen(scope, tree->cons.car);
      codegen(scope, tree->cons.cdr);
      break;
    case ATOM_program:
      codegen(scope, tree->cons.cdr->cons.car);
      Scope_pushCode(OP_RETURN);
      Scope_pushCode(scope->sp);
      Scope_pushCode(OP_STOP);
      Scope_finish(scope);
      break;
    case ATOM_stmts_add:
      codegen(scope, tree->cons.car);
      codegen(scope, tree->cons.cdr);
      break;
    case ATOM_stmts_new: // NEW_BEGIN
      break;
    case ATOM_command:
      gen_self(scope);
      gen_call(scope, tree);
      break;
    case ATOM_args_add:
      codegen(scope, tree->cons.car);
      codegen(scope, tree->cons.cdr);
      break;
    case ATOM_args_new:
      break;
    case ATOM_string_literal:
      codegen(scope, tree->cons.cdr->cons.car->cons.cdr); // skip the first :string_add
      break;
    case ATOM_string_add:
      Scope_pop(scope);
      Scope_pop(scope);
      // TODO gen_2 OP_STRCAT, scope.sp;
      Scope_push(scope);
      break;
    case ATOM_string_content:
      break;
    case ATOM_at_tstring_content:
      gen_str(scope, tree->cons.cdr);
      break;
  }
}

uint8_t *flattenCode(CodeSnippet *code_snippet, int codeSize)
{
  uint8_t *result = mmrbc_alloc(codeSize);
  int index = 0;
  while (code_snippet != NULL) {
    memcpy(&result[index], code_snippet->value, code_snippet->size);
    index += code_snippet->size;
    code_snippet = code_snippet->next;
  }
  return result;
}

MrbCode *Generator_generate(Node *root)
{
  Scope *scope = Scope_new(NULL);
  codegen(scope, root);
  int irepSize = Code_size(scope->code_snippet);
  int codeSize = HEADER_SIZE + irepSize + END_SECTION_SIZE;
  uint8_t *vmCode = mmrbc_alloc(codeSize);
  memcpy(&vmCode[0], "RITE0006", 8);
  vmCode[10] = (codeSize >> 24) & 0xff;
  vmCode[11] = (codeSize >> 16) & 0xff;
  vmCode[12] = (codeSize >> 8) & 0xff;
  vmCode[13] = codeSize & 0xff;
  memcpy(&vmCode[14], "MATZ0000", 8);
  memcpy(&vmCode[22], flattenCode(scope->code_snippet, irepSize), irepSize);
  memcpy(&vmCode[22 + irepSize], "END\0\0\0\0", 7);
  vmCode[22 + irepSize + 7] = 0x08;
  uint16_t crc = calc_crc_16_ccitt(&vmCode[10], codeSize - 10, 0);
  vmCode[8] = (crc >> 8) & 0xff;
  vmCode[9] = crc & 0xff;
  MrbCode *mrb = mmrbc_alloc(sizeof(MrbCode));
  mrb->codeSize = codeSize;
  mrb->vmCode = vmCode;
  return mrb;
}
