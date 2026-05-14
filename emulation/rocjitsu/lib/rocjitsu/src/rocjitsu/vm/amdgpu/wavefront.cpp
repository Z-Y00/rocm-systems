// Copyright (c) 2025-2026 Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#include "rocjitsu/vm/amdgpu/wavefront.h"

#include "rocjitsu/vm/amdgpu/compute_unit.h"

namespace rocjitsu {
namespace amdgpu {

void Wavefront::halt() {
  set_state(WfState::HALTED);
  cu_.release_wf(state_.dispatch_id, state_.wg_id);
}

} // namespace amdgpu
} // namespace rocjitsu
