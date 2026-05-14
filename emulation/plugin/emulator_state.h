// Copyright (c) 2026 Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

/// @file emulator_state.h
/// @brief Public state types used by the emulator plugin API.
///
/// @details These C structs are the canonical ABI representation of emulator
/// state shared between the rocjitsu emulator runtime and plugins. The
/// matching C++ types in the rocjitsu runtime are aliased or derived from
/// these structs so the layout matches across the FFI boundary.

#pragma once

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/// @brief Static limits on the number of operands an instruction may have.
typedef enum emulator_instruction_limits {
  EMULATOR_INSTRUCTION_MAX_SRC_OPERANDS = 4, ///< Maximum source operands per instruction.
  EMULATOR_INSTRUCTION_MAX_DST_OPERANDS = 2, ///< Maximum destination operands per instruction.
} emulator_instruction_limits_t;

/// @brief Instruction property flags.
///
/// @details Stored as a bitmask in @ref emulator_instruction_t::flags.
typedef enum emulator_instruction_flag {
  /// @brief Unconditional branch.
  EMULATOR_INSTRUCTION_FLAG_BRANCH = 1,
  /// @brief Conditional branch.
  EMULATOR_INSTRUCTION_FLAG_COND_BRANCH = 1 << 1,
  /// @brief Indirect branch (target from register).
  EMULATOR_INSTRUCTION_FLAG_INDIRECT_BRANCH = 1 << 2,
  /// @brief Indirect call (target from register, returns to fallthrough).
  EMULATOR_INSTRUCTION_FLAG_INDIRECT_CALL = 1 << 3,
  /// @brief Terminates the program.
  EMULATOR_INSTRUCTION_FLAG_PROGRAM_TERMINATOR = 1 << 4,
  /// @brief Executes immediately without scheduling latency.
  EMULATOR_INSTRUCTION_FLAG_IMMEDIATELY_EXECUTED = 1 << 5,
  /// @brief Memory operation (load or store).
  EMULATOR_INSTRUCTION_FLAG_MEMORY_OP = 1 << 6,
  /// @brief Wait-counter instruction (s_waitcnt, s_wait_loadcnt, s_wait_storecnt, etc.).
  EMULATOR_INSTRUCTION_FLAG_WAITCNT = 1 << 7,
  /// @brief Barrier instruction (s_barrier, s_barrier_signal, s_barrier_wait).
  EMULATOR_INSTRUCTION_FLAG_BARRIER = 1 << 8,
  /// @brief Matrix FMA instruction (v_mfma_*, v_smfmac_*).
  EMULATOR_INSTRUCTION_FLAG_MFMA = 1 << 9,
  /// @brief AccVGPR move instruction (v_accvgpr_write, v_accvgpr_read, v_accvgpr_mov).
  EMULATOR_INSTRUCTION_FLAG_ACCVGPR = 1 << 10,
  /// @brief Destination update is conditional and must not kill the old value.
  EMULATOR_INSTRUCTION_FLAG_PREDICATED_DEF = 1 << 11,
} emulator_instruction_flag_t;

/// @brief Wavefront execution state.
typedef enum emulator_wavefront_state {
  EMULATOR_WAVEFRONT_STATE_HALTED = 0,  ///< Slot is currently unused and is available for dispatch.
  EMULATOR_WAVEFRONT_STATE_RUNNING = 1, ///< In a running state and can be considered for scheduling.
  EMULATOR_WAVEFRONT_STATE_WAITCNT = 2, ///< Stalled at a waitcnt.
  EMULATOR_WAVEFRONT_STATE_BARRIER = 3, ///< Stalled at a barrier.
  EMULATOR_WAVEFRONT_STATE_ENDING = 4,  ///< s_endpgm executed but outstanding memory ops are draining.
} emulator_wavefront_state_t;

/// @brief High-level operand category.
///
/// @details Operands that are literals, labels, waitcnt immediates, message
/// IDs, and other non-register values should not produce a register reference;
/// use the dedicated enumerator instead of @c REGISTER for those cases.
typedef enum emulator_operand_type {
  EMULATOR_OPERAND_TYPE_UNKNOWN = 0,   ///< Unclassified or not applicable.
  EMULATOR_OPERAND_TYPE_REGISTER = 1,  ///< Operand references a register file entry.
  EMULATOR_OPERAND_TYPE_IMMEDIATE = 2, ///< Inline immediate encoded in the instruction word.
  EMULATOR_OPERAND_TYPE_LITERAL = 3,   ///< 32-bit literal stored in a following word.
  EMULATOR_OPERAND_TYPE_LABEL = 4,     ///< Branch label / PC-relative offset.
  EMULATOR_OPERAND_TYPE_WAITCNT = 5,   ///< Encoded wait-counter immediate.
  EMULATOR_OPERAND_TYPE_MESSAGE = 6,   ///< Encoded sendmsg/barrier message ID.
  EMULATOR_OPERAND_TYPE_SPECIAL = 7,   ///< Architectural special value (e.g., constants 0/1/-1).
} emulator_operand_type_t;

/// @brief ISA-independent register-file class.
///
/// @details Each class has its own namespace. For example SGPR 4 and VGPR 4
/// are different registers, so they must not collide. The enum is
/// deliberately small and hardware-oriented; operands that are literals,
/// labels, waitcnt immediates, message IDs, and other non-register values
/// should not produce a register reference (use @c NONE).
typedef enum emulator_register_class {
  EMULATOR_REGISTER_CLASS_NONE = 0,         ///< Operand does not name a register.
  EMULATOR_REGISTER_CLASS_SGPR = 1,         ///< Scalar general-purpose register, indexed as sN.
  EMULATOR_REGISTER_CLASS_VGPR = 2,         ///< Vector general-purpose register, indexed as vN.
  EMULATOR_REGISTER_CLASS_ACC_VGPR = 3,     ///< CDNA accumulator VGPR, indexed as accN.
  EMULATOR_REGISTER_CLASS_EXEC = 4,         ///< EXEC mask.
  EMULATOR_REGISTER_CLASS_VCC = 5,          ///< VCC condition mask.
  EMULATOR_REGISTER_CLASS_SCC = 6,          ///< Scalar condition code bit.
  EMULATOR_REGISTER_CLASS_M0 = 7,           ///< M0 special scalar register.
  EMULATOR_REGISTER_CLASS_FLAT_SCRATCH = 8, ///< Flat-scratch base pair.
  EMULATOR_REGISTER_CLASS_TTMP = 9,         ///< Trap-temporary registers.
  EMULATOR_REGISTER_CLASS_PC = 10,          ///< Program counter/control-flow dependency.
} emulator_register_class_t;

/// @brief An instruction operand.
///
/// @details Backs the C++ @c rocjitsu::Operand. Register-typed operands fill
/// @ref register_class, @ref register_index, @ref register_width, and
/// @ref is_vgpr; non-register operands leave the register fields zero/NONE and
/// rely on @ref type for classification.
typedef struct {
  /// @brief Human-readable name for this operand (e.g. "v0", "s4", or a literal).
  ///
  /// @details Must point to storage that outlives the operand (typically a
  /// string literal or a static buffer). May be NULL for synthetic operands.
  const char *name;
  /// @brief Raw encoding value from the instruction binary.
  int32_t encoding_value;
  /// @brief Operand width in bits.
  uint32_t size_bits;
  /// @brief High-level operand category.
  emulator_operand_type_t type;
  /// @brief Register-file class, or @c EMULATOR_REGISTER_CLASS_NONE for
  ///        non-register operands.
  emulator_register_class_t register_class;
  /// @brief First register index within @ref register_class.
  uint32_t register_index;
  /// @brief Number of consecutive 32-bit register lanes covered.
  uint32_t register_width;
  /// @brief Whether this operand references a VGPR or AccVGPR.
  ///
  /// @details Classified at construction time using the ISA-specific
  /// operand-type tables. Convenience flag — equivalent to
  /// `register_class == VGPR || register_class == ACC_VGPR`.
  bool is_vgpr;
} emulator_operand_t;

/// @brief A decoded instruction.
///
/// @details Backs the C++ @c rocjitsu::Instruction's public state. Operand
/// pointers are non-owning and reference operand storage owned by the
/// instruction or its encoding base class.
typedef struct {
  /// @brief Human-readable mnemonic (e.g. "v_add_u32").
  ///
  /// @details Points to static storage or storage that outlives the
  /// instruction — typically a string literal or a member of the encoding
  /// base class.
  const char *mnemonic;
  /// @brief Cached disassembly string, or NULL if not yet built.
  ///
  /// @details Lazily produced on first request. Includes mnemonic, operands,
  /// and any modifier flags.
  const char *disassembly;
  /// @brief Pointer to the raw encoding words.
  ///
  /// @details Length is @ref raw_encoding_word_count. May be NULL if the
  /// instruction has no backing binary encoding.
  const uint32_t *raw_encoding;
  /// @brief Source operands, populated up to @ref num_src_operands.
  const emulator_operand_t *src_operands[EMULATOR_INSTRUCTION_MAX_SRC_OPERANDS];
  /// @brief Destination operands, populated up to @ref num_dst_operands.
  const emulator_operand_t *dst_operands[EMULATOR_INSTRUCTION_MAX_DST_OPERANDS];
  /// @brief Property bitmask of @ref emulator_instruction_flag values.
  uint64_t flags;
  /// @brief Number of valid words pointed to by @ref raw_encoding.
  uint32_t raw_encoding_word_count;
  /// @brief Size of the instruction's encoding in bytes.
  uint32_t size_bytes;
  /// @brief Encoding format ID (the encoding prefix from the machine instruction).
  uint32_t encoding_id;
  /// @brief Opcode within the encoding format.
  uint32_t opcode;
  /// @brief Number of valid entries in @ref src_operands.
  uint32_t num_src_operands;
  /// @brief Number of valid entries in @ref dst_operands.
  uint32_t num_dst_operands;
} emulator_instruction_t;

/// @brief Allocation slice within a register file.
typedef struct {
  uint32_t base;  ///< First register index in the physical file.
  uint32_t count; ///< Number of registers allocated.
} emulator_register_allocation_t;

/// @brief Outstanding memory operation counters for a wavefront.
///
/// @details Unified counter set that covers all AMDGPU ISA families. CDNA1-4
/// use only vmcnt/lgkmcnt/expcnt. RDNA1/2 add vscnt. RDNA3/3.5/4 use the
/// fine-grained counters (loadcnt/storecnt/dscnt/kmcnt/expcnt) which are
/// stored in the same fields as aliases:
///   loadcnt → vmcnt, storecnt → vscnt, dscnt + kmcnt → lgkmcnt.
///
/// For RDNA3+, the split counters (dscnt, kmcnt) are tracked independently
/// but their sum is also reflected in lgkmcnt for backward compatibility
/// with the monolithic S_WAITCNT instruction that RDNA3/3.5 still support.
typedef struct {
  uint8_t vmcnt;   ///< VMEM load count (GFX9/10) / loadcnt alias (GFX11+).
  uint8_t lgkmcnt; ///< LDS+GDS+K+M count (GFX9/10) / sum of dscnt+kmcnt (GFX11+).
  uint8_t expcnt;  ///< Export count (all ISAs).
  uint8_t vscnt;   ///< Vector store count (GFX10) / storecnt alias (GFX11+).
  uint8_t dscnt;   ///< DS (LDS/GDS) count (GFX11+).
  uint8_t kmcnt;   ///< Scalar/constant memory count (GFX11+).
} emulator_wait_counters_t;

/// @brief AMDGPU wavefront execution snapshot.
///
/// @details Backs the C++ @c rocjitsu::amdgpu::Wavefront's POD state. Each
/// wavefront is bound to a CU slot identified by @ref wf_id; dynamic
/// dispatch state (wg_id, pc, register allocations, execution masks) is
/// set when the slot is activated and reset when the slot is recycled.
typedef struct {
  uint64_t pc;                                 ///< Program counter.
  uint64_t exec;                               ///< EXEC mask -- one bit per lane (1 = active).
  uint64_t vcc;                                ///< Vector condition code (per-lane comparison result).
  uint64_t scratch_base;                       ///< Per-wavefront scratch (private segment) base address.
  emulator_register_allocation_t sgpr_alloc;   ///< Slice in CU's SGPR file.
  emulator_register_allocation_t vgpr_alloc;   ///< Slice in CU's VGPR file.
  emulator_wait_counters_t wait_counters;      ///< Outstanding memory operation counters.
  uint32_t wf_id;                              ///< Slot index within the CU (permanent).
  uint32_t wg_id;                              ///< Workgroup ID (set per dispatch).
  uint32_t dispatch_id;                        ///< Dispatch ID (set per dispatch, unique per dispatch).
  uint32_t lds_base;                           ///< Per-WG LDS base offset (set per dispatch).
  uint32_t wf_size;                            ///< Lanes per wavefront (ISA-fixed).
  uint32_t num_sgprs;                          ///< Allocated scalar registers (set at dispatch).
  uint32_t num_vgprs;                          ///< Allocated vector registers (set at dispatch).
  uint32_t max_sgprs;                          ///< ISA maximum SGPRs per wavefront.
  uint32_t max_vgprs;                          ///< ISA maximum VGPRs per wavefront.
  uint32_t status;                             ///< ISA-specific status register (SCC, EXECZ, VCCZ, HALT, ...).
  emulator_wavefront_state_t state;            ///< Current execution state.
  uint32_t m0;                                 ///< M0 special register (misc addressing).
  uint32_t trace_inst_count;                   ///< Debug: instruction count for trace.
} emulator_wavefront_t;

/// @brief A view of multiple wavefronts.
typedef struct {
  const emulator_wavefront_t *wavefronts; ///< Pointer to the first wavefront.
  uint32_t count;                         ///< Number of valid entries in @ref wavefronts.
} emulator_wavefront_list_t;

#ifdef __cplusplus
} // extern "C"
#endif
