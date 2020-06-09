#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>

#include "mmrbc.h"
#include "common.h"
#include "debug.h"
#include "scope.h"
#include "node.h"
#include "generator.h"
#include "mrubyc/src/opcode.h"
#include "ruby-lemon-parse/parse_header.h"
#include "ruby-lemon-parse/crc.c"

#define END_SECTION_SIZE 8

typedef enum misc {
  NUM_POS,
  NUM_NEG
} Misc;

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
      codegen(scope, node->cons.cdr->cons.car);
    }
    if (node->cons.cdr != NULL) {
      node = node->cons.cdr->cons.car;
    } else {
      node = NULL;
    }
  }
  return nargs;
}

void gen_str(Scope *scope, Node *node)
{
  Scope_pushCode(OP_STRING);
  Scope_pushCode(scope->sp);
  Scope_push(scope);
  int litIndex = Scope_newLit(scope, Node_literalName(node), STRING_LITERAL);
  Scope_pushCode(litIndex);
}

void gen_sym(Scope *scope, Node *node)
{
  Scope_pushCode(OP_LOADSYM);
  Scope_pushCode(scope->sp);
  Scope_push(scope);
  int litIndex = Scope_newSym(scope, Node_literalName(node));
  Scope_pushCode(litIndex);
}

void gen_literal_numeric(Scope *scope, char *num, LiteralType type, Misc pos_neg)
{
  Scope_pushCode(OP_LOADL);
  Scope_pushCode(scope->sp);
  int litIndex;
  if (pos_neg == NUM_NEG) {
    char *negnum[strlen(num) + 1];
    negnum[0] = '-';
    litIndex = Scope_newLit(scope, strsafecat(negnum, num, strlen(num) + 2), type);
  } else {
     litIndex = Scope_newLit(scope, num, type);
  }
  Scope_pushCode(litIndex);
}

void cleanup_numeric_literal(char *lit, char *result)
{
  int j = 0;
  for (int i = 0; i <= strlen(lit); i++) {
    if (lit[i] == '_') continue;
    result[j] = lit[i];
    j++;
  }
}

void gen_float(Scope *scope, Node *node, Misc pos_neg)
{
  char num[strlen(node->cons.car->value.name)];
  cleanup_numeric_literal(node->cons.car->value.name, num);
  gen_literal_numeric(scope, num, FLOAT_LITERAL, pos_neg);
  Scope_push(scope);
}

void gen_int(Scope *scope, Node *node, Misc pos_neg)
{
  char num[strlen(node->cons.car->value.name)];
  cleanup_numeric_literal(node->cons.car->value.name, num);
  unsigned int val = atoi(num);
  if (pos_neg == NUM_POS && 0 <= val && val <= 7) {
    Scope_pushCode(OP_LOADI_0 + val);
    Scope_pushCode(scope->sp);
  } else if (pos_neg == NUM_NEG && 0 <= val && val <= 1) {
    Scope_pushCode(OP_LOADI_0 - val);
    Scope_pushCode(scope->sp);
  } else if (val <= 0xff) {
    if (pos_neg == NUM_NEG) {
      Scope_pushCode(OP_LOADINEG);
    } else {
      Scope_pushCode(OP_LOADI);
    }
    Scope_pushCode(scope->sp);
    Scope_pushCode(val);
  } else if (val <= 0xffff) {
    Scope_pushCode(OP_EXT2);
    if (pos_neg == NUM_NEG) {
      Scope_pushCode(OP_LOADINEG);
    } else {
      Scope_pushCode(OP_LOADI);
    }
    Scope_pushCode(scope->sp);
    Scope_pushCode(val >> 8);
    Scope_pushCode(val & 0xff);
  } else {
    gen_literal_numeric(scope, num, INTEGER_LITERAL, pos_neg);
  }
  Scope_push(scope);
}

void gen_call(Scope *scope, Node *node)
{
  int nargs = 0;
  if (node->cons.cdr->cons.car) {
    nargs = gen_values(scope, node->cons.cdr->cons.car);
    for (int i = 0; i < nargs; i++) {
      Scope_pop(scope);
    }
  }
  char *method_name = Node_literalName(node->cons.car->cons.cdr);
  if (method_name[1] == '\0' && strchr("><+-*/", method_name[0]) != NULL) {
    switch (method_name[0]) {
      case '>': Scope_pushCode(OP_GT); break;
      case '<': Scope_pushCode(OP_LT); break;
      case '+': Scope_pushCode(OP_ADD); break;
      case '-': Scope_pushCode(OP_SUB); break;
      case '*': Scope_pushCode(OP_MUL); break;
      case '/': Scope_pushCode(OP_DIV); break;
      default: FATALP("This should not happen"); break;
    }
    Scope_pushCode(scope->sp - 1);
  } else if (strcmp(method_name, "==") == 0) {
    Scope_pushCode(OP_EQ);
    Scope_pushCode(scope->sp - 1);
  } else if (strcmp(method_name, ">=") == 0) {
    Scope_pushCode(OP_GE);
    Scope_pushCode(scope->sp - 1);
  } else if (strcmp(method_name, "<=") == 0) {
    Scope_pushCode(OP_LE);
    Scope_pushCode(scope->sp - 1);
  } else {
    Scope_pushCode(OP_SEND);
    Scope_pushCode(scope->sp - 1);
    int symIndex = Scope_newSym(scope, method_name);
    Scope_pushCode(symIndex);
    Scope_pushCode(nargs);
  }
}

void gen_array(Scope *scope, Node *node)
{
  int nargs = 0;
  if (node->cons.cdr->cons.car) {
    nargs = gen_values(scope, node);
    for (int i = 0; i < nargs; i++) {
      Scope_pop(scope);
    }
  }
  Scope_pushCode(OP_ARRAY);
  Scope_pushCode(scope->sp);
  Scope_push(scope);
  Scope_pushCode(nargs);
}

void gen_var(Scope *scope, Node *node)
{
  int num;
  switch(Node_atomType(node)) {
    case (ATOM_lvar):
      num = Scope_lvar_findRegnum(scope->lvar, Node_literalName(node->cons.cdr));
      if (num > 0) {
        Scope_pushCode(OP_MOVE);
        Scope_pushCode(scope->sp);
        Scope_push(scope);
        Scope_pushCode(num);
      } else {
        /* fcall without arg */
        gen_self(scope);
        Scope_pushCode(OP_SEND);
        Scope_pushCode(scope->sp - 1);
        int symIndex = Scope_newSym(scope, Node_literalName(node->cons.cdr));
        Scope_pushCode(symIndex);
        Scope_pushCode(0);
      }
      break;
    case (ATOM_at_ivar):
    case (ATOM_at_gvar):
    case (ATOM_at_const):
      num = Scope_newSym(scope, Node_literalName(node->cons.cdr));
      switch (Node_atomType(node)) {
        case (ATOM_at_ivar):
          Scope_pushCode(OP_GETIV);
          break;
        case (ATOM_at_gvar):
          Scope_pushCode(OP_GETGV);
          break;
        case (ATOM_at_const):
          Scope_pushCode(OP_GETCONST);
          break;
        default:
          FATALP("error");
      }
      Scope_pushCode(scope->sp);
      Scope_push(scope);
      Scope_pushCode(num);
      break;
    default:
      break;
  }
}

void gen_assign(Scope *scope, Node *node)
{
  int num;
  switch(Node_atomType(node->cons.car)) {
    case (ATOM_lvar):
      num = Scope_newLvar(scope, Node_literalName(node->cons.car->cons.cdr), scope->sp);
      Scope_push(scope);
      codegen(scope, node->cons.cdr);
      Scope_pushCode(OP_MOVE);
      Scope_pushCode(num);
      Scope_pushCode(scope->sp - 1);
      break;
    case (ATOM_at_ivar):
    case (ATOM_at_gvar):
    case (ATOM_at_const):
      num = Scope_newSym(scope, Node_literalName(node->cons.car->cons.cdr));
      codegen(scope, node->cons.cdr);
      switch(Node_atomType(node->cons.car)) {
        case (ATOM_at_ivar):
          Scope_pushCode(OP_SETIV);
          break;
        case (ATOM_at_gvar):
          Scope_pushCode(OP_SETGV);
          break;
        case (ATOM_at_const):
          Scope_pushCode(OP_SETCONST);
          break;
        default:
          FATALP("error");
      }
      Scope_pushCode(scope->sp - 1);
      Scope_pushCode(num);
      break;
    case ATOM_call:
      codegen(scope, node->cons.cdr->cons.car); /* right hand */
      codegen(scope, node->cons.car->cons.cdr->cons.car); /* left hand */
      Node *call_node = node->cons.car->cons.cdr->cons.cdr;
      int nargs = 0;
      if (call_node->cons.cdr->cons.car) {
        nargs = gen_values(scope, call_node->cons.cdr->cons.car);
      }
      Scope_pushCode(OP_MOVE);
      Scope_pushCode(scope->sp);
      for (int i = 0; i < nargs + 2; i++) {
        Scope_pop(scope);
      }
      Scope_pushCode(scope->sp);
      Scope_push(scope);
      Scope_pushCode(OP_SEND);
      Scope_pushCode(scope->sp);
      char *method_name = Node_literalName(node->cons.car->cons.cdr->cons.cdr->cons.car->cons.cdr);
      char *assign_method_name = mmrbc_alloc(strlen(method_name) + 2);
      memcpy(assign_method_name, method_name, strlen(method_name));
      assign_method_name[strlen(method_name)] = '=';
      assign_method_name[strlen(method_name) + 1] = '\0';
      int symIndex = Scope_newSym(scope, assign_method_name);
      Scope_pushCode(symIndex);
      Scope_pushCode(nargs + 1);
      break;
    default:
      FATALP("error");
  }
}

void gen_op_assign(Scope *scope, Node *node)
{
  int num;
  switch(Node_atomType(node->cons.car)) {
    case (ATOM_lvar):
      num = Scope_newLvar(scope, Node_literalName(node->cons.car->cons.cdr), scope->sp);
      Scope_pushCode(OP_MOVE);
      Scope_push(scope);
      Scope_pushCode(scope->sp);
      Scope_push(scope);
      Scope_pushCode(num);
      break;
    case (ATOM_at_ivar):
    case (ATOM_at_gvar):
    case (ATOM_at_const):
      num = Scope_newSym(scope, Node_literalName(node->cons.car->cons.cdr->cons.car->cons.cdr));
      switch(Node_atomType(node->cons.car->cons.cdr->cons.car)) {
        case (ATOM_at_ivar):
          Scope_pushCode(OP_GETIV);
          break;
        case (ATOM_at_gvar):
          Scope_pushCode(OP_GETGV);
          break;
        case (ATOM_at_const):
          Scope_pushCode(OP_GETCONST);
          break;
        default:
          FATALP("error");
      }
      Scope_pushCode(scope->sp);
      Scope_push(scope);
      Scope_pushCode(num);
      break;
    default:
      FATALP("error");
  }
  codegen(scope, node->cons.cdr->cons.cdr); /* right hand */
  char *method_name = Node_literalName(node->cons.cdr->cons.car->cons.cdr);
  switch (method_name[0]) {
    case '+':
      Scope_pushCode(OP_ADD);
      Scope_pop(scope);
      Scope_pop(scope);
      Scope_pushCode(scope->sp);
      break;
    case '-':
      Scope_pushCode(OP_SUB);
      Scope_pop(scope);
      Scope_pop(scope);
      Scope_pushCode(scope->sp);
      break;
    case '/':
      Scope_pushCode(OP_DIV);
      Scope_pop(scope);
      Scope_pop(scope);
      Scope_pushCode(scope->sp);
      break;
    case '%':
    case '^':
    case '&':
    case '|':
    case '<': /* <<= */
    case '>': /* >>= */
    case '*': /* *= and **= */
      if (!strcmp(method_name, "*=")) {
        Scope_pushCode(OP_MUL);
        Scope_pop(scope);
        Scope_pop(scope);
        Scope_pushCode(scope->sp);
      } else {
        Scope_pushCode(OP_SEND);
        Scope_pop(scope);
        Scope_pop(scope);
        Scope_pushCode(scope->sp);
        method_name[strlen(method_name) - 1] = '\0';
        int symIndex = Scope_newSym(scope, method_name);
        Scope_pushCode(symIndex);
        Scope_pushCode(1);
      }
      break;
    default:
      FATALP("error");
  }
  switch(Node_atomType(node->cons.car)) {
    case (ATOM_lvar):
      Scope_pushCode(OP_MOVE);
      Scope_pushCode(num);
      Scope_pushCode(scope->sp);
      break;
    case (ATOM_at_ivar):
      Scope_pushCode(OP_SETIV);
      Scope_pushCode(scope->sp);
      Scope_pushCode(num);
      break;
    case (ATOM_at_gvar):
      Scope_pushCode(OP_SETGV);
      Scope_pushCode(scope->sp);
      Scope_pushCode(num);
      break;
    case (ATOM_at_const):
      Scope_pushCode(OP_SETCONST);
      Scope_pushCode(scope->sp);
      Scope_pushCode(num);
      break;
    default:
      FATALP("error");
  }
  Scope_push(scope);
}

void codegen(Scope *scope, Node *tree)
{
  int num;
  if (tree == NULL || Node_isAtom(tree)) return;
  switch (Node_atomType(tree)) {
    case ATOM_NONE:
      codegen(scope, tree->cons.car);
      codegen(scope, tree->cons.cdr);
      break;
    case ATOM_program:
      codegen(scope, tree->cons.cdr->cons.car);
      Scope_pushCode(OP_RETURN);
      Scope_pushCode(scope->sp - 1);
      Scope_pushCode(OP_STOP);
      Scope_finish(scope);
      break;
    case ATOM_stmts_add:
      codegen(scope, tree->cons.car);
      codegen(scope, tree->cons.cdr);
      break;
    case ATOM_stmts_new: // NEW_BEGIN
      break;
    case ATOM_assign:
      gen_assign(scope, tree->cons.cdr);
      break;
    case ATOM_op_assign:
      gen_op_assign(scope, tree->cons.cdr);
      break;
    case ATOM_command:
    case ATOM_fcall:
      gen_self(scope);
      gen_call(scope, tree->cons.cdr);
      codegen(scope, tree->cons.cdr);
      break;
    case ATOM_call:
      codegen(scope, tree->cons.cdr);
      gen_call(scope, tree->cons.cdr->cons.cdr);
      break;
    case ATOM_args_add:
      codegen(scope, tree->cons.cdr);
      break;
    case ATOM_args_new:
      codegen(scope, tree->cons.cdr);
      break;
    case ATOM_symbol_literal:
      gen_sym(scope, tree->cons.cdr);
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
    case ATOM_var_ref:
      gen_var(scope, tree->cons.cdr->cons.car);
      break;
    case ATOM_at_int:
      gen_int(scope, tree->cons.cdr, NUM_POS);
      break;
    case ATOM_at_float:
      gen_float(scope, tree->cons.cdr, NUM_POS);
      break;
    case ATOM_unary:
      switch (Node_atomType(tree->cons.cdr->cons.cdr->cons.car)) {
        case (ATOM_at_int):
          gen_int(scope, tree->cons.cdr->cons.cdr->cons.car->cons.cdr, NUM_NEG);
          break;
        case (ATOM_at_float):
          gen_float(scope, tree->cons.cdr->cons.cdr->cons.car->cons.cdr, NUM_NEG);
          break;
        default:
          break;
      }
      break;
    case ATOM_array:
      gen_array(scope, tree);
      break;
    case ATOM_at_ivar:
    case ATOM_at_gvar:
    case ATOM_at_const:
      switch (Node_atomType(tree)) {
        case ATOM_at_ivar:
          Scope_pushCode(OP_GETIV);
          break;
        case (ATOM_at_gvar):
          Scope_pushCode(OP_GETGV);
          break;
        case (ATOM_at_const):
          Scope_pushCode(OP_GETCONST);
          break;
        default:
          FATALP("error");
      }
      Scope_pushCode(scope->sp);
      Scope_push(scope);
      num = Scope_newSym(scope, Node_literalName(tree->cons.cdr));
      Scope_pushCode(num);
      break;
    case ATOM_lvar:
      gen_var(scope, tree);
      break;
    case ATOM_kw_nil:
      Scope_pushCode(OP_LOADNIL);
      Scope_pushCode(scope->sp);
      Scope_push(scope);
      break;
    case ATOM_kw_true:
      Scope_pushCode(OP_LOADT);
      Scope_pushCode(scope->sp);
      Scope_push(scope);
      break;
    case ATOM_kw_false:
      Scope_pushCode(OP_LOADF);
      Scope_pushCode(scope->sp);
      Scope_push(scope);
      break;
    default:
//      FATALP("error");
      break;
  }
}

void memcpyFlattenCode(uint8_t *body, CodeSnippet *code_snippet, int size)
{
  int pos = 0;
  while (code_snippet != NULL) {
    memcpy(&body[pos], code_snippet->value, code_snippet->size);
    pos += code_snippet->size;
    code_snippet = code_snippet->next;
  }
}

void Generator_generate(Scope *scope, Node *root)
{
  codegen(scope, root);
  int irepSize = Code_size(scope->code_snippet);
  int32_t codeSize = HEADER_SIZE + irepSize + END_SECTION_SIZE;
  uint8_t *vmCode = mmrbc_alloc(codeSize);
  memcpy(&vmCode[0], "RITE0006", 8);
  vmCode[10] = (codeSize >> 24) & 0xff;
  vmCode[11] = (codeSize >> 16) & 0xff;
  vmCode[12] = (codeSize >> 8) & 0xff;
  vmCode[13] = codeSize & 0xff;
  memcpy(&vmCode[14], "MATZ0000", 8);
  memcpyFlattenCode(&vmCode[22], scope->code_snippet, irepSize);
  memcpy(&vmCode[22 + irepSize], "END\0\0\0\0", 7);
  vmCode[22 + irepSize + 7] = 0x08;
  uint16_t crc = calc_crc_16_ccitt(&vmCode[10], codeSize - 10, 0);
  vmCode[8] = (crc >> 8) & 0xff;
  vmCode[9] = crc & 0xff;
  Scope_freeCodeSnippets(scope);
  scope->vm_code = vmCode;
  scope->vm_code_size = codeSize;
}
