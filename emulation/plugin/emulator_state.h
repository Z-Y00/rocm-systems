// Copyright (c) 2026 Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

/// @file emulator_state.h
/// @brief Public state types used by the emulator plugin API.

#pragma once

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum emulator_instruction_limits {
  EMULATOR_INSTRUCTION_MAX_SRC_OPERANDS = 4,
  EMULATOR_INSTRUCTION_MAX_DST_OPERANDS = 2,
} emulator_instruction_limits_t;

typedef enum emulator_instruction_flag {
  EMULATOR_INSTRUCTION_FLAG_BRANCH = 1,
  EMULATOR_INSTRUCTION_FLAG_COND_BRANCH = 1 << 1,
  EMULATOR_INSTRUCTION_FLAG_INDIRECT_BRANCH = 1 << 2,
  EMULATOR_INSTRUCTION_FLAG_INDIRECT_CALL = 1 << 3,
  EMULATOR_INSTRUCTION_FLAG_PROGRAM_TERMINATOR = 1 << 4,
  EMULATOR_INSTRUCTION_FLAG_IMMEDIATELY_EXECUTED = 1 << 5,
  EMULATOR_INSTRUCTION_FLAG_MEMORY_OP = 1 << 6,
  EMULATOR_INSTRUCTION_FLAG_WAITCNT = 1 << 7,
  EMULATOR_INSTRUCTION_FLAG_BARRIER = 1 << 8,
  EMULATOR_INSTRUCTION_FLAG_MFMA = 1 << 9,
  EMULATOR_INSTRUCTION_FLAG_ACCVGPR = 1 << 10,
  EMULATOR_INSTRUCTION_FLAG_PREDICATED_DEF = 1 << 11,
} emulator_instruction_flag_t;

typedef enum emulator_wavefront_state {
  EMULATOR_WAVEFRONT_STATE_HALTED = 0,
  EMULATOR_WAVEFRONT_STATE_RUNNING = 1,
  EMULATOR_WAVEFRONT_STATE_WAITCNT = 2,
  EMULATOR_WAVEFRONT_STATE_BARRIER = 3,
  EMULATOR_WAVEFRONT_STATE_ENDING = 4,
} emulator_wavefront_state_t;

typedef enum emulator_operand_type {
  EMULATOR_OPERAND_TYPE_UNKNOWN = 0,
  EMULATOR_OPERAND_TYPE_REGISTER = 1,
  EMULATOR_OPERAND_TYPE_IMMEDIATE = 2,
  EMULATOR_OPERAND_TYPE_LITERAL = 3,
  EMULATOR_OPERAND_TYPE_LABEL = 4,
  EMULATOR_OPERAND_TYPE_WAITCNT = 5,
  EMULATOR_OPERAND_TYPE_MESSAGE = 6,
  EMULATOR_OPERAND_TYPE_SPECIAL = 7,
} emulator_operand_type_t;

typedef enum emulator_register_class {
  EMULATOR_REGISTER_CLASS_NONE = 0,
  EMULATOR_REGISTER_CLASS_SGPR = 1,
  EMULATOR_REGISTER_CLASS_VGPR = 2,
  EMULATOR_REGISTER_CLASS_ACC_VGPR = 3,
  EMULATOR_REGISTER_CLASS_EXEC = 4,
  EMULATOR_REGISTER_CLASS_VCC = 5,
  EMULATOR_REGISTER_CLASS_SCC = 6,
  EMULATOR_REGISTER_CLASS_M0 = 7,
  EMULATOR_REGISTER_CLASS_FLAT_SCRATCH = 8,
  EMULATOR_REGISTER_CLASS_TTMP = 9,
  EMULATOR_REGISTER_CLASS_PC = 10,
} emulator_register_class_t;

typedef struct {
  const char *name;
  int32_t encoding_value;
  uint32_t size_bits;
  emulator_operand_type_t type;
  emulator_register_class_t register_class;
  uint32_t register_index;
  uint32_t register_width;
  bool is_vgpr;
} emulator_operand_t;

typedef struct {
  const char *mnemonic;
  const char *disassembly;
  const uint32_t *raw_encoding;
  const emulator_operand_t *src_operands[EMULATOR_INSTRUCTION_MAX_SRC_OPERANDS];
  const emulator_operand_t *dst_operands[EMULATOR_INSTRUCTION_MAX_DST_OPERANDS];
  uint64_t flags;
  uint32_t raw_encoding_word_count;
  uint32_t size_bytes;
  uint32_t encoding_id;
  uint32_t opcode;
  uint32_t num_src_operands;
  uint32_t num_dst_operands;
} emulator_instruction_t;

typedef struct {
  uint32_t base;
  uint32_t count;
} emulator_register_allocation_t;

typedef struct {
  uint8_t vmcnt;
  uint8_t lgkmcnt;
  uint8_t expcnt;
  uint8_t vscnt;
  uint8_t dscnt;
  uint8_t kmcnt;
} emulator_wait_counters_t;

typedef struct {
  uint64_t pc;
  uint64_t exec;
  uint64_t vcc;
  uint64_t scratch_base;
  emulator_register_allocation_t sgpr_alloc;
  emulator_register_allocation_t vgpr_alloc;
  emulator_wait_counters_t wait_counters;
  uint32_t wf_id;
  uint32_t wg_id;
  uint32_t dispatch_id;
  uint32_t lds_base;
  uint32_t wf_size;
  uint32_t num_sgprs;
  uint32_t num_vgprs;
  uint32_t max_sgprs;
  uint32_t max_vgprs;
  uint32_t status;
  emulator_wavefront_state_t state;
  uint32_t m0;
  uint32_t trace_inst_count;
} emulator_wavefront_t;

typedef struct {
  const emulator_wavefront_t *wavefronts;
  uint32_t count;
} emulator_wavefront_list_t;

#ifdef __cplusplus
} // extern "C"
#endif
