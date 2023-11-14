#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include "util.h"

int stoi(const char *s, size_t len) {
  int n = 0;
  for (size_t i = 0; i < len; i++) {
    n = n * 10 + (s[i] - '0');
  }
  return n;
}

uint64_t djb2(const char *s) {
  uint64_t hash = 5381;
  int c;
  while ((c = *s++)) {
    hash = ((hash << 5) + hash) + c;
  }
  return hash;
}

uint64_t fnv1a64(const char *s) {
  return fnv1a64_2(s, strlen(s));
}

uint64_t fnv1a64_2(const char *s, size_t len) {
  uint64_t hash = 0xcbf29ce484222325;
  for (size_t i = 0; i < len; i++) {
    hash ^= s[i];
    hash *= 0x100000001b3;
  }
  return hash;
}

char *format(const char *fmt, ...) {
  va_list args;

  va_start(args, fmt);
  int size = vsnprintf(NULL, 0, fmt, args);
  va_end(args);

  char *str = calloc(size + 1, sizeof(char));
  va_start(args, fmt);
  vsnprintf(str, size + 1, fmt, args);
  va_end(args);

  return str;
}

int count_digits(int n) {
  int count = 0;

  if (n < 0) {
    n = -n;
    count++;
  }

  do {
    n /= 10;
    count++;
  } while (n != 0);

  return count;
}

char *randstr(size_t len) {
  static const char alphabet[] = "0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ";
#define ALPHABET_LENGTH sizeof(alphabet)/sizeof(alphabet[0])
  char *str = calloc(len + 1, sizeof(char));
  for (size_t i = 0; i < len; i++) {
    size_t index = rand() % ALPHABET_LENGTH;
    str[i] = alphabet[index];
  }
  str[len] = 0;
  return str;
#undef ALPHABET_LENGTH
}

char *readfile(const char *filename, size_t *size) {
  FILE *f = NULL;
  if (!(f = fopen(filename, "r")))
    LOG_FATAL("%s: %s", filename, strerror(errno));

  fseek(f, 0L, SEEK_END);
  *size = ftell(f);
  rewind(f);

  char *text = calloc(*size + 1, sizeof(char));
  if (!text)
    LOG_FATAL("calloc failed in readfile");

  size_t nread = fread(text, sizeof(char), *size, f);
  if (nread != *size)
    LOG_FATAL("only read %zu/%zu bytes from file %s\n", nread, *size, filename);

  text[nread] = 0;
  fclose(f);

  return text;
}

int spawn_subprocess(char *prog, char *const args[]) {
  int status = 0;
  pid_t pid = fork();
  switch (pid) {
    case -1:
      return pid;
    case 0:
      execvp(prog, args);
      perror(prog);
      exit(1);
    default:
      waitpid(pid, &status, 0);
  }
  return status;
}
