#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include "ast.h"
#include "codegen.h"
#include "defs.h"
#include "lex.h"
#include "ir.h"
#include "optimize.h"
#include "parse.h"
#include "symtab.h"
#include "types.h"
#include "util.h"

#define DUMP_TOKENS   (1 << 1)
#define DUMP_AST      (1 << 2)
#define DUMP_SYMBOLS  (1 << 3)
#define DUMP_IR       (1 << 4)

#define BUILD_ARTIFACT "/tmp/neo-build-artifact"

#define CONSTANT_FOLDING (1 << 1)
#define DEFAULT_OPTIMIZATIONS (CONSTANT_FOLDING)

/* Compiler Options */
int opt_dflags = 0;
int opt_oflags = DEFAULT_OPTIMIZATIONS;
char *opt_inpath = NULL;
char *opt_outpath = "a.out";

static void parse_opts(int argc, char **argv) {
  for (int i = 1; i < argc; i++) {
    char *arg = argv[i];

    if (strcmp(arg, "-o") == 0) {
      if (++i >= argc)
        LOG_FATAL("not enough arguments for option '%s'", arg);

      opt_outpath = argv[i];
      continue;
    }

    if (strcmp(arg, "-dT") == 0) {
      opt_dflags |= DUMP_TOKENS;
      continue;
    }

    if (strcmp(arg, "-dA") == 0) {
      opt_dflags |= DUMP_AST;
      continue;
    }

    if (strcmp(arg, "-dIR") == 0) {
      opt_dflags |= DUMP_IR;
      continue;
    }

    if (strcmp(arg, "-dSY") == 0) {
      opt_dflags |= DUMP_SYMBOLS;
      continue;
    }

    if (strcmp(arg, "--no-fold") == 0) {
      opt_oflags ^= CONSTANT_FOLDING;
      LOG_WARN("constant folding and common subexpression elimination disabled.");
      continue;
    }

    opt_inpath = arg;
  }

  if (!opt_inpath)
    LOG_FATAL("input file is required");
}

static char *readfile(const char *filename) {
  FILE *f = NULL;
  if (!(f = fopen(filename, "r")))
    LOG_FATAL("couldn't open file %s: %s", filename, strerror(errno));

  fseek(f, 0L, SEEK_END);
  size_t file_size = ftell(f);
  rewind(f);

  char *text = calloc(file_size + 1, sizeof(char));
  if (!text)
    LOG_FATAL("no memory for readfile");

  size_t nread = fread(text, sizeof(char), file_size, f);
  if (nread != file_size)
    LOG_FATAL("only read %zu/%zu bytes from file %s\n", nread, file_size, filename);

  text[nread] = 0;
  fclose(f);

  return text;
}

void dump_tokens(Token *tokens) {
  Token *tok = tokens;
  while (tok) {
    if (tok->kind == TOK_EOF)
      break;
    printf("%.*s\n", tok->len, tok->text);
    tok = tok->next;
  }
}

void free_tokens(Token *tokens) {
  Token *tok = tokens;
  while (tok) {
    Token *next = tok->next;
    free(tok);
    tok = next;
  }
}

void dump_symbols() {
  hashmap_foreach(&SYMTAB.symbols, print_symbols);
}

void dump_ir(BasicBlock *prog) {
  BasicBlock *block = prog;
  int pc = 0;
  while (block) {
    printf("[BasicBlock %s#%d]\n", block->tag, block->id);
    Instruction *inst = block->head;
    while (inst) {
      printf(" %d | ", pc++);
      dump_instruction(inst);
      inst = inst->next;
    }
    block = block->next;
  }
}

void warn_unused(Node *ast) {
  Node *node = ast;
  while (node) {
    if (!node->visited) {
      switch (node->kind) {
        case ND_FUNC_DECL:
          LOG_WARN("unused function %s at line %d, col %d",
              node->func.name, node->span.line, node->span.col);
          break;
        case ND_VAR_DECL:
          LOG_WARN("unused variable %s at line %d, col %d",
              node->var.name, node->span.line, node->span.col);
          break;
        default: break;
      }
    }
    node = node->next;
  }
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

void assemble_target(char *obj_filepath) {
  char *asm_prog = "nasm";
  char *const asm_args[] = { asm_prog, "-felf64", "-o", obj_filepath, BUILD_ARTIFACT, NULL };
  spawn_subprocess(asm_prog, asm_args);
  LOG_INFO("finished assembling target.");
  LOG_INFO("created object file: %s", obj_filepath);
}

void link_target(char *obj_filepath, char *outpath) {
  char *link_prog = "ld";
  char *const link_args[] = { link_prog, "-o", outpath, obj_filepath, NULL };
  spawn_subprocess(link_prog, link_args);
  LOG_INFO("finished linking target.");
  LOG_INFO("created binary: %s", outpath);
}

char *change_extension(char *filename, char *new_extension) {
  char *curr_extension = NULL;
  size_t filename_len = strlen(filename);
  size_t new_extension_len = strlen(new_extension);

  if ((curr_extension = strrchr(filename, '.')))
    filename_len = curr_extension - filename;

  char *result = calloc(filename_len + new_extension_len + 1, sizeof(char));
  if (!result)
    LOG_FATAL("calloc failed in change_extension");

  memcpy(result, filename, filename_len);
  memcpy(result + filename_len, new_extension, new_extension_len);
  result[filename_len + new_extension_len] = 0;

  return result;
}

void cleanup() {
}

void init_globals() {
  SYMTAB.name = "__SYMTAB__";
  SYMTAB.parent = NULL;
  hashmap_init(&SYMTAB.symbols);

  /* Add primitive data types to global scope */
  for (TypeKind ty = TY_VOID; ty <= TY_BOOL; ty++) {
    Type *primitive = &PRIMITIVES[ty];
    Symbol *symbol = symbol_new(SYM_TYPE);
    symbol->name = primitive->name;
    symbol->type = primitive;
    add_symbol(&SYMTAB, symbol);
  }
}

int main(int argc, char **argv) {
  atexit(cleanup);
  parse_opts(argc, argv);

  init_globals();

  /* Lexing */
  char *source = readfile(opt_inpath);
  Token *tokens = lex(source);
  if (opt_dflags & DUMP_TOKENS)
    dump_tokens(tokens);

  /* Parsing */
  Node *ast = parse(tokens);

  if (opt_dflags & DUMP_AST)
    dump_node(ast, 0);

  /* Constant folding optimization */
  if (opt_oflags & CONSTANT_FOLDING)
    fold_constants(ast);

  /* Free file contents & tokens */
  free_tokens(tokens);
  free(source);

  if (opt_dflags & DUMP_SYMBOLS)
    dump_symbols();

  Symbol *entry_point = find_symbol(&SYMTAB, "main", 4);
  if (!entry_point) {
    LOG_FATAL("function 'main' is missing!");
  } else if (entry_point->kind != SYM_FUNC) {
    LOG_FATAL("symbol 'main' is not a function!");
  }

  /* Control flow analysis */
  BasicBlock *prog = lower_to_ir(entry_point->node);

  if (opt_dflags & DUMP_IR)
    dump_ir(prog);

  warn_unused(ast);

  /* Codegen */
  /*
  Target target = nasm_x86_64_generate(prog);

#ifdef DEBUG
  printf("GENERATED CODE:\n%.*s", (int)target.code_size, target.code);
#endif

  FILE *outfile = fopen(BUILD_ARTIFACT, "w");
  if (!outfile) {
    LOG_FATAL("couldn't open outfile '%s' for writing: %s",
        BUILD_ARTIFACT, strerror(errno));
  }

  LOG_INFO("created temporary file: '%s'", BUILD_ARTIFACT);

  size_t nwritten = fwrite(target.code, sizeof(char), target.code_size, outfile);
  if (nwritten != target.code_size) {
    LOG_FATAL("only wrote %zu/%zu bytes of code to '%s': %s",
        nwritten, target.code_size, BUILD_ARTIFACT, strerror(errno));
  }
  fclose(outfile);

  char *obj_filepath = change_extension(opt_outpath, ".o");

  assemble_target(obj_filepath);
  link_target(obj_filepath, opt_outpath);
  free(obj_filepath);

  unlink(BUILD_ARTIFACT);
  */

  return 0;
}
