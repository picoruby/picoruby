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
  unsigned long val;
  switch (num[1]) {
    case ('b'):
    case ('B'):
      val = strtol(num+2, NULL, 2);
      break;
    case ('o'):
    case ('O'):
      val = strtol(num+2, NULL, 8);
      break;
    case ('x'):
    case ('X'):
      val = strtol(num+2, NULL, 16);
      break;
    default:
      val = strtol(num, NULL, 10);
  }
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
    char buf[12];
    snprintf(buf, 12, "%ld", val);
    gen_literal_numeric(scope, buf, INTEGER_LITERAL, pos_neg);
  }
  Scope_push(scope);
}

void gen_call(Scope *scope, Node *node)
{
  int nargs = 0;
  int op = OP_SEND;
  if (node->cons.cdr->cons.car) {
    if (Node_atomType(node->cons.cdr->cons.car->cons.cdr->cons.car) == ATOM_block_arg) {
      /* .call(&:method) */
      codegen(scope, node->cons.cdr->cons.car->cons.cdr->cons.car->cons.cdr);
      Scope_pop(scope);
      op = OP_SENDB;
    } else {
      nargs = gen_values(scope, node);
      for (int i = 0; i < nargs; i++) {
        Scope_pop(scope);
      }
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
    Scope_pushCode(op);
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

void gen_hash(Scope *scope, Node *node)
{
  int nassocs = 0;
  Node *assoc = node;
  while (assoc) {
    codegen(scope, assoc->cons.car->cons.cdr);
    nassocs++;
    assoc = assoc->cons.cdr;
  }
  for (int i = 0; i < nassocs * 2; i++) {
    Scope_pop(scope);
  }
  Scope_pushCode(OP_HASH);
  Scope_pushCode(scope->sp);
  Scope_push(scope);
  Scope_pushCode(nassocs);
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

int assignSymIndex(Scope *scope, char *method_name)
{
  char *assign_method_name = mmrbc_alloc(strlen(method_name) + 2);
  memcpy(assign_method_name, method_name, strlen(method_name));
  assign_method_name[strlen(method_name)] = '=';
  assign_method_name[strlen(method_name) + 1] = '\0';
  int symIndex = Scope_newSym(scope, assign_method_name);
  mmrbc_free(assign_method_name);
  return symIndex;
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
        nargs = gen_values(scope, call_node);
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
      int symIndex = assignSymIndex(scope, method_name);
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
  char *method_name, *call_name;
  method_name = Node_literalName(node->cons.cdr->cons.car->cons.cdr);
  bool isANDOPorOROP = false; /* &&= or ||= */
  if (method_name[1] == '|' || method_name[1] == '&') {
    isANDOPorOROP = true;
  }
  CodeSnippet *jmpLabel;
  Node *recv;
  int symIndex;
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
      num = Scope_newSym(scope, Node_literalName(node->cons.car->cons.cdr));
      switch(Node_atomType(node->cons.car)) {
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
    case (ATOM_call):
      Scope_push(scope);
      call_name = Node_literalName(node->cons.car->cons.cdr->cons.cdr->cons.car->cons.cdr);
      recv = node->cons.car->cons.cdr->cons.car;
      codegen(scope, node->cons.car);
      break;
    default:
      FATALP("error");
  }
  /* right hand */
  if (!isANDOPorOROP) codegen(scope, node->cons.cdr->cons.cdr);
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
      } else if (isANDOPorOROP) {
        switch (method_name[1]) {
          case '|':
            Scope_pushCode(OP_JMPIF);
            break;
          case '&':
            Scope_pushCode(OP_JMPNOT);
            break;
        }
        Scope_pushCode(--scope->sp);
        jmpLabel = Scope_reserveJmpLabel(scope);
        /* right condition of `___ &&= ___` */
        codegen(scope, node->cons.cdr);
        Scope_pop(scope);
      } else {
        Scope_pushCode(OP_SEND);
        Scope_pop(scope);
        Scope_pop(scope);
        Scope_pushCode(scope->sp);
        method_name[strlen(method_name) - 1] = '\0';
        symIndex = Scope_newSym(scope, method_name);
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
    case (ATOM_call):
      /*
       * TODO FIXME
       * `obj[]+=` probably works
       * `obj.attr+=` doesn't work yet
       */
      /* right hand of assignment (mass-assign dosen't work) */
      Scope_pushCode(OP_MOVE);
      Scope_push(scope);
      Scope_push(scope);
      Scope_pushCode(scope->sp);
      Scope_pop(scope);
      Scope_pop(scope);
      Scope_pushCode(scope->sp);
      /* load recv of assignment */
      codegen(scope, recv);
      /* `n` of `recv[n]+=` */
      gen_values(scope, node->cons.car->cons.cdr->cons.cdr);
      /* exec assignment .[]= or .attr= */
      Scope_pushCode(OP_SEND);
      Scope_pop(scope);
      Scope_pop(scope);
      Scope_pushCode(scope->sp);
      symIndex = assignSymIndex(scope, call_name);
      Scope_pushCode(symIndex);
     /* count of args */
     if (!strcmp(call_name, "[]")) {
       Scope_pushCode(2); /* .[]= */
     } else {
       Scope_pushCode(1); /* .attr= */
     }
      break;
    default:
      FATALP("error");
  }
  //Scope_push(scope);
  /* goto label */
  if (isANDOPorOROP) Scope_backpatchJmpLabel(jmpLabel, scope->vm_code_size);
}

void gen_dstr(Scope *scope, Node *node)
{
  int num = 0; /* number of ATOM_dstr_add */
  Node *dstr = node->cons.cdr->cons.car;
  while (Node_atomType(dstr) != ATOM_dstr_new) {
    dstr = dstr->cons.cdr->cons.car;
    num++;
  }
  /* start from the bottom (== the first str) of the tree */
  int sp = scope->sp; /* copy */
  for (int count = num; 0 < count; count--) {
    dstr = node->cons.cdr->cons.car;
    for (int i = 1; i < count; i++) {
      dstr = dstr->cons.cdr->cons.car;
    }
    codegen(scope, dstr->cons.cdr->cons.cdr->cons.car);
    if (count != num) {
      /* TRICKY (I'm not sure if this is a correct way) */
// I don't remember why I wrote this :joy:
// TODO remove if it's OK
//      if (scope->sp > sp + 1) {
//        Scope_pushCode(OP_MOVE);
//        Scope_pushCode(sp + 1);
//        Scope_pushCode(scope->sp - 1);
//      }
      Scope_pushCode(OP_STRCAT);
      scope->sp = sp;
      Scope_pushCode(scope->sp);
      Scope_push(scope);
    }
  }
}

void gen_and_or(Scope *scope, Node *node, int opcode)
{
  /* left condition */
  codegen(scope, node->cons.car);
  Scope_pushCode(opcode); /* and->OP_JMPNOT, or->OP_JMPIF */
  Scope_pushCode(--scope->sp);
  CodeSnippet *label = Scope_reserveJmpLabel(scope);
  /* right condition */
  codegen(scope, node->cons.cdr);
  /* goto label */
  Scope_backpatchJmpLabel(label, scope->vm_code_size);
}

void gen_case_when(Scope *scope, Node *node, int cond_reg, CodeSnippet *label_true[])
{
  if (Node_atomType(node->cons.car) != ATOM_args_add) {
    return;
  } else {
    gen_case_when(scope, node->cons.car->cons.cdr, cond_reg, label_true + 1);
    codegen(scope, node->cons.car->cons.cdr->cons.cdr);
    Scope_pushCode(OP_MOVE);
    Scope_pushCode(scope->sp);
    Scope_pushCode(cond_reg);
    Scope_pushCode(OP_SEND);
    Scope_pushCode(cond_reg + 1);
    Scope_pushCode(Scope_newSym(scope, "==="));
    Scope_pushCode(1);
    /* when condition matched */
    Scope_pushCode(OP_JMPIF);
    Scope_pushCode(cond_reg + 1);
    *label_true = Scope_reserveJmpLabel(scope);
    scope->sp = cond_reg + 1;
  }
}

void gen_case(Scope *scope, Node *node)
{
  /* count number of cases */
  Node *case_body = node->cons.cdr->cons.car;
  int i = 0;
  while (1) {
    if (case_body && case_body->cons.car) i++; else break;
    case_body = case_body->cons.cdr->cons.cdr->cons.car;
  }
  int when_count = i;
  CodeSnippet *label_end_array[when_count];
  /* case expression */
  codegen(scope, node->cons.car);
  int cond_reg = scope->sp - 1; /* cond_reg === when_expr */
  /* each case_body */
  case_body = node->cons.cdr->cons.car;
  i = 0;
  while (1) {
    /* when expression */
    int args_count = 0;
    {
      /* count number of args */
      Node *args_node = case_body;
      while (1) {
        if (Node_atomType(args_node->cons.car) != ATOM_args_add) break;
        args_count++;
        args_node = args_node->cons.car->cons.cdr;
      }
    }
    CodeSnippet *label_true_array[args_count];
    gen_case_when(scope, case_body, cond_reg, label_true_array);
    /* when condition didn't match */
    Scope_pushCode(OP_JMP);
    CodeSnippet *label_false = Scope_reserveJmpLabel(scope);
    /* content */
    for (int j = 0; j < args_count; j++)
      Scope_backpatchJmpLabel(label_true_array[j], scope->vm_code_size);
    codegen(scope, case_body->cons.cdr->cons.car);
    Scope_pushCode(OP_JMP);
    label_end_array[i++] = Scope_reserveJmpLabel(scope);
    /* next case */
    Scope_backpatchJmpLabel(label_false, scope->vm_code_size);
    case_body = case_body->cons.cdr->cons.cdr->cons.car;
    Scope_pop(scope);
    if (case_body) {
      if (case_body->cons.car) {
        continue;
      } else {
        if (case_body->cons.cdr->cons.car) {
          /* else */
          codegen(scope, case_body->cons.cdr->cons.car);
          break;
        }
      }
    } else {
      /* no else clause */
      Scope_pushCode(OP_LOADNIL);
      Scope_pushCode(scope->sp++);
      break;
    }
  }
  for (i = 0; i < when_count; i++)
    Scope_backpatchJmpLabel(label_end_array[i], scope->vm_code_size);
}

void gen_if(Scope *scope, Node *node)
{
  /* assert condition */
  codegen(scope, node->cons.car);
  Scope_pushCode(OP_JMPNOT);
  Scope_pushCode(--scope->sp);
  CodeSnippet *label_false = Scope_reserveJmpLabel(scope);
  /* condition true */
  codegen(scope, node->cons.cdr->cons.car);
  Scope_pop(scope);
  Scope_pushCode(OP_JMP);
  CodeSnippet *label_end = Scope_reserveJmpLabel(scope);
  /* condition false */
  Scope_backpatchJmpLabel(label_false, scope->vm_code_size);
  if (Node_atomType(node->cons.cdr->cons.cdr->cons.car) == ATOM_NONE) {
    /* right before KW_end */
    Scope_pushCode(OP_LOADNIL);
    Scope_pushCode(scope->sp++);
  } else {
    /* if_tail */
    codegen(scope, node->cons.cdr->cons.cdr->cons.car);
  }
  /* right after KW_end */
  Scope_backpatchJmpLabel(label_end, scope->vm_code_size);
}

void gen_while(Scope *scope, Node *node, int op_jmp)
{
  Scope_pushBreakStack(scope);
  Scope_pushCode(OP_JMP);
  CodeSnippet *label_cond = Scope_reserveJmpLabel(scope);
  scope->break_stack->redo_pos = scope->vm_code_size;
  /* inside while */
  Scope_pop(scope);
  uint32_t top = scope->vm_code_size;
  codegen(scope, node->cons.cdr);
  /* just before condition */
  Scope_backpatchJmpLabel(label_cond, scope->vm_code_size);
  /* condition */
  codegen(scope, node->cons.car);
  Scope_pushCode(op_jmp);
  Scope_pushCode(--scope->sp);
  CodeSnippet *label_top = Scope_reserveJmpLabel(scope);
  Scope_backpatchJmpLabel(label_top, top);
  /* after while block */
  Scope_pushCode(OP_LOADNIL);
  Scope_pushCode(scope->sp++);
  Scope_popBreakStack(scope);
}

void gen_break(Scope *scope, Node *node)
{
  Scope_push(scope);
  codegen(scope, node);
  Scope_pop(scope);
  Scope_pushCode(OP_JMP);
  scope->break_stack->code_snippet = Scope_reserveJmpLabel(scope);
}

void gen_next(Scope *scope, Node *node)
{
  Scope_push(scope);
  codegen(scope, node);
  Scope_push(scope);
  Scope_pushCode(OP_JMP);
  CodeSnippet *label = Scope_reserveJmpLabel(scope);
  Scope_backpatchJmpLabel(label, scope->break_stack->next_pos);
}

void gen_redo(Scope *scope)
{
  Scope_push(scope);
  Scope_pushCode(OP_JMP);
  CodeSnippet *label = Scope_reserveJmpLabel(scope);
  Scope_backpatchJmpLabel(label, scope->break_stack->redo_pos);
}

void codegen(Scope *scope, Node *tree)
{
  int num;
  if (tree == NULL || Node_isAtom(tree)) return;
  switch (Node_atomType(tree)) {
    case ATOM_and:
      gen_and_or(scope, tree->cons.cdr, OP_JMPNOT);
      break;
    case ATOM_or:
      gen_and_or(scope, tree->cons.cdr, OP_JMPIF);
      break;
    case ATOM_case:
      gen_case(scope, tree->cons.cdr);
      break;
    case ATOM_if:
      gen_if(scope, tree->cons.cdr);
      break;
    case ATOM_while:
      gen_while(scope, tree->cons.cdr, OP_JMPIF);
      break;
    case ATOM_until:
      gen_while(scope, tree->cons.cdr, OP_JMPNOT);
      break;
    case ATOM_break:
      gen_break(scope, tree->cons.cdr);
      break;
    case ATOM_next:
      gen_next(scope, tree->cons.cdr);
      break;
    case ATOM_redo:
      gen_redo(scope);
      break;
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
      break;
    case ATOM_call:
      codegen(scope, tree->cons.cdr->cons.car);
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
    case ATOM_dsymbol:
      gen_dstr(scope, tree->cons.cdr->cons.car);
      Scope_pushCode(OP_INTERN);
      Scope_pushCode(scope->sp - 1);
      break;
    case ATOM_str:
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
    case ATOM_hash:
      gen_hash(scope, tree->cons.cdr->cons.car);
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
    case ATOM_kw_self:
      gen_self(scope);
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
    case ATOM_kw_return:
      if (tree->cons.cdr->cons.car) {
        codegen(scope, tree->cons.cdr->cons.car);
      } else {
        Scope_pushCode(OP_LOADNIL);
        Scope_pushCode(scope->sp);
        Scope_push(scope);
      }
      Scope_pushCode(OP_RETURN);
      Scope_pushCode(scope->sp - 1);
      break;
    case ATOM_dstr:
      gen_dstr(scope, tree);
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
