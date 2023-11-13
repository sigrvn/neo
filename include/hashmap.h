#ifndef NEO_HASHMAP_H
#define NEO_HASHMAP_H

#include <stdbool.h>
#include <stddef.h>

typedef struct {
  char *key;
  void *value;
} MapEntry;

typedef struct {
  size_t size;
  size_t capacity;
  MapEntry *entries;
} HashMap;

void hashmap_init(HashMap *m);
void hashmap_free(HashMap *m);
void hashmap_resize(HashMap *m, size_t new_capacity);
bool hashmap_insert(HashMap *m, const char *key, void *value);
void *hashmap_lookup(HashMap *m, const char *key);
void *hashmap_lookup2(HashMap *m, const char *key, size_t len);
bool hashmap_delete(HashMap *m, const char *key);
void hashmap_foreach(HashMap *m, void (*fn)(MapEntry *));
void hashmap_clear(HashMap *m);
void hashmap_print(HashMap *m, int indent);

#endif
