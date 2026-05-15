// Copyright (c) 2025-2026 Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

/// @file rj_vm.h
/// @brief Public C API for creating and running a rocjitsu virtual machine.

#ifndef ROCJITSU_VM_RJ_VM_H_
#define ROCJITSU_VM_RJ_VM_H_

#include "rocjitsu/base/rj_compiler.h"
#include "rocjitsu/base/rj_status.h"

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/// @addtogroup vm
/// @{

/// @brief Wavefront execution state.
typedef enum rj_vm_wavefront_state_e {
  ROCJITSU_VM_WAVEFRONT_STATE_HALTED = 0,  ///< Slot is currently unused and is available for dispatch.
  ROCJITSU_VM_WAVEFRONT_STATE_RUNNING = 1, ///< In a running state and can be considered for scheduling.
  ROCJITSU_VM_WAVEFRONT_STATE_WAITCNT = 2, ///< Stalled at a waitcnt.
  ROCJITSU_VM_WAVEFRONT_STATE_BARRIER = 3, ///< Stalled at a barrier.
  ROCJITSU_VM_WAVEFRONT_STATE_ENDING = 4,  ///< s_endpgm executed but outstanding memory ops are draining.
} rj_vm_wavefront_state_t;

/// @brief Allocation slice within a register file.
typedef struct rj_vm_register_allocation_t {
  uint32_t base;  ///< First register index in the physical file.
  uint32_t count; ///< Number of registers allocated.
} rj_vm_register_allocation_t;

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
typedef struct rj_vm_wait_counters_t {
  uint8_t vmcnt;   ///< VMEM load count (GFX9/10) / loadcnt alias (GFX11+).
  uint8_t lgkmcnt; ///< LDS+GDS+K+M count (GFX9/10) / sum of dscnt+kmcnt (GFX11+).
  uint8_t expcnt;  ///< Export count (all ISAs).
  uint8_t vscnt;   ///< Vector store count (GFX10) / storecnt alias (GFX11+).
  uint8_t dscnt;   ///< DS (LDS/GDS) count (GFX11+).
  uint8_t kmcnt;   ///< Scalar/constant memory count (GFX11+).
} rj_vm_wait_counters_t;

/// @brief AMDGPU wavefront execution snapshot.
///
/// @details Backs the C++ @c rocjitsu::amdgpu::Wavefront's POD state. Each
/// wavefront is bound to a CU slot identified by @ref wf_id; dynamic
/// dispatch state (wg_id, pc, register allocations, execution masks) is
/// set when the slot is activated and reset when the slot is recycled.
typedef struct rj_vm_wavefront_t {
  uint64_t pc;                              ///< Program counter.
  uint64_t exec;                            ///< EXEC mask -- one bit per lane (1 = active).
  uint64_t vcc;                             ///< Vector condition code (per-lane comparison result).
  uint64_t scratch_base;                    ///< Per-wavefront scratch (private segment) base address.
  rj_vm_register_allocation_t sgpr_alloc;   ///< Slice in CU's SGPR file.
  rj_vm_register_allocation_t vgpr_alloc;   ///< Slice in CU's VGPR file.
  rj_vm_wait_counters_t wait_counters;      ///< Outstanding memory operation counters.
  uint32_t wf_id;                           ///< Slot index within the CU (permanent).
  uint32_t wg_id;                           ///< Workgroup ID (set per dispatch).
  uint32_t dispatch_id;                     ///< Dispatch ID (set per dispatch, unique per dispatch).
  uint32_t lds_base;                        ///< Per-WG LDS base offset (set per dispatch).
  uint32_t wf_size;                         ///< Lanes per wavefront (ISA-fixed).
  uint32_t num_sgprs;                       ///< Allocated scalar registers (set at dispatch).
  uint32_t num_vgprs;                       ///< Allocated vector registers (set at dispatch).
  uint32_t max_sgprs;                       ///< ISA maximum SGPRs per wavefront.
  uint32_t max_vgprs;                       ///< ISA maximum VGPRs per wavefront.
  uint32_t status;                          ///< ISA-specific status register (SCC, EXECZ, VCCZ, HALT, ...).
  rj_vm_wavefront_state_t state;            ///< Current execution state.
  uint32_t m0;                              ///< M0 special register (misc addressing).
  uint32_t trace_inst_count;                ///< Debug: instruction count for trace.
} rj_vm_wavefront_t;

/// @brief A view of multiple wavefronts.
typedef struct rj_vm_wavefront_list_t {
  const rj_vm_wavefront_t *wavefronts; ///< Pointer to the first wavefront.
  uint32_t count;                      ///< Number of valid entries in @ref wavefronts.
} rj_vm_wavefront_list_t;

/// @brief Opaque VM handle.
///
/// @details Represents a fully configured virtual machine including topology,
/// memory, loaded programs, and pending dispatches. All configuration is driven
/// by the JSON config passed to rj_vm_create or rj_vm_create_from_string.
typedef struct rj_vm_t rj_vm_t;

/// @brief Create a VM from a JSON configuration file.
///
/// @details Parses the JSON against the FlatBuffers schema, constructs the full
/// component hierarchy, and loads any program binaries, all from the configuration.
/// @param[in] json_path Path to the JSON config file.
/// @param[in] schema_path Path to the simulation_config.fbs schema.
/// @param[out] vm The newly created VM handle.
/// @retval ROCJITSU_STATUS_SUCCESS VM was created successfully.
/// @retval ROCJITSU_STATUS_INVALID_ARGUMENT A required argument is NULL.
/// @retval ROCJITSU_STATUS_INVALID_FILE The JSON or schema file could not be opened.
/// @retval ROCJITSU_STATUS_ERROR Parsing or construction failed.
RJ_API_EXPORT rj_status_t rj_vm_create(const char *json_path, const char *schema_path,
                                       rj_vm_t **vm);

/// @brief Create a VM from a JSON configuration string.
///
/// @param[in] json JSON configuration string.
/// @param[in] schema_path Path to the simulation_config.fbs schema.
/// @param[out] vm The newly created VM handle.
/// @retval ROCJITSU_STATUS_SUCCESS VM was created successfully.
/// @retval ROCJITSU_STATUS_INVALID_ARGUMENT A required argument is NULL.
/// @retval ROCJITSU_STATUS_INVALID_FILE The schema file could not be opened.
/// @retval ROCJITSU_STATUS_ERROR Parsing or construction failed.
RJ_API_EXPORT rj_status_t rj_vm_create_from_string(const char *json, const char *schema_path,
                                                   rj_vm_t **vm);

/// @brief Increment the VM's reference count.
///
/// @details Use this to share a VM handle across multiple owners. Each call
/// must be balanced by a corresponding rj_vm_release.
/// @param[in] vm VM handle (may be NULL, in which case this is a no-op).
RJ_API_EXPORT void rj_vm_retain(rj_vm_t *vm);

/// @brief Decrement the VM's reference count.
///
/// @details If the VM has been destroyed (via rj_vm_destroy) and the reference
/// count reaches 0, the backing memory is freed.
/// @param[in] vm VM handle (may be NULL, in which case this is a no-op).
RJ_API_EXPORT void rj_vm_release(rj_vm_t *vm);

/// @brief Mark a VM for destruction.
///
/// @details If the reference count is already 0, frees immediately. Otherwise,
/// the VM is freed when the last rj_vm_release drops the reference count to 0.
/// @param[in] vm VM to destroy (may be NULL).
RJ_API_EXPORT void rj_vm_destroy(rj_vm_t *vm);

/// @brief Step the entire VM by one tick.
///
/// @details Processes all simulation events at the next timestamp, advancing the
/// simulation by one tick.
/// @param[in] vm VM handle.
/// @param[out] active Non-zero if any wavefront is still executing.
/// @retval ROCJITSU_STATUS_SUCCESS Step completed successfully.
/// @retval ROCJITSU_STATUS_INVALID_ARGUMENT @p vm is NULL.
RJ_API_EXPORT rj_status_t rj_vm_step(rj_vm_t *vm, int *active);

/// @brief Run the VM to completion or until max_ticks is reached.
///
/// @details Runs the simulation to completion. Terminates when all primary
/// components signal completion, the tick limit from the configuration is
/// reached, or quiescence is detected.
/// @param[in] vm VM handle.
/// @param[out] ticks_executed Number of ticks actually executed (may be NULL).
/// @retval ROCJITSU_STATUS_SUCCESS Simulation completed successfully.
/// @retval ROCJITSU_STATUS_INVALID_ARGUMENT @p vm is NULL.
RJ_API_EXPORT rj_status_t rj_vm_run(rj_vm_t *vm, uint64_t *ticks_executed);

/// @brief Save a VM checkpoint to disk.
/// @param[in] vm VM handle.
/// @param[in] path Output file path for the checkpoint.
/// @param[in] tick Simulation tick to record as the checkpoint timestamp.
/// @retval ROCJITSU_STATUS_SUCCESS Checkpoint saved successfully.
/// @retval ROCJITSU_STATUS_INVALID_ARGUMENT @p vm or @p path is NULL.
/// @retval ROCJITSU_STATUS_ERROR Serialization or I/O failed.
RJ_API_EXPORT rj_status_t rj_vm_save_checkpoint(const rj_vm_t *vm, const char *path, uint64_t tick);

/// @brief Restore a VM from a checkpoint file.
/// @param[in] path Path to the checkpoint file.
/// @param[out] vm The restored VM handle.
/// @retval ROCJITSU_STATUS_SUCCESS VM restored successfully.
/// @retval ROCJITSU_STATUS_INVALID_ARGUMENT A required argument is NULL.
/// @retval ROCJITSU_STATUS_INVALID_FILE The checkpoint file could not be opened.
/// @retval ROCJITSU_STATUS_ERROR Deserialization failed.
RJ_API_EXPORT rj_status_t rj_vm_restore_checkpoint(const char *path, rj_vm_t **vm);

/// @}

#ifdef __cplusplus
} // extern "C"
#endif

#endif // ROCJITSU_VM_RJ_VM_H_
