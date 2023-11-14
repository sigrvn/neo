#ifndef NEO_IR_H
#define NEO_IR_H

#include <stdint.h>

#include "ast.h"
#include "compiler.h"
#include "hashmap.h"
#include "types.h"

/* NEO Intermediate Representation (Three-Address Code) */
/* NOTE: The order of these matter and correspond to the Operator enum in ast.h */
typedef enum {
  OP_NEG         = UN_NEG,
  OP_NOT         = UN_NOT,
  OP_DEREF       = UN_DEREF,
  OP_ADDR        = UN_ADDR,
  OP_ADD         = BIN_ADD,
  OP_SUB         = BIN_SUB,
  OP_MUL         = BIN_MUL,
  OP_DIV         = BIN_DIV,
  OP_CMP         = BIN_CMP,
  OP_CMP_NOT     = BIN_CMP_NOT,
  OP_CMP_LT      = BIN_CMP_LT,
  OP_CMP_GT      = BIN_CMP_GT,
  OP_CMP_LT_EQ   = BIN_CMP_LT_EQ,
  OP_CMP_GT_EQ   = BIN_CMP_GT_EQ,
  OP_DEF,
  OP_ASSIGN,
  OP_JMP,
  OP_BR,
  OP_RET,
  OP_DEAD
} Opcode;

#define MAX_OPERANDS 2

typedef struct Instruction Instruction;
struct Instruction {
  Opcode opcode;
  int start, end;
  char *assignee;
  int nopers;
  struct {
    enum {
      O_UNKNOWN = 0,
      O_VALUE,
      O_VARIABLE,
      O_LABEL
    } kind;
    union {
      Value val;
      char *var;
      char *label;
    };
  } operands[MAX_OPERANDS];
  Span span;
  Type *type;
  Instruction *next;
  Instruction *prev;
};

#define IS_VALUE(o)     (o.kind == O_VALUE)
#define IS_VARIABLE(o)  (o.kind == O_VARIABLE)
#define IS_LABEL(o)     (o.kind == O_LABEL)

typedef struct BasicBlock BasicBlock;
struct BasicBlock {
  int id;
  char *tag;
  Instruction *head, *tail;
  BasicBlock **pred, **succ;
  BasicBlock *next, *prev;
};

struct ControlGraph {
  int pc;
  BasicBlock *entry, *exit;
};

BasicBlock *lower_to_ir(Node *node);
void dump_ir(BasicBlock *prog);
void dump_instruction(Instruction *inst);

#endif
