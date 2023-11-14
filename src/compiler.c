#include "compiler.h"
#include "util.h"

CompilationUnit *units = NULL;

void file_open(File *file, const char *filepath, int id) {
  file->id = id;
  file->filepath = filepath,
  file->contents = readfile(filepath, &file->size);
}

void file_free(File *file) {
  if (file->contents)
    free(file->contents);
}
