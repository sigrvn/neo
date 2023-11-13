#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "hashmap.h"
#include "util.h"

#define MAP_LOAD_FACTOR      0.65
#define MAP_INITIAL_CAPACITY 16

static bool insert_entry(MapEntry *entries,
                         size_t capacity,
                         const char *key,
                         void *value,
                         size_t *plength) {
  uint64_t hash = fnv1a64(key);
  size_t idx = (size_t)(hash & (uint64_t)(capacity - 1));

  while (entries[idx].key) {
    if (strcmp(key, entries[idx].key) == 0) {
      entries[idx].value = value;
      return true;
    }
    idx++;
    if (idx >= capacity)
      idx = 0;
  }

  if (plength) {
    key = format("%s", key);
    if (!key)
      LOG_FATAL("format failed in insert_entry for hashmap");
    (*plength)++;
  }
  entries[idx].key = (char *)key;
  entries[idx].value = value;
  return false;
}

void hashmap_init(HashMap *m) {
  m->size = 0;
  m->capacity = MAP_INITIAL_CAPACITY;
  m->entries = calloc(m->capacity, sizeof(MapEntry));
  if (!m->entries)
    LOG_FATAL("calloc failed in hashmap_init");
}

void hashmap_free(HashMap *m) {
  hashmap_clear(m);
  free(m->entries);
}

void hashmap_resize(HashMap *m, size_t new_capacity) {
  if (new_capacity < m->capacity)
    LOG_FATAL("capacity overflow in hashmap_resize");

  MapEntry *new_entries = calloc(new_capacity, sizeof(MapEntry));
  if (!new_entries)
    LOG_FATAL("calloc failed in hashmap_resize");

  for (size_t i = 0; i < m->capacity; i++) {
    MapEntry entry = m->entries[i];
    if (entry.key)
      insert_entry(new_entries, new_capacity, entry.key, entry.value, NULL);
  }

  free(m->entries);
  m->entries = new_entries;
  m->capacity = new_capacity;
}

bool hashmap_insert(HashMap *m, const char *key, void *value) {
  assert(value != NULL);
  if (m->size >= m->capacity * MAP_LOAD_FACTOR)
    hashmap_resize(m, m->capacity << 1);
  return insert_entry(m->entries, m->capacity, key, value, &m->size);
}

void *hashmap_lookup(HashMap *m, const char *key) {
  return hashmap_lookup2(m, key, strlen(key));
}

void *hashmap_lookup2(HashMap *m, const char *key, size_t len) {
  uint64_t hash = fnv1a64_2(key, len);
  size_t idx = (size_t)(hash & (uint64_t)(m->capacity - 1));

  while (m->entries[idx].key) {
    if (len == strlen(m->entries[idx].key) &&
        memcmp(key, m->entries[idx].key, len) == 0)
      return m->entries[idx].value;
    idx++;
    if (idx >= m->capacity)
      idx = 0;
  }
  return NULL;
}

bool hashmap_delete(HashMap *m, const char *key) {
  size_t len = strlen(key);
  uint64_t hash = fnv1a64_2(key, len);
  size_t idx = (size_t)(hash & (uint64_t)(m->capacity - 1));

  while (m->entries[idx].key) {
    if (len == strlen(m->entries[idx].key) &&
        memcmp(key, m->entries[idx].key, len) == 0) {
      m->size--;
      free((void *)m->entries[idx].key);
      m->entries[idx].key = NULL;
      m->entries[idx].value = NULL;
      return true;
    }
    idx++;
    if (idx >= m->capacity)
      idx = 0;
  }
  return false;
}

void hashmap_foreach(HashMap *m, void (*fn)(MapEntry *)) {
  for (size_t i = 0; i < m->capacity; i++) {
    if (m->entries[i].key)
      fn(&m->entries[i]);
  }
}

void hashmap_clear(HashMap *m)
{
  for (size_t i = 0; i < m->capacity; i++) {
    if (m->entries[i].key)
      free((void *)m->entries[i].key);
    m->entries[i].key = NULL;
    m->entries[i].value = NULL;
  }
}

void hashmap_print(HashMap *m, int indent) {
  printf("{\n");
  for (size_t i = 0; i < m->capacity; i++) {
    if (m->entries[i].key)
      printf("%*s\"%s\": %p\n", indent, "",
          m->entries[i].key, m->entries[i].value);
  }
  printf("}\n");
}
