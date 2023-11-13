#ifndef NEO_OPTIMIZE_H
#define NEO_OPTIMIZE_H

#include "ast.h"

#define CONSTANT_FOLDING (1 << 1)
#define DEFAULT_FEATURES (CONSTANT_FOLDING)

void fold_constants(Node *node);

#endif
