#ifndef NEO_CODEGEN_H
#define NEO_CODEGEN_H

#include "ir.h"

typedef struct {
  size_t code_size;
  char *code;
} Target;

Target nasm_x86_64_generate(BasicBlock *prog);

#endif
