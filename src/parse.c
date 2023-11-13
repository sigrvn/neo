#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ast.h"
#include "compiler.h"
#include "hashmap.h"
#include "parse.h"
#include "symtab.h"
#include "types.h"
#include "util.h"

static Scope *scope     = NULL;
static Token *prev_tok  = NULL;
static Token *tok       = NULL;

static void enter_scope(char *name) {
  Scope *next_scope = scope_new(name);
  next_scope->parent = scope;
  scope = next_scope;
}

static void exit_scope() {
  Scope *parent = scope->parent;
  scope_free(scope);
  scope = parent;
}

static void advance() {
  prev_tok = tok;
  tok = tok->next;
}

static bool match(const char *str) {
  if (tok->len == strlen(str) && memcmp(str, tok->text, tok->len) == 0) {
    advance();
    return true;
  }
  return false;
}

static Token *expect(const char *str) {
  if (!match(str)) {
    LOG_FATAL("at line %d, col %d: expected '%s', got '%.*s' instead",
        tok->span.line, tok->span.col, str, tok->len, tok->text);
  }
  return prev_tok;
}

static Node *node_new(NodeKind kind) {
  Node *node = calloc(1, sizeof(Node));
  if (!node)
    LOG_FATAL("calloc failed for node in node_new");

  node->kind = kind;
  node->type = &PRIMITIVES[TY_VOID];
  node->visited = false;
  node->span = tok->span;
  node->next = NULL;
  return node;
}

void push_node(Node **stack, Node *node) {
  node->next = *stack;
  *stack = node;
}

Node *pop_node(Node **stack) {
  assert(*stack != NULL);
  Node *ret = *stack;
  *stack = ret->next;

  /* Propagate inner-most expression type to outer expressions */
  if (*stack) {
    (*stack)->type = ret->type;
  }

  return ret;
}

static void parse_factor();
static void parse_term();
static Node *parse_block();
static Node *parse_expression();
static Node *parse_identifier();

static Node *parse_unary(Node **stack, char un_op) {
  Node *node = node_new(ND_UNARY_EXPR);
  node->unary.un_op = un_op;
  node->unary.expr = pop_node(stack);
  node->type = node->unary.expr->type;
  return node;
}

static Node *parse_binary(Node **stack, char bin_op) {
  Node *node = node_new(ND_BINARY_EXPR);
  node->binary.bin_op = bin_op;
  node->binary.lhs = pop_node(stack);
  parse_term(stack);
  node->binary.rhs = pop_node(stack);
  node->type = node->binary.lhs->type;
  return node;
}

static Node *parse_call(Token *ident) {
  Node *node = node_new(ND_CALL_EXPR);

  Symbol *symbol = find_symbol(scope, ident->text, ident->len);
  if (!symbol) {
    LOG_FATAL("at line %d, col %d: unknown function '%.*s'",
        ident->span.line, ident->span.col, ident->len, ident->text);
  }
  else if (symbol->kind != SYM_FUNC) {
    LOG_FATAL("at line %d, col %d: symbol '%s' is not a function. (kind: %d)",
        ident->span.line, ident->span.col, symbol->name, symbol->type->kind);
  }

  node->type = symbol->node->func.return_type;
  node->call.name = format("%.*s", ident->len, ident->text);

  expect("(");

  Node args = { 0 };
  Node *cur = &args;

  for (;;) {
    if (match(")")) break;

    cur = cur->next = parse_expression();

    if (match(",")) { continue; }
    else { expect(")"); break; }
  }
  node->call.args = args.next;

  return node;
}

static Node *parse_number() {
  Node *node = node_new(ND_VALUE_EXPR);
  node->type = &PRIMITIVES[TY_INT];
  node->value.kind = VAL_INT;
  node->value.i_val = stoi(tok->text, tok->len);
  advance();
  return node;
}

static Node *parse_boolean(bool value) {
  Node *node = node_new(ND_VALUE_EXPR);
  node->type = &PRIMITIVES[TY_BOOL];
  node->value.kind = VAL_BOOL;
  node->value.b_val = value ? true : false;
  return node;
}

static Node *parse_character() {
  Node *node = node_new(ND_VALUE_EXPR);
  node->type = &PRIMITIVES[TY_CHAR];
  node->value.kind = VAL_CHAR;
  node->value.c_val = tok->text[0];
  advance();
  return node;
}

static void parse_factor(Node **stack) {
  Node *node = NULL;
  if (tok->kind == TOK_IDENT) {
    node = parse_identifier();
  } else if (tok->kind == TOK_NUMBER) {
    node = parse_number();
  } else if (tok->kind == TOK_CHAR) {
    node = parse_character();
  } else if (match("true")) {
    node = parse_boolean(true);
  } else if (match("false")) {
    node = parse_boolean(false);
  } else {
    LOG_FATAL("at line %d, col %d: invalid token '%.*s' while parsing expression",
        tok->span.line, tok->span.col, tok->len, tok->text);
  }
  push_node(stack, node);
}

static void parse_term(Node **stack) {
  parse_factor(stack);
  for (;;) {
    int bin_op = OP_UNKNOWN;
    if      (match("*")) { bin_op = BIN_MUL; }
    else if (match("/")) { bin_op = BIN_DIV; }

    if (bin_op == OP_UNKNOWN) { break; }
    Node *node = parse_binary(stack, bin_op);
    push_node(stack, node);
  }
}

static void _parse_expression(Node **stack) {
  int un_op = OP_UNKNOWN;
  if      (match("-")) { un_op = UN_NEG; }
  else if (match("!")) { un_op = UN_NOT; }
  else if (match("*")) { un_op = UN_DEREF; }

  parse_term(stack);

  if (un_op != OP_UNKNOWN) {
    Node *node = parse_unary(stack, un_op);
    push_node(stack, node);
  }

  for (;;) {
    int bin_op = OP_UNKNOWN;
    if      (match("+"))   { bin_op = BIN_ADD; }
    else if (match("-"))   { bin_op = BIN_SUB; }
    else if (match("=="))  { bin_op = BIN_CMP; }
    else if (match("!="))  { bin_op = BIN_CMP_NOT; }
    else if (match("<"))   { bin_op = BIN_CMP_LT; }
    else if (match(">"))   { bin_op = BIN_CMP_GT; }
    else if (match("<="))  { bin_op = BIN_CMP_LT_EQ; }
    else if (match(">="))  { bin_op = BIN_CMP_GT_EQ; }

    if (bin_op == OP_UNKNOWN) { break; }
    Node *node = parse_binary(stack, bin_op);
    push_node(stack, node);
  }
}

static Node *parse_expression() {
  Node *stack = NULL;
  _parse_expression(&stack);
  return pop_node(&stack);
}

static Node *parse_if_statement() {
  Node *node = node_new(ND_COND_STMT);

  /* TODO: add typechecking to see if expression is a logical expression */
  node->cond.expr = parse_expression();
  node->cond.body = parse_block();

  return node;
}

static Node *parse_else_statement() {
  Node *node = node_new(ND_COND_STMT);
  node->cond.expr = NULL;
  node->cond.body = parse_block();
  return node;
}

static const Type* parse_type() {
  if (tok->kind != TOK_IDENT) {
    LOG_FATAL("at line %d, col %d: expected identifier for type, got '%.*s' instead",
        tok->span.line, tok->span.col, tok->len, tok->text);
  }

  /* Search for type symbol in current scope */
  Symbol *symbol = find_symbol(scope, tok->text, tok->len);
  if (!symbol) {
    LOG_FATAL("at line %d, col %d: unknown type '%.*s'",
        tok->span.line, tok->span.col, tok->len, tok->text);
  }

  if (symbol->kind != SYM_TYPE) {
    LOG_FATAL("at line %d, col %d: symbol '%s' is not a type",
        tok->span.line, tok->span.col, symbol->name);
  }

  advance();

  return symbol->type;
}

static Node *parse_varref(Token *ident) {
  assert(ident->kind == TOK_IDENT);

  Node *node = node_new(ND_REF_EXPR);

  Symbol *symbol = find_symbol(scope, ident->text, ident->len);
  if (!symbol) {
    printf("couldn't find symbol '%.*s'\n", ident->len, ident->text);
    LOG_FATAL("at line %d, col %d: unknown variable '%.*s'",
        ident->span.line, ident->span.col, ident->len, ident->text);
  } else if (symbol->kind != SYM_VAR) {
    LOG_FATAL("at line %d, col %d: symbol '%s' is not a variable. (kind: %d)",
        ident->span.line, ident->span.col, symbol->name, symbol->type->kind);
  }

  node->type = symbol->node->var.type;
  node->ref = format("%.*s", ident->len, ident->text);

  return node;
}

static Node *parse_vardecl() {
  Token *ident = tok;
  assert(ident->kind == TOK_IDENT);

  Node *node = node_new(ND_VAR_DECL);
  node->var.name = format("%.*s", ident->len, ident->text);

  /* Insert variable into current scope */
  Symbol *symbol = symbol_new(SYM_VAR);
  symbol->name = format("%.*s", ident->len, ident->text);
  symbol->node = node;

  if (add_symbol(scope, symbol)) {
    LOG_FATAL("at line %d, col %d: variable '%.*s' redeclared in scope",
        ident->span.line, ident->span.col, ident->len, ident->text);
  }

  advance(); /* advance from <identifier> */

  /* Parse assignment and/or type declaration of variable */
  if (match("=")) {
    node->var.value = parse_expression();
    /* Infer type from expression */
    node->var.type = node->var.value->type;
  } else {
    expect(":");
    node->var.type = parse_type();

    if (match("=")) {
      node->var.value = parse_expression();
    } else {
      LOG_WARN("uninitialized variable '%s' on line %d, col %d",
          node->var.name, node->span.line, node->span.col);
    }
  }

  expect(";");
  return node;
}

static Node *parse_assignment(Token *ident) {
  assert(ident->kind == TOK_IDENT);

  if (!find_symbol(scope, ident->text, ident->len)) {
    LOG_FATAL("at line %d, col %d: unknown variable '%.*s'",
        ident->span.line, ident->span.col, ident->len, ident->text);
  }

  Node *node = node_new(ND_ASSIGN_STMT);
  node->assign.name = format("%.*s", ident->len, ident->text);
  node->assign.value = parse_expression();

  /* TODO: add typechecking to see if expression matches declared type for var */

  expect(";");
  return node;
}

static Node *parse_return() {
  Node *node = node_new(ND_RET_STMT);
  node->ret.value = parse_expression();
  expect(";");
  return node;
}

static Node* parse_identifier() {
  Token *ident = tok;
  advance(); /* advance from <identifier> */

  Node *stmt = NULL;
  if (match("=")) {
    stmt = parse_assignment(ident);
  } else if (match("(")) {
    stmt = parse_call(ident);
  } else {
    stmt = parse_varref(ident);
  }
  return stmt;
}

static Node *parse_block() {
  expect("{");

  Node body = { 0 };
  Node *cur = &body;

  Node *stmt = NULL;
  for (;;) {
    if (match("}"))
      break;

    if (tok->kind == TOK_IDENT) {
      stmt = parse_identifier();
    } else if (match("var")) {
      stmt = parse_vardecl();
    } else if (match("if")) {
      stmt = parse_if_statement();
    } else if (match("else")) {
      stmt = parse_else_statement();
    } else if (match("return")) {
      stmt = parse_return();
    } else {
      LOG_FATAL("at line %d, col %d: invalid token '%.*s' while parsing block",
          tok->span.line, tok->span.col, tok->len, tok->text);
    }

    cur = cur->next = stmt;
  }

  return body.next;
}

static Node *parse_param() {
  if (tok->kind != TOK_IDENT) {
    LOG_FATAL("at line %d, col %d: expected identifier for function parameter, got '%.*s' instead",
        tok->span.line, tok->span.col, tok->len, tok->text);
  }

  Node *node = node_new(ND_VAR_DECL);
  node->var.name = format("%.*s", tok->len, tok->text);

  /* Add paramter to function scope as a variable */
  Symbol *symbol = symbol_new(SYM_VAR);
  symbol->name = format("%.*s", tok->len, tok->text);
  symbol->node = node;

  if (add_symbol(scope, symbol)) {
    LOG_FATAL("at line %d, col %d: function parameter '%s' redeclared in scope",
        tok->span.line, tok->span.col, node->var.name);
  }

  advance(); /* advance from <identifier> */

  /* Parse type */
  expect(":");
  node->var.type = parse_type();

  return node;
}

static Node *parse_funcdecl() {
  if (tok->kind != TOK_IDENT) {
    LOG_FATAL("at line %d, col %d: expected identifier for function, got '%.*s' instead",
        tok->span.line, tok->span.col, tok->len, tok->text);
  }

  Node *node = node_new(ND_FUNC_DECL);
  node->func.name = format("%.*s", tok->len, tok->text);

  Symbol *symbol = symbol_new(SYM_FUNC);
  symbol->name = format("%.*s", tok->len, tok->text);
  symbol->node = node;

  /* Insert function into current scope */
  if (add_symbol(scope, symbol)) {
    LOG_FATAL("at line %d, col %d: function '%s' redeclared in scope",
        node->span.line, node->span.col, node->func.name);
  }

  /* Create & enter function scope */
  enter_scope(node->func.name);

  /* Insert function into its own scope for recursion */
  add_symbol(scope, symbol);

  advance(); /* advance from <identifier> */

  /* Parse parameters */
  Node params = { 0 };
  Node *cur = &params;

  expect("(");
  for (;;) {
    if (match(")"))
      break;

    /* Add paramter to list */
    cur = cur->next = parse_param();

    /* If there is a comma after this parameter, continue parsing params */
    if (match(",")) {
      continue;
    } else {
      expect(")");
      break;
    }
  }
  node->func.params = params.next;

  /* Parse function return type (if no arrow, it's TY_VOID) */
  node->func.return_type = match("->") ? parse_type() : &PRIMITIVES[TY_VOID];

  /* Parse function body */
  node->func.body = parse_block();

  /* Exit the function's scope */
  exit_scope();

  return node;
}

Node *parse(Token *tokens) {
  /* Initialize parser state */
  scope = &SYMTAB;
  prev_tok = NULL;
  tok = tokens;

  Node ast = { 0 };
  Node *cur = &ast;

  Node *decl = NULL;
  while (tok->kind != TOK_EOF) {
    if (match("var")) {
      decl = parse_vardecl();
    } else if (match("func")) {
      decl = parse_funcdecl();
    } else {
      LOG_FATAL("at line %d, col %d: invalid token '%.*s' while parsing top-level",
          tok->span.line, tok->span.col, tok->len, tok->text);
    }
    cur = cur->next = decl;
  }

  return ast.next;
}

