#ifndef LEMON_PARSE_HEADER_H_
#define LEMON_PARSE_HEADER_H_

#include <stdbool.h>

#include "../scope.h"

typedef enum atom_type {
  ATOM_NONE = 0,
  ATOM_program = 1,
  ATOM_method_add_arg,
  ATOM_kw_nil,
  ATOM_kw_true,
  ATOM_kw_false,
  ATOM_lvar,
  ATOM_array,
  ATOM_hash,
  ATOM_assoc_new,
  ATOM_call,
  ATOM_scall,
  ATOM_fcall,
  ATOM_vcall,
  ATOM_command,
  ATOM_assign,
  ATOM_op_assign,
  ATOM_at_op,
  ATOM_var_field,
  ATOM_var_ref,
  ATOM_dstr,
  ATOM_str,
  ATOM_dstr_add,
  ATOM_dstr_new,
  ATOM_string_content,
  ATOM_args_new,
  ATOM_args_add,
  ATOM_args_add_block,
  ATOM_block_arg,
  ATOM_kw_self,
  ATOM_kw_return,
  ATOM_at_int,
  ATOM_at_float,
  ATOM_stmts_add,
  ATOM_string_literal,
  ATOM_symbol_literal,
  ATOM_binary,
  ATOM_unary,
  ATOM_stmts_new,
  ATOM_at_ident,
  ATOM_at_ivar,
  ATOM_at_gvar,
  ATOM_at_const,
  ATOM_at_tstring_content,
  ATOM_if,
} AtomType;

typedef enum {
  ATOM,
  CONS,
  LITERAL
} NodeType;

typedef struct node Node;

typedef struct {
  struct node *car;
  struct node *cdr;
} Cons;

typedef struct {
  AtomType type;
} Atom;

typedef struct {
  char *name;
} Value;

struct node {
  NodeType type;
  union {
    Atom atom;
    Cons cons;
    Value value;
  };
};

typedef struct literal_store
{
  char *str;
  struct literal_store *prev;
} LiteralStore;

typedef struct parser_state {
  Scope *scope;
  Node *root;
  LiteralStore *literal_store;
  int error_count;
} ParserState;

#endif
