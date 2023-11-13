#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#include "ast.h"
#include "optimize.h"
#include "util.h"

int32_t fold_int_unary(int un_op, int32_t n) {
  int32_t result = 0;
  switch (un_op) {
    case UN_NEG:
      result = 0 - n;
      break;
    case UN_NOT:
      result = !n;
      break;
    default: LOG_FATAL("unknown unary operator in fold_int_unary: %d", un_op);
  }
  return result;
}

int32_t fold_int_binary(int bin_op, int32_t left, int32_t right) {
  int32_t result = 0;
  switch (bin_op) {
    case BIN_ADD: result = left + right; break;
    case BIN_SUB: result = left - right; break;
    case BIN_MUL: result = left * right; break;
    case BIN_DIV: result = left / right; break;
    case BIN_CMP: result = left == right; break;
    case BIN_CMP_NOT: result = left != right; break;
    case BIN_CMP_LT: result = left < right; break;
    case BIN_CMP_GT: result = left > right; break;
    case BIN_CMP_LT_EQ: result = left <= right; break;
    case BIN_CMP_GT_EQ: result = left >= right; break;
    default: LOG_FATAL("unknown binary operator in fold_int_binary: %d", bin_op);
  }
  return result;
}

/* Performs Constant Folding and Common-Subexpression Elimination in one pass */
void fold_constants(Node *node) {
  if (!node) return;

  Node *next = node->next;

  switch (node->kind) {
    case ND_UNKNOWN:
      LOG_FATAL("LOG_FATAL error at line %d, col %d: unknown node in AST!",
          node->span.line, node->span.col);
      break;
    case ND_FUNC_DECL:
      fold_constants(node->func.body);
      break;
    case ND_VAR_DECL:
      fold_constants(node->var.value);
      break;
    case ND_ASSIGN_STMT:
      AssignStmt assign = node->assign;
      if (assign.value->kind == ND_REF_EXPR && strcmp(assign.name, assign.value->ref) == 0) {
        LOG_INFO("eliminating self-assignment of variable '%s' on line %d, col %d",
            assign.name, node->span.line, node->span.col);
        node->kind = ND_NOOP;
        free(assign.value);
      } else {
        fold_constants(node->assign.value);
      }
      break;
    case ND_UNARY_EXPR:
      Node *expr = node->unary.expr;
      if (expr->kind == ND_VALUE_EXPR) {
        LOG_INFO("folding constant unary expression of on line %d, col %d",
            node->span.line, node->span.col);

        Value folded = { .kind = expr->value.kind };
        switch (folded.kind) {
          case VAL_INT:
            folded.i_val = fold_int_unary(node->unary.un_op, expr->value.i_val);
            break;
          default:
            LOG_WARN("constant folding not yet supported for Value kind: %d", folded.kind);
            goto end;
        }

        node->kind = ND_VALUE_EXPR;
        node->value = folded;
      }
      break;
    case ND_BINARY_EXPR:
      Node *lhs = node->binary.lhs;
      Node *rhs = node->binary.rhs;

      // TODO: We only fold value constant expressions that are of the same type.
      // No implicit type coercion happens here.
      //
      // In the future, add a warning here if the types of rhs and lhs are not the same
      // and handle type coercion properly.

      if (lhs->kind == ND_UNARY_EXPR || lhs->kind == ND_BINARY_EXPR)
        fold_constants(lhs);

      if (rhs->kind == ND_UNARY_EXPR || rhs->kind == ND_BINARY_EXPR)
        fold_constants(rhs);

      if (lhs->kind == ND_VALUE_EXPR
          && lhs->kind == rhs->kind
          && lhs->value.kind == rhs->value.kind) {
        LOG_INFO("folding constant binary expression of on line %d, col %d",
            node->span.line, node->span.col);

        Value folded = { .kind = lhs->value.kind };
        switch (folded.kind) {
          case VAL_INT:
            folded.i_val = fold_int_binary(node->binary.bin_op, lhs->value.i_val, rhs->value.i_val);
            break;
          default:
            LOG_WARN("constant folding not yet supported for Value kind: %d", folded.kind);
            goto end;
        }

        node->kind = ND_VALUE_EXPR;
        node->value = folded;
      }
      break;
    default: break;
  }

end:
  fold_constants(next);
}

