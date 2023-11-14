#ifndef NEO_LEX_H
#define NEO_LEX_H

#include "compiler.h"
#include "defs.h"

typedef enum {
  TOK_UNKNOWN,
  TOK_KEYWORD,
  TOK_SYMBOL,
  TOK_CHAR,
  TOK_STRING,
  TOK_NUMBER,
  TOK_IDENT,
  TOK_EOF,
} TokenKind;

typedef struct Token Token;
struct Token {
  TokenKind kind;
  int len;
  char *text;
  Span span;
  Token *next;
};

Token *lex(File *file);

void dump_tokens(Token *tokens);
void free_tokens(Token *tokens);

#endif
