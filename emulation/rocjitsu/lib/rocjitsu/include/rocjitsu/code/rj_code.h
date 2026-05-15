// Copyright (c) 2025-2026 Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

/// @file rj_code.h
/// @brief C API for instruction decoding, executables, code objects, and basic blocks.

#ifndef ROCJITSU_CODE_RJ_CODE_H_
#define ROCJITSU_CODE_RJ_CODE_H_

#include "rocjitsu/base/rj_compiler.h"
#include "rocjitsu/base/rj_status.h"

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/// @addtogroup code
/// @{

/// @brief Instruction Set Architecture (ISA) architecture identifiers.
typedef enum rj_code_arch_e {
  /// @brief Compute Data Network Architecture 1 (CDNA1).
  ROCJITSU_CODE_ARCH_CDNA1 = 0,
  /// @brief Compute Data Network Architecture 2 (CDNA2).
  ROCJITSU_CODE_ARCH_CDNA2 = 1,
  /// @brief Compute Data Network Architecture 3 (CDNA3).
  ROCJITSU_CODE_ARCH_CDNA3 = 2,
  /// @brief Compute Data Network Architecture 4 (CDNA4).
  ROCJITSU_CODE_ARCH_CDNA4 = 3,
  /// @brief Radeon DNA Architecture 1 (RDNA1, GFX10.1).
  ROCJITSU_CODE_ARCH_RDNA1 = 4,
  /// @brief Radeon DNA Architecture 2 (RDNA2, GFX10.3).
  ROCJITSU_CODE_ARCH_RDNA2 = 5,
  /// @brief Radeon DNA Architecture 3 (RDNA3, GFX11).
  ROCJITSU_CODE_ARCH_RDNA3 = 6,
  /// @brief Radeon DNA Architecture 3.5 (RDNA3.5, GFX11.5).
  ROCJITSU_CODE_ARCH_RDNA3_5 = 7,
  /// @brief Radeon DNA Architecture 4 (RDNA4, GFX12).
  ROCJITSU_CODE_ARCH_RDNA4 = 8,
  /// @brief RISC-V 32-bit integer base ISA.
  ROCJITSU_CODE_ARCH_RV32I = 9,
  /// @brief RISC-V 64-bit integer base ISA.
  ROCJITSU_CODE_ARCH_RV64I = 10,
  /// @brief Total number of supported architectures.
  ROCJITSU_CODE_ARCH_NUM_ARCHS = 11,
  /// @brief Sentinel value representing an invalid architecture.
  ROCJITSU_CODE_ARCH_INVALID = ROCJITSU_CODE_ARCH_NUM_ARCHS
} rj_code_arch_t;

/// @brief Type representing a single raw binary machine instruction word.
typedef uint32_t rj_code_binary_inst_t;

/// @brief Instruction object.
typedef struct rj_code_inst_t rj_code_inst_t;

/// @brief Opaque handle to a decoder object.
typedef struct rj_code_decoder_t rj_code_decoder_t;

/// @brief Create a decoder for the architecture specified by @p arch.
/// @param[in] arch Architecture to create a decoder for.
/// @param[out] decoder Newly created decoder handle (refcount = 0).
/// @retval ROCJITSU_STATUS_SUCCESS Decoder was created successfully.
/// @retval ROCJITSU_STATUS_INVALID_ARGUMENT if @p arch is invalid or @p decoder is NULL.
RJ_API_EXPORT rj_status_t rj_code_decoder_create(rj_code_arch_t arch, rj_code_decoder_t **decoder);

/// @brief Increment a decoder's reference count.
/// @param[in] decoder Decoder handle (may be NULL, in which case this is a no-op).
RJ_API_EXPORT void rj_code_decoder_retain(rj_code_decoder_t *decoder);

/// @brief Decrement a decoder's reference count.
///
/// @details If the decoder has been destroyed and the reference count reaches 0,
/// the backing memory is freed.
/// @param[in] decoder Decoder handle (may be NULL, in which case this is a no-op).
RJ_API_EXPORT void rj_code_decoder_release(rj_code_decoder_t *decoder);

/// @brief Mark a decoder for destruction.
///
/// @details If the reference count is already 0, frees immediately. Otherwise,
/// the decoder is freed when the last release drops the reference count to 0.
/// @param[in] decoder Decoder to destroy (may be NULL).
RJ_API_EXPORT void rj_code_decoder_destroy(rj_code_decoder_t *decoder);

/// @brief Decode an instruction from raw binary bits.
/// @param[in] decoder The decoder object to use for decoding.
/// @param[in] binary_inst Pointer to raw instruction bits in the instruction stream.
/// @param[out] inst Pointer to the newly decoded instruction object.
/// @retval ROCJITSU_STATUS_SUCCESS Instruction was decoded successfully.
/// @retval ROCJITSU_STATUS_INVALID_ARGUMENT A required argument is NULL.
/// @retval ROCJITSU_STATUS_ERROR Decoding failed.
RJ_API_EXPORT rj_status_t rj_code_decoder_decode(rj_code_decoder_t *decoder,
                                                 const rj_code_binary_inst_t *binary_inst,
                                                 rj_code_inst_t **inst);

/// @brief GPU target identifiers.
typedef enum rj_code_target_id_t {
  /// @brief gfx942 target ID (CDNA3).
  ROCJITSU_CODE_TARGET_GFX942 = 0,
  /// @brief gfx950 target ID (CDNA4).
  ROCJITSU_CODE_TARGET_GFX950 = 1,
  /// @brief Sentinel value representing an invalid target.
  ROCJITSU_CODE_TARGET_INVALID
} rj_code_target_id_t;

/// @brief Static limits on the number of operands an instruction may have.
typedef enum rj_code_inst_limits_e {
  /// @brief Maximum source operands per instruction.
  ROCJITSU_CODE_INST_MAX_SRC_OPERANDS = 4,
  /// @brief Maximum destination operands per instruction.
  ROCJITSU_CODE_INST_MAX_DST_OPERANDS = 2,
} rj_code_inst_limits_t;

/// @brief Instruction property flags.
/// @details Each flag is a single bit in a bitmask. Multiple flags can be
/// combined with bitwise OR to describe an instruction's properties.
/// Stored as a bitmask in @ref rj_code_inst_state_t::flags.
typedef enum rj_code_inst_flag_e {
  /// @brief Unconditional branch.
  ROCJITSU_CODE_INST_FLAG_BRANCH = 1,
  /// @brief Conditional branch.
  ROCJITSU_CODE_INST_FLAG_COND_BRANCH = 1 << 1,
  /// @brief Indirect branch (target from register).
  ROCJITSU_CODE_INST_FLAG_INDIRECT_BRANCH = 1 << 2,
  /// @brief Indirect call (target from register, returns to fallthrough).
  ROCJITSU_CODE_INST_FLAG_INDIRECT_CALL = 1 << 3,
  /// @brief Terminates the program (e.g. s_endpgm).
  ROCJITSU_CODE_INST_FLAG_PROGRAM_TERMINATOR = 1 << 4,
  /// @brief Executes immediately without scheduling latency.
  ROCJITSU_CODE_INST_FLAG_IMMEDIATELY_EXECUTED = 1 << 5,
  /// @brief Memory operation (load or store).
  ROCJITSU_CODE_INST_FLAG_MEMORY_OP = 1 << 6,
  /// @brief Wait-counter instruction (s_waitcnt, s_wait_loadcnt, s_wait_storecnt, etc.).
  ROCJITSU_CODE_INST_FLAG_WAITCNT = 1 << 7,
  /// @brief Barrier instruction (s_barrier, s_barrier_signal, s_barrier_wait).
  ROCJITSU_CODE_INST_FLAG_BARRIER = 1 << 8,
  /// @brief Matrix FMA instruction (v_mfma_*, v_smfmac_*).
  ROCJITSU_CODE_INST_FLAG_MFMA = 1 << 9,
  /// @brief AccVGPR move instruction (v_accvgpr_write, v_accvgpr_read, v_accvgpr_mov).
  ROCJITSU_CODE_INST_FLAG_ACCVGPR = 1 << 10,
  /// @brief Destination update is conditional and must not kill the old value.
  ROCJITSU_CODE_INST_FLAG_PREDICATED_DEF = 1 << 11,
} rj_code_inst_flag_t;

/// @brief High-level operand category.
///
/// @details Operands that are literals, labels, waitcnt immediates, message
/// IDs, and other non-register values should not produce a register reference;
/// use the dedicated enumerator instead of @c REGISTER for those cases.
typedef enum rj_code_operand_type_e {
  ROCJITSU_CODE_OPERAND_TYPE_UNKNOWN = 0,   ///< Unclassified or not applicable.
  ROCJITSU_CODE_OPERAND_TYPE_REGISTER = 1,  ///< Operand references a register file entry.
  ROCJITSU_CODE_OPERAND_TYPE_IMMEDIATE = 2, ///< Inline immediate encoded in the instruction word.
  ROCJITSU_CODE_OPERAND_TYPE_LITERAL = 3,   ///< 32-bit literal stored in a following word.
  ROCJITSU_CODE_OPERAND_TYPE_LABEL = 4,     ///< Branch label / PC-relative offset.
  ROCJITSU_CODE_OPERAND_TYPE_WAITCNT = 5,   ///< Encoded wait-counter immediate.
  ROCJITSU_CODE_OPERAND_TYPE_MESSAGE = 6,   ///< Encoded sendmsg/barrier message ID.
  ROCJITSU_CODE_OPERAND_TYPE_SPECIAL = 7,   ///< Architectural special value (e.g., constants 0/1/-1).
} rj_code_operand_type_t;

/// @brief ISA-independent register-file class.
///
/// @details Each class has its own namespace. For example SGPR 4 and VGPR 4
/// are different registers, so they must not collide. The enum is
/// deliberately small and hardware-oriented; operands that are literals,
/// labels, waitcnt immediates, message IDs, and other non-register values
/// should not produce a register reference (use @c NONE).
typedef enum rj_code_register_class_e {
  ROCJITSU_CODE_REGISTER_CLASS_NONE = 0,         ///< Operand does not name a register.
  ROCJITSU_CODE_REGISTER_CLASS_SGPR = 1,         ///< Scalar general-purpose register, indexed as sN.
  ROCJITSU_CODE_REGISTER_CLASS_VGPR = 2,         ///< Vector general-purpose register, indexed as vN.
  ROCJITSU_CODE_REGISTER_CLASS_ACC_VGPR = 3,     ///< CDNA accumulator VGPR, indexed as accN.
  ROCJITSU_CODE_REGISTER_CLASS_EXEC = 4,         ///< EXEC mask.
  ROCJITSU_CODE_REGISTER_CLASS_VCC = 5,          ///< VCC condition mask.
  ROCJITSU_CODE_REGISTER_CLASS_SCC = 6,          ///< Scalar condition code bit.
  ROCJITSU_CODE_REGISTER_CLASS_M0 = 7,           ///< M0 special scalar register.
  ROCJITSU_CODE_REGISTER_CLASS_FLAT_SCRATCH = 8, ///< Flat-scratch base pair.
  ROCJITSU_CODE_REGISTER_CLASS_TTMP = 9,         ///< Trap-temporary registers.
  ROCJITSU_CODE_REGISTER_CLASS_PC = 10,          ///< Program counter/control-flow dependency.
} rj_code_register_class_t;

/// @brief Public POD state of an instruction operand.
///
/// @details Backs the C++ @c rocjitsu::Operand. Register-typed operands fill
/// @ref register_class, @ref register_index, @ref register_width, and
/// @ref is_vgpr; non-register operands leave the register fields zero/NONE and
/// rely on @ref type for classification.
typedef struct rj_code_operand_t {
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
  rj_code_operand_type_t type;
  /// @brief Register-file class, or @c ROCJITSU_CODE_REGISTER_CLASS_NONE for
  ///        non-register operands.
  rj_code_register_class_t register_class;
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
} rj_code_operand_t;

/// @brief Public POD state of a decoded instruction.
///
/// @details Backs the C++ @c rocjitsu::Instruction's public state. Operand
/// pointers are non-owning and reference operand storage owned by the
/// instruction or its encoding base class.
typedef struct rj_code_inst_state_t {
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
  const rj_code_operand_t *src_operands[ROCJITSU_CODE_INST_MAX_SRC_OPERANDS];
  /// @brief Destination operands, populated up to @ref num_dst_operands.
  const rj_code_operand_t *dst_operands[ROCJITSU_CODE_INST_MAX_DST_OPERANDS];
  /// @brief Property bitmask of @ref rj_code_inst_flag_t values.
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
} rj_code_inst_state_t;

/// @brief Opaque handle to an executable (x86 HIP fat binary or standalone device ELF).
typedef struct rj_code_executable_t rj_code_executable_t;

/// @brief Load an executable from a file.
/// @param[in] path Path to the executable file.
/// @param[out] exec Handle to the newly created executable (refcount = 0; caller owns it).
/// @returns ROCJITSU_STATUS_SUCCESS on success.
/// @retval ROCJITSU_STATUS_INVALID_ARGUMENT if @p path or @p exec is NULL.
RJ_API_EXPORT rj_status_t rj_code_executable_create(const char *path, rj_code_executable_t **exec);

/// @brief Increment the executable's reference count.
/// @param[in] exec Executable handle (may be NULL, in which case this is a no-op).
RJ_API_EXPORT void rj_code_executable_retain(rj_code_executable_t *exec);

/// @brief Decrement the executable's reference count.
///
/// @details If the executable has been destroyed and the reference count reaches 0,
/// the backing memory is freed.
/// @param[in] exec Executable handle (may be NULL, in which case this is a no-op).
RJ_API_EXPORT void rj_code_executable_release(rj_code_executable_t *exec);

/// @brief Mark an executable for destruction.
///
/// @details If the reference count is already 0, frees immediately. Otherwise,
/// the executable is freed when the last release drops the reference count to 0.
/// @param[in] exec Executable to destroy (may be NULL).
RJ_API_EXPORT void rj_code_executable_destroy(rj_code_executable_t *exec);

/// @brief Return the number of code objects for the given target within this executable.
/// @param[in] exec Executable to query.
/// @param[in] target Target architecture to filter by.
/// @returns Number of code objects matching @p target.
RJ_API_EXPORT uint32_t rj_code_executable_num_code_objects(const rj_code_executable_t *exec,
                                                           rj_code_target_id_t target);

/// @brief Opaque handle to a code object (single AMD GPU HSA device ELF).
typedef struct rj_code_object_t rj_code_object_t;

/// @brief Get a code object from an executable.
/// @details Allocates a new handle and retains it (refcount = 1). Caller must call
/// rj_code_object_destroy() then rj_code_object_release() when done.
/// @param[in] exec The executable containing the code object.
/// @param[in] target Target architecture to filter by.
/// @param[in] index Index of the code object for that target.
/// @param[out] obj Handle to the code object (refcount = 1; caller owns it).
/// @returns ROCJITSU_STATUS_SUCCESS on success.
/// @retval ROCJITSU_STATUS_INVALID_ARGUMENT if @p index is out of range or @p obj is NULL.
RJ_API_EXPORT rj_status_t rj_code_executable_get_code_object(const rj_code_executable_t *exec,
                                                             rj_code_target_id_t target,
                                                             uint32_t index,
                                                             rj_code_object_t **obj);

/// @brief Increment a code object's reference count.
/// @param[in] obj Code object handle (may be NULL, in which case this is a no-op).
RJ_API_EXPORT void rj_code_object_retain(rj_code_object_t *obj);

/// @brief Decrement a code object's reference count.
///
/// @details If the code object has been destroyed and the reference count reaches 0,
/// the backing memory is freed.
/// @param[in] obj Code object handle (may be NULL, in which case this is a no-op).
RJ_API_EXPORT void rj_code_object_release(rj_code_object_t *obj);

/// @brief Mark a code object for destruction.
///
/// @details If the reference count is already 0, frees immediately. Otherwise,
/// the code object is freed when the last release drops the reference count to 0.
/// @param[in] obj Code object to destroy (may be NULL).
RJ_API_EXPORT void rj_code_object_destroy(rj_code_object_t *obj);

/// @brief Opaque handle to a list of decoded instructions.
typedef struct rj_code_inst_list_t rj_code_inst_list_t;

/// @brief Create an instruction list from a code object.
/// @param[in] obj Code object to decode instructions from.
/// @param[in] target_id Target architecture for decoding.
/// @param[out] inst_list Handle to the newly created instruction list (refcount = 0).
/// @returns ROCJITSU_STATUS_SUCCESS on success.
RJ_API_EXPORT rj_status_t rj_code_inst_list_create(rj_code_object_t *obj,
                                                   rj_code_target_id_t target_id,
                                                   rj_code_inst_list_t **inst_list);

/// @brief Increment an instruction list's reference count.
/// @param[in] inst_list Instruction list handle (may be NULL, in which case this is a no-op).
RJ_API_EXPORT void rj_code_inst_list_retain(rj_code_inst_list_t *inst_list);

/// @brief Decrement an instruction list's reference count.
///
/// @details If the instruction list has been destroyed and the reference count reaches 0,
/// the backing memory is freed.
/// @param[in] inst_list Instruction list handle (may be NULL, in which case this is a no-op).
RJ_API_EXPORT void rj_code_inst_list_release(rj_code_inst_list_t *inst_list);

/// @brief Mark an instruction list for destruction.
///
/// @details If the reference count is already 0, frees immediately. Otherwise,
/// the instruction list is freed when the last release drops the reference count to 0.
/// @param[in] inst_list Instruction list to destroy (may be NULL).
RJ_API_EXPORT void rj_code_inst_list_destroy(rj_code_inst_list_t *inst_list);

/// @brief Opaque handle to a list of basic blocks.
typedef struct rj_code_basic_block_list_t rj_code_basic_block_list_t;

/// @brief Opaque handle to a single basic block.
typedef struct rj_code_basic_block_t rj_code_basic_block_t;

/// @brief Create a list of basic blocks from a code object's .text sections.
/// @param[in] obj Code object to analyze.
/// @param[in] target_id Target architecture for decoding.
/// @param[out] list Handle to the newly created basic block list (refcount = 0; caller owns it).
/// @returns ROCJITSU_STATUS_SUCCESS on success.
RJ_API_EXPORT rj_status_t rj_code_basic_block_list_create(rj_code_object_t *obj,
                                                          rj_code_target_id_t target_id,
                                                          rj_code_basic_block_list_t **list);

/// @brief Increment the basic block list's reference count.
/// @param[in] list Basic block list handle (may be NULL, in which case this is a no-op).
RJ_API_EXPORT void rj_code_basic_block_list_retain(rj_code_basic_block_list_t *list);

/// @brief Decrement the basic block list's reference count.
///
/// @details If the list has been destroyed and the reference count reaches 0,
/// the backing memory is freed.
/// @param[in] list Basic block list handle (may be NULL, in which case this is a no-op).
RJ_API_EXPORT void rj_code_basic_block_list_release(rj_code_basic_block_list_t *list);

/// @brief Mark a basic block list for destruction.
///
/// @details If the reference count is already 0, frees immediately. Otherwise,
/// the list is freed when the last release drops the reference count to 0.
/// @param[in] list Basic block list to destroy (may be NULL).
RJ_API_EXPORT void rj_code_basic_block_list_destroy(rj_code_basic_block_list_t *list);

/// @brief Return the number of basic blocks in the list.
/// @param[in] list Basic block list to query.
/// @returns Number of basic blocks.
RJ_API_EXPORT uint32_t rj_code_basic_block_list_size(const rj_code_basic_block_list_t *list);

/// @brief Get a basic block by index.
/// @details Allocates a new handle and retains it (refcount = 1). Caller must call
/// rj_code_basic_block_destroy() then rj_code_basic_block_release() when done.
/// @param[in] list Basic block list to index into.
/// @param[in] index Zero-based index of the basic block.
/// @param[out] block Handle to the basic block (refcount = 1; caller owns it).
/// @returns ROCJITSU_STATUS_SUCCESS on success.
/// @retval ROCJITSU_STATUS_INVALID_ARGUMENT if @p index is out of range or @p block is NULL.
RJ_API_EXPORT rj_status_t rj_code_basic_block_list_get(const rj_code_basic_block_list_t *list,
                                                       uint32_t index,
                                                       rj_code_basic_block_t **block);

/// @brief Increment a basic block's reference count.
/// @param[in] block Basic block handle (may be NULL, in which case this is a no-op).
RJ_API_EXPORT void rj_code_basic_block_retain(rj_code_basic_block_t *block);

/// @brief Decrement a basic block's reference count.
///
/// @details If the block has been destroyed and the reference count reaches 0,
/// the backing memory is freed.
/// @param[in] block Basic block handle (may be NULL, in which case this is a no-op).
RJ_API_EXPORT void rj_code_basic_block_release(rj_code_basic_block_t *block);

/// @brief Mark a basic block for destruction.
///
/// @details If the reference count is already 0, frees immediately. Otherwise,
/// the block is freed when the last release drops the reference count to 0.
/// @param[in] block Basic block to destroy (may be NULL).
RJ_API_EXPORT void rj_code_basic_block_destroy(rj_code_basic_block_t *block);

/// @brief Return the byte offset of the basic block's first instruction within the .text section.
/// @param[in] block Basic block to query.
/// @returns Byte offset from the start of the .text section.
RJ_API_EXPORT uint64_t rj_code_basic_block_start_offset(const rj_code_basic_block_t *block);

/// @brief Return the total size of the basic block in bytes.
/// @param[in] block Basic block to query.
/// @returns Size in bytes.
RJ_API_EXPORT uint32_t rj_code_basic_block_size(const rj_code_basic_block_t *block);

/// @brief Return the number of instructions in the basic block.
/// @param[in] block Basic block to query.
/// @returns Number of instructions.
RJ_API_EXPORT uint32_t rj_code_basic_block_num_instructions(const rj_code_basic_block_t *block);

/// @brief Get the mnemonic string for an instruction.
/// @param[in] inst Instruction to query.
/// @returns Null-terminated mnemonic string. The pointer is valid for the lifetime of the
/// instruction.
RJ_API_EXPORT const char *rj_code_inst_mnemonic(const rj_code_inst_t *inst);

/// @brief Get the size of an instruction in bytes.
/// @param[in] inst Instruction to query.
/// @returns Size in bytes.
RJ_API_EXPORT uint32_t rj_code_inst_size(const rj_code_inst_t *inst);

/// @brief Get the flags for an instruction.
/// @param[in] inst Instruction to query.
/// @returns Bitmask of @ref rj_code_inst_flag_t values.
RJ_API_EXPORT uint32_t rj_code_inst_flags(const rj_code_inst_t *inst);

/// @brief Disassemble an instruction into a string buffer.
/// @param[in] inst Instruction to disassemble.
/// @param[out] buf Output buffer for the disassembly string.
/// @param[in] buf_size Size of the output buffer in bytes.
/// @returns ROCJITSU_STATUS_SUCCESS on success.
RJ_API_EXPORT rj_status_t rj_code_inst_disassemble(const rj_code_inst_t *inst, char *buf,
                                                   uint32_t buf_size);

/// @brief Get the first instruction in a basic block.
/// @param[in] block Basic block to query.
/// @returns Pointer to the first instruction, or NULL if the block is empty.
RJ_API_EXPORT const rj_code_inst_t *
rj_code_basic_block_first_inst(const rj_code_basic_block_t *block);

/// @brief Get the next instruction in the same basic block.
/// @param[in] inst Current instruction.
/// @returns Pointer to the next instruction, or NULL if at the end of the block.
RJ_API_EXPORT const rj_code_inst_t *rj_code_inst_next(const rj_code_inst_t *inst);

/// @}

/// @defgroup dbt Dynamic Binary Translation
/// @{

typedef struct rj_code_dbt_options_t {
  rj_code_arch_t guest_arch;
  rj_code_arch_t host_arch;
} rj_code_dbt_options_t;

/// @brief Translate a code object from guest_arch to host_arch.
/// @param[in]  source     Source code object to translate.
/// @param[in]  options    Translation options.
/// @param[out] translated Newly created translated code object (refcount = 0; caller owns it).
/// @returns ROCJITSU_STATUS_SUCCESS on success.
[[nodiscard]] RJ_API_EXPORT rj_status_t rj_code_translate(const rj_code_object_t *source,
                                                          const rj_code_dbt_options_t *options,
                                                          rj_code_object_t **translated);

/// @}

#ifdef __cplusplus
} // extern "C"
#endif

#endif // ROCJITSU_CODE_RJ_CODE_H_
