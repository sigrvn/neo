#ifndef NEO_PARSE_H
#define NEO_PARSE_H

#include "ast.h"
#include "compiler.h"
#include "lex.h"

Node *parse(File *file, Token *tokens);

#endif
