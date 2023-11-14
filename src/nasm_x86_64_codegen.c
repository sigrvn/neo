#include <assert.h>
#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ast.h"
#include "codegen.h"
#include "ir.h"
#include "symtab.h"
#include "util.h"

#define CODE_CAPACITY 4096

/* NASM x86_64 (Linux) */

typedef enum {
  RAX,
  RBX,
  RCX,
  RDX,
  RSP,
  RBP,
  RSI,
  RDI,
  R8,
  R9,
  R10,
  R11,
  R12,
  R13,
  R14,
  R15,
  NUM_REGISTERS
} RegisterID;

typedef struct RegisterData RegisterData;
struct RegisterData {
  int start, end;
  char *var;
  Value value;
  Type *type;
  RegisterData *next;
};

static RegisterData *regdata_new(int start, int end) {
  RegisterData *data = calloc(1, sizeof(RegisterData));
  if (!data)
    LOG_FATAL("calloc failed in regdata_new");

  data->start = start;
  data->end = end;
  data->var = NULL;
  data->type = NULL;
  data->next = NULL;
  return data;
}

typedef struct {
  RegisterID rid;
  bool active;
  RegisterData *data;
} Register;

const char *regname(Register *r) {
  switch (r->rid) {
    case RAX: return "rax";
    case RBX: return "rbx";
    case RCX: return "rcx";
    case RDX: return "rdx";
    case RSP: return "rsp";
    case RBP: return "rbp";
    case RSI: return "rsi";
    case RDI: return "rdi";
    case R8:  return  "r8";
    case R9:  return  "r9";
    case R10: return "r10";
    case R11: return "r11";
    case R12: return "r12";
    case R13: return "r13";
    case R14: return "r14";
    case R15: return "r15";
    default:  return "???";
  }
}

#define REG_MUST_PRESERVE(r) \
  ((*r).rid == RBX || (*r).rid == RSP || (*r).rid == RBP || \
   ((*r).rid >= R12 && (*r).rid <= R15))

/* Directives to allocate memory (in bytes) */
enum {
  DB = 1,
  DW = 2,
  DD = 4,
  DQ = 8,
  DO = 16,
  DY = 32,
  DZ = 64,
};

const char *init_mem[] = {
  [DB] = "db",
  [DW] = "dw",
  [DD] = "dd",
  [DQ] = "dq",
  [DO] = "do",
  [DY] = "dy",
  [DZ] = "dz",
};

enum {
  RESB = 1,
  RESD = 4,
  RESQ = 8,
};

const char *uninit_mem[] = {
  [RESB] = "resb",
  [RESD] = "resd",
  [RESQ] = "resq",
};

/* Registers */
Register registers[NUM_REGISTERS];

/* Stack */
RegisterData *stack = NULL;
size_t stack_length = 0;
size_t stack_size_bytes = 0;

/* Static Memory */

/* Code Buffer */
size_t code_size = 0;
size_t code_capacity = 0;
char *code = NULL;

#define _writeln(fmt, ...) _write(fmt"\n", ##__VA_ARGS__)

static void _write(const char *fmt, ...) {
  va_list args;

  va_start(args, fmt);
  int length = vsnprintf(NULL, 0, fmt, args);
  va_end(args);

  if (code_size + length + 1 > code_capacity) {
    code_capacity = (code_size + length) << 1;
    if (code_capacity < code_size) {
      LOG_FATAL("capacity overflow for code buffer in _write");
    }

    void *tmp = realloc(code, sizeof(char) * code_capacity);
    if (!tmp) {
      LOG_FATAL("realloc failed for code buffer in _write");
    }
    code = tmp;
  }

  char *dest = (char *)code + code_size;

  va_start(args, fmt);
  int nwritten = vsnprintf(dest, length + 1, fmt, args);
  va_end(args);

  if (nwritten != length) {
    LOG_FATAL("only wrote %d/%d bytes to code buffer", nwritten, length);
  }

  code_size += length;
}

static void _write_value(Value v) {
  switch (v.kind) {
    case VAL_INT:    _write("%d", v.i_val); break;
    case VAL_UINT:   _write("%u", v.u_val); break;
    case VAL_FLOAT:  _write("%f", v.f_val); break;
    case VAL_DOUBLE: _write("%g", v.d_val); break;
    case VAL_CHAR:   _write("%c", v.c_val); break;
    case VAL_BOOL:   _write("%d", v.b_val); break;
    case VAL_STRING: _write("%.*s", v.s_len, v.s_val); break;
  }
}

/* Simple Linear Scan Register Allocator */
static void save_register(Register *r) {
  _writeln("push %s", regname(r));

  RegisterData *data = r->data;
  if (data) {
    if (!stack) {
      stack = data;
    }
    else {
      RegisterData *next = stack;
      stack = data;
      stack->next = next;
    }

    stack_length++;
    stack_size_bytes += data->type->size;
  }
#ifdef DEBUG
  printf("-> saved register '%s' to stack\n", regname(r));
  printf("-> stack_size: %ld (%ld bytes)\n", stack_length, stack_size_bytes);
#endif
}

static void restore_register(Register *r) {
  _writeln("pop %s", regname(r));
}

static Register *find_available_register() {
  Register *r = NULL, *oldest = &registers[RAX];
  for (int rid = RAX; rid < NUM_REGISTERS; rid++) {
    r = &registers[rid];
    if (!r->active) {
      oldest = NULL;
      break;
    }

    if (r->data->end > oldest->data->end)
      oldest = r;
  }

  if (oldest)
    r = oldest;

  if (REG_MUST_PRESERVE(r))
    save_register(r);

  r->active = true;
  return r;
}

static void release_register(Register *r) {
  r->active = false;
  if (r->data)
    free(r->data);
}

static Register *find_register_by_variable(const char *var) {
  for (RegisterID rid = RAX; rid < NUM_REGISTERS; rid++) {
    Register *r = &registers[rid];
    RegisterData *data = r->data;
    if (data && data->var && strcmp(data->var, var) == 0)
      return r;
  }
  return NULL;
}

static Register *put_variable_in_register(Instruction *inst) {
  Register *r = find_available_register();
  r->data = regdata_new(inst->start, inst->end);
  r->data->var = inst->assignee;
#ifdef DEBUG
  printf("-> moved variable '%s' to register '%s'\n", inst->assignee, regname(r));
#endif
  return r;
}

static Register *compile_assign(Instruction *inst) {
  assert(inst->nopers == 1);
  assert(inst->operands[0].kind != O_UNKNOWN);
  assert(inst->operands[0].kind != O_LABEL);

  Register *src_register = NULL;
  Register *dest_register = NULL;

  dest_register = find_register_by_variable(inst->assignee);
  if (!dest_register)
    dest_register = put_variable_in_register(inst);

  _write("mov %s, ", regname(dest_register));

  switch (inst->operands[0].kind) {
    case O_VALUE:
      _write_value(inst->operands[0].val);
      _write("\n");
      break;
    case O_VARIABLE:
      src_register = find_register_by_variable(inst->operands[0].var);
      if (!src_register)
        LOG_FATAL("operand '%s' is not in any register", inst->operands[0].var);
      /* If not in a register, look for variable in global variables and load that instead */
      _write("%s\n", regname(src_register));
      break;
    default: LOG_FATAL("shouldn't have gotten here...");
  }

  return dest_register;
}

static Register *compile_add(Instruction *inst) {
  assert(inst->nopers == 2);
  assert(inst->operands[0].kind != O_UNKNOWN);
  assert(inst->operands[0].kind != O_LABEL);
  assert(inst->operands[1].kind != O_UNKNOWN);
  assert(inst->operands[1].kind != O_LABEL);

  Register *dest_register = find_register_by_variable(inst->assignee);
  if (!dest_register)
    dest_register = put_variable_in_register(inst);

  uint8_t op_idx = 0;
  if (strcmp(inst->operands[0].var, dest_register->data->var) == 0) {
    _write("add %s, ", regname(dest_register));
    op_idx = 1;
  }
  else if (strcmp(inst->operands[1].var, dest_register->data->var) == 0) {
    _write("add %s, ", regname(dest_register));
    op_idx = 0;
  }
  else {
    Instruction temp = *inst;
    temp.opcode = OP_ASSIGN;
    temp.nopers = 1;
    Register *temp_register = compile_assign(&temp);
    _write("add %s, ", regname(temp_register));
    op_idx = 1;
  }

  switch (inst->operands[op_idx].kind) {
    case O_VALUE:
      _write_value(inst->operands[op_idx].val);
      break;
    case O_VARIABLE:
      Register *src_register = find_register_by_variable(inst->operands[op_idx].var);
      if (!src_register)
        LOG_FATAL("operand '%s' is not in any register", inst->operands[op_idx].var);
      _write("%s", regname(src_register));
      break;
    default: LOG_FATAL("shouldn't have gotten here...");
  }

  _write("\n");
  return dest_register;
}

static void compile_return(Instruction *inst) {
  LOG_WARN("compile_return function does nothing");
}

static void compile_instruction(Instruction *inst) {
  switch (inst->opcode) {
    case OP_DEF:
      break;
    case OP_ASSIGN: compile_assign(inst); break;
    case OP_ADD: compile_add(inst); break;
    case OP_RET: compile_return(inst); break;
    case OP_DEAD:
      LOG_WARN("ignoring dead variable '%s' at line %d, col %d",
          inst->assignee, inst->span.line, inst->span.col);
      break;
    default:
      LOG_FATAL("compilation not supported for opcode: %d", inst->opcode);
  }
}

static void compile_block(BasicBlock *block) {
  if (!block) return;

  Instruction *inst = block->head;
  while (inst) {
    compile_instruction(inst);
    inst = inst->next;
  }
  compile_block(block->next);
}

static void alloc_global_symbols() {
  _writeln("section .bss");

  for (size_t i = 0; i < SYMTAB.symbols.capacity; i++) {
    MapEntry entry = SYMTAB.symbols.entries[i];
    if (entry.key) {
      Symbol *symbol = (Symbol *)entry.value;
      if (symbol->name && symbol->kind == SYM_VAR) {
        const Type *type = symbol->node->var.type;

        /* Try to reserve memory using the directive with GCD of the type size */
        int alloc = RESB;
        if (type->size % RESQ == 0)
          alloc = RESQ;
        else if (type->size % RESD == 0)
          alloc = RESD;

        int bytes_to_alloc = type->size / alloc;
        const char *directive = uninit_mem[alloc];

        _write("%s: %s %d\n", symbol->name, directive, bytes_to_alloc);
      }
    }
  }
}

Target nasm_x86_64_generate(BasicBlock *prog) {
  /* Initialize codegen state */
  memset(registers, 0, sizeof(Register) * NUM_REGISTERS);

  stack_length = 0;
  stack_size_bytes = 0;

  code = NULL;
  code_size = 0;
  code_capacity = 0;

  /* Allocate space for uninitialized global variables */
  alloc_global_symbols();

  _writeln("section .text");
  /* TODO: define external linkage here */

  /* Entry point of program */
  _writeln("global _start");
  _writeln("_start:");

  compile_block(prog);

  /* Exit syscall */
  _writeln("mov rdi, 0");
  _writeln("mov rax, 0x3c");
  _writeln("syscall");

  Target target = {
    .code = code,
    .code_size = code_size,
  };

  return target;
}
