#ifndef NEO_COMPILER_H
#define NEO_COMPILER_H

#include <stddef.h>

#include "ast.h"

typedef struct {
  int id;
  size_t size;
  char *filepath;
  char *contents;
} File;

void file_open(File *file, const char *filepath, int id);
void file_free(File *file);

typedef struct {
  File file;
  Node *ast;
} CompilationUnit;

extern CompilationUnit *units;

#endif
