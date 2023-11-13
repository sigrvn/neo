#ifndef NEO_AST_H
#define NEO_AST_H

#include <stdbool.h>
#include <stdint.h>

#include "compiler.h"
#include "types.h"

typedef struct Node Node;

typedef struct {
  char *name;
  const Type *return_type;
  Node *params;
  Node *body;
} FuncDecl;

typedef struct {
  char *name;
  const Type *type;
  Node *value;
} VarDecl;

typedef struct {
  char *name;
  Node *value;
} AssignStmt;

typedef struct {
  Node *value;
} RetStmt;

typedef struct {
  Node *expr;
  Node *body;
} CondStmt;

/* NOTE: the order of these matter */
typedef enum {
  OP_UNKNOWN = 0,
  UN_NEG,
  UN_NOT,
  UN_DEREF,
  UN_ADDR,
  BIN_ADD,
  BIN_SUB,
  BIN_MUL,
  BIN_DIV,
  BIN_CMP,
  BIN_CMP_NOT,
  BIN_CMP_LT,
  BIN_CMP_GT,
  BIN_CMP_LT_EQ,
  BIN_CMP_GT_EQ
} Operator;

typedef struct {
  Operator un_op;
  Node *expr;
} UnaryExpr;

typedef struct {
  Operator bin_op;
  Node *lhs;
  Node *rhs;
} BinaryExpr;

typedef struct {
  char *name;
  Node *args;
} CallExpr;

typedef enum {
  VAL_INT,
  VAL_UINT,
  VAL_FLOAT,
  VAL_DOUBLE,
  VAL_CHAR,
  VAL_BOOL,
  VAL_STRING
} ValueKind;

typedef struct {
  ValueKind kind;
  union {
    int32_t i_val;
    uint32_t u_val;
    float f_val;
    double d_val;
    char c_val;
    bool b_val;
    struct {
      char *s_val;
      size_t s_len;
    };
  };
} Value;

void dump_value(Value *val);
uint8_t *copy_value(Value *val, size_t *val_size);

typedef enum {
  ND_UNKNOWN = 0,
  ND_NOOP,
  ND_FUNC_DECL,
  ND_VAR_DECL,
  ND_RET_STMT,
  ND_COND_STMT,
  ND_CALL_EXPR,
  ND_ASSIGN_STMT,
  ND_UNARY_EXPR,
  ND_BINARY_EXPR,
  ND_VALUE_EXPR,
  ND_REF_EXPR
} NodeKind;

struct Node {
  NodeKind kind;
  union {
    FuncDecl func;
    VarDecl var;
    AssignStmt assign;
    RetStmt ret;
    CondStmt cond;
    UnaryExpr unary;
    BinaryExpr binary;
    CallExpr call;
    Value value;
    char *ref;
  };
  bool visited;
  Span span;
  const Type *type;
  Node *next;
};

void dump_node(Node *node, int level);
void warn_unused(Node *ast);

#endif
