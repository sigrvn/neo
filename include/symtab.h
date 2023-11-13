#ifndef NEO_SYMTAB_H
#define NEO_SYMTAB_H

#include "ast.h"
#include "hashmap.h"
#include "types.h"

typedef enum {
  SYM_UNKNOWN,
  SYM_VAR,
  SYM_FUNC,
  SYM_TYPE
} SymbolKind;

typedef struct {
  SymbolKind kind;
  char *name;
  Node *node;
  Type *type;
} Symbol;

Symbol *symbol_new(SymbolKind kind);

typedef struct Scope Scope;
struct Scope {
  HashMap symbols;
  Scope *parent;
  char *name;
};

Scope *scope_new(char *name);
void scope_free(Scope *scope);

bool add_symbol(Scope *scope, Symbol *symbol);
Symbol *find_symbol(Scope *scope, char *name, int len);

void print_symbols(MapEntry *entry);

extern Scope SYMTAB;

#endif
