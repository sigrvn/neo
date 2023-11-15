#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ir.h"
#include "util.h"

static const char *OPCODES[] = {
  [OP_ADD] = "+",
  [OP_NEG] = "-",
  [OP_SUB] = "-",
  [OP_MUL] = "*",
  [OP_DIV] = "/",
  [OP_NOT] = "!",
  [OP_CMP] = "==",
  [OP_CMP_NOT] = "!=",
  [OP_CMP_LT] = "<",
  [OP_CMP_GT] = ">",
  [OP_CMP_LT_EQ] = "<=",
  [OP_CMP_GT_EQ] = ">=",
};

typedef struct {
  int pc;
  int ntemps;
  int nblocks;

  HashMap vars;
  HashMap exprs;

  BasicBlock *head, *tail;
} IREmitter;

static void emit(IREmitter *, Node *);

static void emitter_init(IREmitter *e) {
  e->pc = e->ntemps = e->nblocks = 0;
  hashmap_init(&e->vars);
  hashmap_init(&e->exprs);
  e->head = e->tail = NULL;
}

static void emitter_deinit(IREmitter *e) {
  hashmap_free(&e->vars);
  hashmap_free(&e->exprs);
}

static char *emitter_make_temporary(IREmitter *e) {
  int temp_id = e->ntemps++;
  char *temp_name = format("$t%d", temp_id);
  return temp_name;
}

static BasicBlock *block_new(int id, char *tag) {
  BasicBlock *block = calloc(1, sizeof(BasicBlock));
  if (!block)
    LOG_FATAL("calloc failed for BasicBlock in block_new");

  block->id = id;
  block->tag = tag;

  block->head = block->tail = NULL;
  block->pred = block->succ = NULL;
  block->next = block->prev = NULL;

  return block;
}

static void emitter_add_block(IREmitter *e, char *tag) {
  BasicBlock *new_block = block_new(e->nblocks++, tag);

  if (!e->tail) {
    e->head = e->tail = new_block;
  } else {
    new_block->prev = e->tail;
    e->tail->next = new_block;
    e->tail = new_block;
  }
}

static Instruction* instruction_new(Opcode opcode, Span span) {
  Instruction *inst = calloc(1, sizeof(Instruction));
  if (!inst)
    LOG_FATAL("calloc failed in instruction_new");

  inst->opcode = opcode;
  // inst->start = inst->end = 0;
  // inst->assignee = NULL;
  // inst->nopers = 0;
  inst->span = span;
  return inst;
}

static char *encode_instruction(Instruction *inst) {
  size_t size = 1 + (sizeof(inst->operands[0]) * inst->nopers);
  char *buf = calloc(size, sizeof(char));
  if (!buf)
    LOG_FATAL("calloc failed in encode_instruction");

  memcpy(buf, &inst->opcode, 1);

  for (uint8_t i = 0; i < inst->nopers; i++)
    memcpy(buf, &inst->operands[i], sizeof(inst->operands[0]));

  uint64_t hash = fnv1a64_2(buf, size);
  char *encoded = format("%zu", hash);

  free(buf);
  return encoded;
}

static void instruction_add_operand(Instruction *inst, void *value, int kind) {
  if (inst->nopers == MAX_OPERANDS)
    LOG_FATAL("too many operands for opcode '%s'", OPCODES[inst->opcode]);

  inst->operands[inst->nopers].kind = kind;
  switch (kind) {
    case O_VALUE:
      inst->operands[inst->nopers].val = *((Value*)value);
      break;
    case O_VARIABLE:
    case O_LABEL:
      inst->operands[inst->nopers].var = (char *)value;
      break;
    default: LOG_FATAL("invalid operand kind: %d", kind);
  }

  inst->nopers++;
}

static void instruction_add_operands_from_node(IREmitter *e, Instruction *inst, Node *node) {
  switch (node->kind) {
    case ND_VALUE_EXPR:
      instruction_add_operand(inst, &node->value, O_VALUE);
      break;
    case ND_REF_EXPR:
      instruction_add_operand(inst, node->ref, O_VARIABLE);
      break;
    default:
      /* Generate temporary instruction of more complex expression and
       * assign the value to this instruction */
      emit(e, node);
      Instruction *temp = e->tail->tail;
      instruction_add_operand(inst, temp->assignee, O_VARIABLE);
  }
}

static void emitter_add_instruction(IREmitter *e, Instruction *inst) {
  BasicBlock *curr_block = e->tail;
  if (!curr_block)
    LOG_FATAL("no block to add instruction to");

  if (inst->assignee) {
    char *encoded = encode_instruction(inst);
    char *exists = (char *)hashmap_lookup(&e->exprs, encoded);
    if (exists) {
      LOG_INFO("eliminating redundant calculation for variable '%s'", inst->assignee);
      inst->opcode = OP_ASSIGN;
      inst->nopers = 0;
      memset(inst->operands, 0, sizeof(inst->operands[0]) * MAX_OPERANDS);
      instruction_add_operand(inst, exists, O_VARIABLE);
    } else {
      hashmap_insert(&e->exprs, encoded, inst->assignee);
    }
  }

  /* Add instruction to instruction list of tail block */
  if (!curr_block->tail) {
    curr_block->head = curr_block->tail = inst;
  } else {
    inst->prev = curr_block->tail;
    curr_block->tail->next = inst;
    curr_block->tail = inst;
  }

  e->pc++;
}

static void emit_function(IREmitter *e, Node *node) {
  emitter_add_block(e, node->func.name);

  Instruction *inst = instruction_new(OP_DEF, node->span);
  instruction_add_operand(inst, node->func.name, O_LABEL);

  emitter_add_instruction(e, inst);

  emit(e, node->func.params);
  emit(e, node->func.body);
}

static void emit_variable(IREmitter *e, Node *node) {
  Instruction *inst = instruction_new(OP_ASSIGN, node->span);
  inst->assignee = node->var.name;

  if (node->var.value)
    instruction_add_operands_from_node(e, inst, node->var.value);

  emitter_add_instruction(e, inst);
}

static void emit_assignment(IREmitter *e, Node *node) {
  Instruction *inst = instruction_new(OP_ASSIGN, node->span);
  inst->assignee = node->assign.name;

  instruction_add_operands_from_node(e, inst, node->assign.value);
  emitter_add_instruction(e, inst);
}

static void emit_conditional(IREmitter *e, Node *node) {
  LOG_FATAL("conditional translation to IR is not implemented yet");
}

static void emit_return(IREmitter *e, Node *node) {
  Instruction *inst = instruction_new(OP_RET, node->span);

  instruction_add_operands_from_node(e, inst, node->ret.value);
  emitter_add_instruction(e, inst);
}

static void emit_call(IREmitter *e, Node *node) {
  LOG_FATAL("call translation to IR is not implemented yet");
}

static void emit_unary_op(IREmitter *e, Node *node) {
  Instruction *inst = instruction_new(node->unary.un_op, node->span);

  instruction_add_operands_from_node(e, inst, node->unary.expr);

  /* NOTE: The line below relies on the operands created from above, do not move the order around */
  inst->assignee = emitter_make_temporary(e);
  emitter_add_instruction(e, inst);
}

static void emit_binary_op(IREmitter *e, Node *node) {
  Instruction *inst = instruction_new(node->binary.bin_op, node->span);

  instruction_add_operands_from_node(e, inst, node->binary.lhs);
  instruction_add_operands_from_node(e, inst, node->binary.rhs);

  /* NOTE: The line below relies on the operands created from above, do not move the order around */
  inst->assignee = emitter_make_temporary(e);
  emitter_add_instruction(e, inst);
}

static void emit(IREmitter *e, Node *node) {
  if (!node) return;

  node->visited = true;
  Node *next = node->next;

  switch (node->kind) {
    case ND_NOOP: free(node); break;
    case ND_FUNC_DECL: emit_function(e, node); break;
    case ND_VAR_DECL: emit_variable(e, node); break;
    case ND_ASSIGN_STMT: emit_assignment(e, node); break;
    case ND_COND_STMT: emit_conditional(e, node); break;
    case ND_RET_STMT: emit_return(e, node); break;
    case ND_CALL_EXPR: emit_call(e, node); break;
    case ND_UNARY_EXPR: emit_unary_op(e, node); break;
    case ND_BINARY_EXPR: emit_binary_op(e, node); break;
                         /* Leaf node, return itself */
    case ND_VALUE_EXPR:
    case ND_REF_EXPR:
    default: LOG_FATAL("cannot emit IR from node: %d", node->kind);
  }

  emit(e, next);
}

void calculate_live_intervals(IREmitter *e) {
  HashMap live;
  hashmap_init(&live);

  BasicBlock *block = e->tail;
  while (block) {
    Instruction *inst = block->tail;
    while (inst) {
      e->pc--;

      if (inst->assignee) {
        int end = (int)hashmap_lookup(&live, inst->assignee);
        if (e->pc > end) {
          inst->opcode = OP_DEAD;
          LOG_WARN("dead variable '%s' at line %d, col %d",
              inst->assignee, inst->span.line, inst->span.col);
          goto next;
        }

        inst->start = e->pc;
        inst->end = end;
      }

      for (int i = 0; i < inst->nopers; i++) {
        if (IS_VARIABLE(inst->operands[i]) && !hashmap_lookup(&live, inst->operands[i].var)) {
          hashmap_insert(&live, inst->operands[i].var, (void *)e->pc);
        }
      }

next:
      inst = inst->prev;
    }

    block = block->prev;
  }

  hashmap_free(&live);
}

BasicBlock *lower_to_ir(Node *node) {
  IREmitter e;
  emitter_init(&e);

  /* Create basic blocks */
  emitter_add_block(&e, "$entry");
  emit(&e, node);
  emitter_add_block(&e, "$exit");

  /* Do liveness analysis */
  calculate_live_intervals(&e);

  emitter_deinit(&e);
  return e.head;
}

void dump_operand(Operand *operand) {
  switch (operand->kind) {
    case O_VALUE:
      dump_value(&operand->val);
      break;
    case O_VARIABLE:
      printf("%s", operand->var);
      break;
    case O_LABEL:
      printf("%s", operand->label);
      break;
    default: LOG_FATAL("invalid operand kind: %d", operand->kind);
  }
}

void dump_instruction(Instruction *inst) {
  switch (inst->opcode) {
    case OP_DEF:
      assert(inst->nopers == 1);
      printf("def ");
      dump_operand(&inst->operands[0]);
      break;
    case OP_ASSIGN:
      assert(inst->nopers == 1);
      printf("  %s := ", inst->assignee);
      dump_operand(&inst->operands[0]);
      break;
    case OP_NEG:
    case OP_NOT:
      assert(inst->nopers == 1);
      printf("  %s := ", inst->assignee);
      printf(OPCODES[inst->opcode]);
      dump_operand(&inst->operands[0]);
      break;
    case OP_ADD: // Binary Ops
    case OP_SUB:
    case OP_MUL:
    case OP_DIV:
    case OP_CMP:
    case OP_CMP_NOT:
    case OP_CMP_LT:
    case OP_CMP_GT:
    case OP_CMP_LT_EQ:
    case OP_CMP_GT_EQ:
      assert(inst->nopers == 2);
      printf("  %s := ", inst->assignee);
      dump_operand(&inst->operands[0]);
      printf(OPCODES[inst->opcode]);
      dump_operand(&inst->operands[1]);
      break;
    case OP_RET:
      assert(inst->nopers == 1);
      printf("  ret ");
      dump_operand(&inst->operands[0]);
      break;
    case OP_DEAD:
      printf("  <dead @ %d:%d>\n", inst->span.line, inst->span.col);
      return;
    default:
      LOG_FATAL("invalid : %d", inst->opcode);
  }
  printf(" (start %d, end %d)\n", inst->start, inst->end);
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
