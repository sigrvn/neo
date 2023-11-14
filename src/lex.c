#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lex.h"
#include "util.h"

/* Lexer state */
static File *currfile   = NULL;
static char *p          = NULL;
static char *start      = NULL;
static char *line_start = NULL;

static int line         = 0;
static int col          = 0;

static void fail_at(Token *tok, const char *fmt, ...) {
  fprintf(stderr, "%s:%d:%d: ",
      currfile->filepath,
      tok->span.line,
      tok->span.col);

  va_list args;
  va_start(args, fmt);
  vfprintf(stderr, fmt, args);
  va_end(args);

  fprintf(stderr, "\n");

  while (tok->text < line_start && line_start[-1] != '\n')
    line_start--;

  char *line_end = line_start;
  while (*line_end != '\n')
    line_end++;

  int line_len = line_end - line_start;
  int nspaces = 3 + count_digits(line) + tok->span.col;

  fprintf(stderr, " %d | %.*s\n", line, line_len, line_start);
  fprintf(stderr, "%*s^\n", nspaces, "");

  exit(EXIT_FAILURE);
}

static char next() {
  if (*p == 0)
    return 0;

  char c = *p;
  if (c == '\n') {
    line_start = p + 1;
    line++;
    col = 1;
  } else {
    col++;
  }
  p++;
  return c;
}

static char peek() {
  return (*p == 0) ? 0 : *p;
}

static bool match(char c) {
  bool matches = peek() == c;
  if (matches) next();
  return matches;
}

static bool match_pattern(bool (*fn)(char)) {
  bool matches = fn(peek());
  if (matches) next();
  return matches;
}

static bool is_whitespace(char c) {
  return c == ' ' || c == '\n' || c == '\t' || c == '\r';
}

static bool is_alphabetic(char c) {
  return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || c == '_';
}

static bool is_numeric(char c) {
  return c >= '0' && c <= '9';
}

static bool is_alphanumeric(char c) {
  return is_alphabetic(c) || is_numeric(c);
}

static Token* token_new(TokenKind kind) {
  Token *tok = calloc (1, sizeof(Token));
  if (!tok)
    LOG_FATAL("calloc failed for tok in token_new");

  tok->kind = kind;
  tok->text = p;
  tok->span.line = line;
  tok->span.col  = col;
  tok->span.file_id = currfile->id;
  return tok;
}

static bool is_keyword(const char *s, int len) {
  static const char *keywords[] = {
    "const", "var", "return", "func",
    "import", "export", "struct", "enum",
    "if", "else", "true", "false",
  };

#define NUM_KEYWORDS sizeof(keywords) / sizeof(keywords[0])
  for (size_t i = 0; i < NUM_KEYWORDS; i++) {
    const char *kw = keywords[i];
    if (len == strlen(kw) && memcmp(kw, s, len) == 0) {
      return true;
    }
  }
  return false;
}

static Token *lex_alpha() {
  Token *tok = token_new(TOK_IDENT);

  while (match_pattern(is_alphanumeric));
  tok->len = p - start;

  if (is_keyword(tok->text, tok->len))
    tok->kind = TOK_KEYWORD;

  return tok;
}

/* TODO: add support for binary/octal/hexadecimal numbers & floating point numbers */
static Token *lex_number() {
  Token *tok = token_new(TOK_NUMBER);

  while (match_pattern(is_numeric));
  tok->len = p - start;

  return tok;
}

static void lex_character(Token *tok) {
  tok->text = p;
  tok->kind = TOK_CHAR;
  char c = next();
  if (c == '\'')
    fail_at(tok, "missing char");

  c = next();
  if (c != '\'')
    fail_at(tok, "char is too long");

  tok->len = p - tok->text - 1;
}

static void lex_string(Token *tok) {
  tok->text = p;
  tok->kind = TOK_STRING;
  for (char c = next(); c != '"'; c = next()) {
    if (!c)
      fail_at(tok, "undelimited string");
  }
  tok->len = p - tok->text - 1;
}

static Token *lex_symbol() {
  Token *tok = token_new(TOK_SYMBOL);
  char c = next();
  switch (c) {
    case '+':
    case '*':
    case '/':
    case ';':
    case ',':
    case '.':
    case '{':
    case '}':
    case '(':
    case ')':
    case '[':
    case ']': break;
    case '-': match('>'); break;
    case '=': match('='); break;
    case '!': match('='); break;
    case ':': match('='); break;
    case '<': match('='); break;
    case '>': match('='); break;
    case '"': lex_string(tok); return tok;
    case '\'': lex_character(tok); return tok;
    default: fail_at(tok, "unknown character '%c'", c);
  }
  tok->len = p - start;
  return tok;
}

Token *lex(File *file) {
  /* Initialize lexer state */
  currfile = file;
  p = file->contents;
  line = col = 1;

  Token head = { 0 };
  Token *cur = &head;

  for (;;) {
    start = p;

    char c = peek();
    if (c == 0) {
      cur = cur->next = token_new(TOK_EOF);
      break;
    }

    /* Skip whitespace */
    if (is_whitespace(c)) {
      while (match_pattern(is_whitespace));
      continue;
    }

    /* Skip comments */
    if (match('/')) {
      /* Single-line */
      if (match('/')) {
        for (;;) {
          if (match('\n')) break;
          next();
        }
        continue;
      }

      /* Multi-line */
      if (match('*')) {
        for (;;) {
          if (match('*') && match('/')) break;
          next();
        }
        continue;
      }
    }

    if (is_alphabetic(c)) {
      cur = cur->next = lex_alpha();
    } else if (is_numeric(c)) {
      cur = cur->next = lex_number();
    } else {
      cur = cur->next = lex_symbol();
    }
  }

  return head.next;
}

void dump_tokens(Token *tokens) {
  Token *tok = tokens;
  while (tok) {
    if (tok->kind == TOK_EOF)
      break;
    printf("%.*s\n", tok->len, tok->text);
    tok = tok->next;
  }
}

void free_tokens(Token *tokens) {
  Token *tok = tokens;
  while (tok) {
    Token *next = tok->next;
    free(tok);
    tok = next;
  }
}
