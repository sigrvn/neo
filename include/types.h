#ifndef NEO_TYPES_H
#define NEO_TYPES_H

typedef enum {
  /* Primitive types */
  TY_VOID,
  TY_INT,
  TY_UINT,
  TY_FLOAT,
  TY_DOUBLE,
  TY_CHAR,
  TY_BOOL,
  /* User-defined */
  TY_STRUCT,
  TY_ENUM,
  TY_UNION
} TypeKind;

#define IS_PRIMITIVE(ty) (ty >= TY_VOID && ty <= TY_BOOL)

typedef struct {
  TypeKind kind;
  char *name;
  int align;
  int size;
  int ptr;
} Type;

extern const Type PRIMITIVES[];
#define NUM_PRIMTIIVES sizeof(PRIMITIVES) / sizeof(PRIMITIVES[0])

#endif
