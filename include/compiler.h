#ifndef NEO_COMPILER_H
#define NEO_COMPILER_H

typedef struct {
  int line;
  int col;
  int file_id;
} Span;

typedef struct {
  const char *filepath;
} File;

#endif
