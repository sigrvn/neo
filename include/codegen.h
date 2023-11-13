#ifndef NEO_CODEGEN_H
#define NEO_CODEGEN_H

#include "ir.h"

#define BUILD_ARTIFACT "/tmp/neo-build-artifact"

typedef struct {
  size_t code_size;
  char *code;
} Target;

Target nasm_x86_64_generate(BasicBlock *prog);

#endif
