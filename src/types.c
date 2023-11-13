#include <stdbool.h>
#include <stdint.h>

#include "types.h"

const Type PRIMITIVES[] = {
  [TY_VOID] = {
    .kind = TY_VOID,
    .name = "void",
    .align = 0,
    .size = 0,
    .ptr = 0,
  },
  [TY_INT] = {
    .kind = TY_INT,
    .name = "int",
    .align = 0,
    .size = sizeof(int32_t),
    .ptr = 0,
  },
  [TY_UINT] = {
    .kind = TY_UINT,
    .name = "uint",
    .align = 0,
    .size = sizeof(uint32_t),
    .ptr = 0,
  },
  [TY_FLOAT] = {
    .kind = TY_FLOAT,
    .name = "float",
    .align = 0,
    .size = sizeof(float),
    .ptr = 0,
  },
  [TY_DOUBLE] = {
    .kind = TY_DOUBLE,
    .name = "double",
    .align = 0,
    .size = sizeof(double),
    .ptr = 0,
  },
  [TY_CHAR] = {
    .kind = TY_CHAR,
    .name = "char",
    .align = 0,
    .size = sizeof(char),
    .ptr = 0,
  },
  [TY_BOOL] = {
    .kind = TY_BOOL,
    .name = "bool",
    .align = 0,
    .size = sizeof(bool),
    .ptr = 0,
  },
};
