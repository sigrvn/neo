#include <errno.h>
#include <getopt.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "ast.h"
#include "codegen.h"
#include "compiler.h"
#include "lex.h"
#include "ir.h"
#include "optimize.h"
#include "parse.h"
#include "symtab.h"
#include "types.h"
#include "util.h"

#define NEO_VERSION "0.1.0"

#define DUMP_TOKENS   (1 << 1)
#define DUMP_AST      (1 << 2)
#define DUMP_SYMBOLS  (1 << 3)
#define DUMP_IR       (1 << 4)

#define DEFAULT_FEATURES (CONSTANT_FOLDING)

typedef struct {
  char *name;
  int val;
} Feature;

typedef struct {
  int dflags;
  int fflags;

  char *output;

  char **sources;
  size_t nsources;

  bool verbose;
} CompilerOpts;

#define OPTSTRING "d:o:v"
static struct option long_options[] = {
  {"dump", required_argument, 0, 'd'},
  {"feature", required_argument, 0, 'f'},
  {"output", required_argument, 0, 'o'},
  {"verbose", no_argument, 0, 'v'},
  {0, 0, 0, 0}
};

void set_dump_flag(int *dflags, const char *arg) {
  static const char *dump_map[] = {
    [DUMP_TOKENS] = "tok",
    [DUMP_AST] = "ast",
    [DUMP_SYMBOLS] = "sym",
    [DUMP_IR] = "ir",
  };

  for (int i = DUMP_TOKENS; i < DUMP_IR; i <<= 1) {
    if (strcmp(arg, dump_map[i]) == 0)
      *dflags |= i;
  }
}

void set_feature_flag(int *fflags, const char *arg) {
  static const Feature feature_map[] = {
    {"no-fold", CONSTANT_FOLDING},
  };

  for (const Feature *f = feature_map; f; f++) {
    if (strcmp(arg, f->name) == 0)
      *fflags ^= f->val;
  }
}

static CompilerOpts parse_opts(int argc, char **argv) {
  CompilerOpts opts = {
    .dflags = 0,
    .fflags = DEFAULT_FEATURES,
    .output = "a.out",
    .sources = NULL,
    .nsources = 0,
  };

  int c, idx;
  while ((c = getopt_long(argc, argv, OPTSTRING, long_options, &idx)) != -1) {
    switch (c) {
      case 'd':
        set_dump_flag(&opts.dflags, optarg);
        break;
      case 'f':
        set_feature_flag(&opts.fflags, optarg);
        break;
      case 'o':
        opts.output = optarg;
        break;
      case 'v':
        opts.verbose = true;
        break;
      case '?':
        // getopt_long already printed an error message
        break;
      default:
        LOG_FATAL("unknown option: %s", optarg);
    }
  }

  if (!(opts.sources = calloc(argc - optind, sizeof(char*))))
    LOG_FATAL("calloc failed for file sources array");

  for (int i = optind; i < argc; i++)
    opts.sources[opts.nsources++] = argv[i];

  return opts;
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
    const Type *primitive = &PRIMITIVES[ty];
    Symbol *symbol = symbol_new(SYM_TYPE);
    symbol->name = primitive->name;
    symbol->type = primitive;
    add_symbol(&SYMTAB, symbol);
  }
}

int main(int argc, char **argv) {
  atexit(cleanup);
  CompilerOpts opts = parse_opts(argc, argv);

  init_globals();

  /* Lexing */
  char *source = readfile(opts.sources[0]);
  Token *tokens = lex(source);
  if (opts.dflags & DUMP_TOKENS)
    dump_tokens(tokens);

  /* Parsing */
  Node *ast = parse(tokens);

  if (opts.dflags & DUMP_AST)
    dump_node(ast, 0);

  /* Constant folding optimization */
  if (opts.fflags & CONSTANT_FOLDING)
    fold_constants(ast);

  /* Free file contents & tokens */
  free_tokens(tokens);
  free(source);

  if (opts.dflags & DUMP_SYMBOLS)
    dump_symbols();

  Symbol *entry_point = find_symbol(&SYMTAB, "main", 4);
  if (!entry_point) {
    LOG_FATAL("function 'main' is missing!");
  } else if (entry_point->kind != SYM_FUNC) {
    LOG_FATAL("symbol 'main' is not a function!");
  }

  /* Control flow analysis */
  BasicBlock *prog = lower_to_ir(entry_point->node);

  if (opts.dflags & DUMP_IR)
    dump_ir(prog);

  warn_unused(ast);

  /* Codegen */
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

  char *obj_filepath = change_extension(opts.output, ".o");

  assemble_target(obj_filepath);
  link_target(obj_filepath, opts.output);
  free(obj_filepath);

  unlink(BUILD_ARTIFACT);

  free(opts.sources);

  return 0;
}
