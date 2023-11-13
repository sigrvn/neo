#include <stdarg.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "ast.h"
#include "util.h"

static const char unary_ops[] = {
  [UN_NEG] = '-',
  [UN_NOT] = '!',
  [UN_DEREF] = '*',
  [UN_ADDR] = '&',
};

static const char *binary_ops[] = {
  [BIN_ADD] = "+",
  [BIN_SUB] = "-",
  [BIN_MUL] = "*",
  [BIN_DIV] = "/",
  [BIN_CMP] = "==",
  [BIN_CMP_NOT] = "!=",
  [BIN_CMP_LT] = "<",
  [BIN_CMP_GT] = ">",
  [BIN_CMP_LT_EQ] = "<=",
  [BIN_CMP_GT_EQ] = ">=",
};

static void dump(int level, const char *fmt, ...) {
  fprintf(stdout, "%*s", level, "");
  va_list args;
  va_start(args, fmt);
  vfprintf(stdout, fmt, args);
  va_end(args);
}

static void dump_func_decl(FuncDecl *func, int level) {
  dump(level, "function:\n");
  dump(level, " name: %s\n", func->name);
  dump(level, " return_type: %s\n", func->return_type->name);
  dump(level, " params:\n");
  dump_node(func->params, level + 2);
  dump(level, " body:\n");
  dump_node(func->body, level + 2);
}

static void dump_var_decl(VarDecl *var, int level) {
  dump(level, "variable:\n");
  dump(level, " name: %s\n", var->name);
  dump(level, " type: %s\n", var->type->name);
  dump(level, " value:\n");
  dump_node(var->value, level + 2);
}

static void dump_ret_stmt(RetStmt *ret, int level) {
  dump(level, "return:\n");
  dump(level, " value:\n");
  dump_node(ret->value, level + 2);
}

static void dump_cond_stmt(CondStmt *cond, int level) {
  dump(level, "conditional:\n");
  dump(level, " expr:\n");
  dump_node(cond->expr, level + 2);
  dump(level, " body:\n");
  dump_node(cond->body, level + 2);
}

static void dump_assign_stmt(AssignStmt *assign, int level) {
  dump(level, "assignment:\n");
  dump(level, " name: %s\n", assign->name);
  dump_node(assign->value, level + 2);
}

static void dump_unary_expr(UnaryExpr *unary, int level) {
  dump(level, "unary:\n");
  dump(level, " op: %c\n", unary_ops[unary->un_op]);
  dump(level, " expr:\n");
  dump_node(unary->expr, level + 2);
}

static void dump_binary_expr(BinaryExpr *binary, int level) {
  dump(level, "binary:\n");
  dump(level, " op: %s\n", binary_ops[binary->bin_op]);
  dump(level, " lhs:\n");
  dump_node(binary->lhs, level + 2);
  dump(level, " rhs:\n");
  dump_node(binary->rhs, level + 2);
}

static void dump_call_expr(CallExpr *call, int level) {
  dump(level, "call:\n");
  dump(level, " name: %s\n", call->name);
  dump(level, " args:\n");
  dump_node(call->args, level + 2);
}

static void dump_value_expr(Value *value, int level) {
  dump(level, "value: ");
  dump_value(value);
  dump(level, "\n");
}

void dump_node(Node *node, int level) {
  if (!node) return;

  switch (node->kind) {
    case ND_UNKNOWN: dump(level, "<UNKNOWN>\n"); break;
    case ND_FUNC_DECL: dump_func_decl(&node->func, level); break;
    case ND_VAR_DECL: dump_var_decl(&node->var, level); break;
    case ND_RET_STMT: dump_ret_stmt(&node->ret, level); break;
    case ND_COND_STMT: dump_cond_stmt(&node->cond, level); break;
    case ND_CALL_EXPR: dump_call_expr(&node->call, level); break;
    case ND_ASSIGN_STMT: dump_assign_stmt(&node->assign, level); break;
    case ND_UNARY_EXPR: dump_unary_expr(&node->unary, level); break;
    case ND_BINARY_EXPR: dump_binary_expr(&node->binary, level); break;
    case ND_VALUE_EXPR: dump_value_expr(&node->value, level); break;
    case ND_REF_EXPR: dump(level, "ref: %s\n", node->ref); break;
    default: LOG_FATAL("invalid AST! (%d)", node->kind);
  }

  dump_node(node->next, level);
}

void dump_value(Value *val) {
  switch (val->kind) {
    case VAL_INT:
      printf("%d", val->i_val);
      break;
    case VAL_UINT:
      printf("%d", val->u_val);
      break;
    case VAL_FLOAT:
      printf("%f", val->f_val);
      break;
    case VAL_DOUBLE:
      printf("%g", val->d_val);
      break;
    case VAL_CHAR:
      printf("%c", val->c_val);
      break;
    case VAL_BOOL:
      printf("%s", val->b_val ? "true" : "false");
      break;
    case VAL_STRING:
      printf("%*s", (int)val->s_len, val->s_val);
      break;
    default: LOG_FATAL("invalid value! (%d)", val->kind);
  }
}

uint8_t *copy_value(Value *val, size_t *value_size) {
  uint8_t *bytes = NULL;
  size_t size = 0;

  switch (val->kind) {
    case VAL_INT:
      size += sizeof(intmax_t);
      bytes = calloc(size, sizeof(uint8_t));
      memcpy(bytes, &val->i_val, size);
      break;
    case VAL_UINT:
      size += sizeof(uintmax_t);
      bytes = calloc(size, sizeof(uint8_t));
      memcpy(bytes, &val->u_val, size);
      break;
    case VAL_FLOAT:
      size += sizeof(float);
      bytes = calloc(size, sizeof(uint8_t));
      memcpy(bytes, &val->f_val, size);
      break;
    case VAL_DOUBLE:
      size += sizeof(double);
      bytes = calloc(size, sizeof(uint8_t));
      memcpy(bytes, &val->d_val, size);
      break;
    case VAL_CHAR:
      size += sizeof(uint8_t);
      bytes = calloc(size, sizeof(uint8_t));
      memcpy(bytes, &val->c_val, size);
      break;
    case VAL_BOOL:
      size += sizeof(bool);
      bytes = calloc(size, sizeof(uint8_t));
      memcpy(bytes, &val->b_val, size);
      break;
    case VAL_STRING:
      size += val->s_len;
      bytes = calloc(size, sizeof(uint8_t));
      memcpy(bytes, val->s_val, size);
      break;
  }

  *value_size = size;
  return bytes;
}

void warn_unused(Node *ast) {
  Node *node = ast;
  while (node) {
    if (!node->visited) {
      switch (node->kind) {
        case ND_FUNC_DECL:
          LOG_WARN("unused function %s at line %d, col %d",
              node->func.name, node->span.line, node->span.col);
          break;
        case ND_VAR_DECL:
          LOG_WARN("unused variable %s at line %d, col %d",
              node->var.name, node->span.line, node->span.col);
          break;
        default: break;
      }
    }
    node = node->next;
  }
}
