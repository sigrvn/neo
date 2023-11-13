#include <stdlib.h>

#include "ast.h"
#include "symtab.h"
#include "util.h"

Scope SYMTAB = { 0 };

Symbol *symbol_new(SymbolKind kind) {
  Symbol *symbol = calloc(1, sizeof(Symbol));
  if (!symbol)
    LOG_FATAL("calloc failed for symbol in symbol_new");

  symbol->kind = kind;
  return symbol;
}

Scope *scope_new(char *name) {
  Scope *scope = calloc(1, sizeof(Scope));
  if (!scope)
    LOG_FATAL("calloc failed in enter_scope");

  hashmap_init(&scope->symbols);
  scope->parent = NULL;
  scope->name = name;
  return scope;
}

void scope_free(Scope *scope) {
  hashmap_free(&scope->symbols);
  free(scope);
}

bool add_symbol(Scope *scope, Symbol *symbol) {
  return hashmap_insert(&scope->symbols, symbol->name, (void*)symbol);
}

Symbol *find_symbol(Scope *scope, char *name, int len) {
  Scope *curr = scope;
  while (curr) {
    Symbol *symbol = (Symbol*)hashmap_lookup2(&curr->symbols, name, len);
    if (symbol)
      return symbol;
    curr = curr->parent;
  }

  return NULL;
}

void print_symbols(MapEntry *entry) {
  printf("\"%s\": ", entry->key);
  Symbol *symbol = (Symbol*)entry->value;
  switch (symbol->kind) {
    case SYM_UNKNOWN:
      printf("symbol is unknown!\n");
      break;
    case SYM_VAR:
      printf("Variable: %s\n", symbol->node->var.name);
      break;
    case SYM_FUNC:
      printf("Function: %s\n", symbol->node->func.name);
      break;
    case SYM_TYPE:
      printf("Type: %s\n", symbol->type->name);
      break;
  }
}
