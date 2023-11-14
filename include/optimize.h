#ifndef NEO_OPTIMIZE_H
#define NEO_OPTIMIZE_H

#include "ast.h"

#define CONSTANT_FOLDING (1 << 1)

void fold_constants(Node *node);

#endif
