#ifndef NEO_UTIL_H
#define NEO_UTIL_H

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#ifdef DEBUG
#define UNUSED(x) (void)x
#define STATIC_ASSERT(cond, msg) \
  typedef char static_assertion_##msg[(!!(cond))*2-1]
#endif

#define MAX(x, y) (x > y) ? x : y
#define MIN(x, y) (x > y) ? y : x

#define ANSI_RED     "\x1b[1;31m"
#define ANSI_GREEN   "\x1b[1;32m"
#define ANSI_YELLOW  "\x1b[1;33m"
#define ANSI_RESET   "\x1b[0m"

#define LOG_INFO(fmt, ...)  fprintf(stderr, ANSI_GREEN "info: " ANSI_RESET fmt "\n", ##__VA_ARGS__)
#define LOG_WARN(fmt, ...)  fprintf(stderr, ANSI_YELLOW "warn: " ANSI_RESET fmt "\n", ##__VA_ARGS__)
#define LOG_ERROR(fmt, ...) fprintf(stderr, ANSI_RED "error: " ANSI_RESET fmt "\n", ##__VA_ARGS__)
#define LOG_FATAL(fmt, ...) fprintf(stderr, "fatal: "fmt"\n", ##__VA_ARGS__)

uint64_t djb2(const char *s);
uint64_t fnv1a64(const char *s);
uint64_t fnv1a64_2(const char *s, size_t len);

int stoi(const char *s, size_t len);
double stod(const char *s, size_t len);

char *format(const char *fmt, ...);
char *randstr(size_t len);

#endif