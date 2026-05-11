// MIT License
//
// Copyright (c) 2017-2025 Advanced Micro Devices, Inc.
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
// THE SOFTWARE.

#include "aqlprofile-sdk/aql_profile_v2.h"
#include "def/gpu_block_info.h"
#include "core/architectures/gfx12_architecture.hpp"
#include "core/pm4_factory.h"
#include "def/gfx12_def.h"
#include "pm4/gfx12_cmd_builder.h"
#include "pm4/gfx12_pmc_builder.hpp"
#include "pm4/gfx12_primitives_provider.hpp"
#include "pm4/gfx12_spm_builder.hpp"
#include "pm4/gfx12_sqtt_builder.hpp"

namespace aql_profile {

class Gfx12Factory : public Pm4Factory {
 public:
  explicit Gfx12Factory(const AgentInfo* agent_info)
      : Pm4Factory(new Gfx12Architecture(agent_info)) {
    agent_info_ = agent_info;
    ConstructBuilders(agent_info);
  }
  bool IsGFX12() const override { return true; }

  ~Gfx12Factory() override { delete prims_; }

 private:
  pm4_builder::Gfx12PrimitivesProvider* prims_{nullptr};

  void ConstructBuilders(const AgentInfo* agent_info) {
    cmd_builder_ = new pm4_builder::Gfx12CmdBuilder(nullptr);
    if (cmd_builder_ == NULL) throw aql_profile_exc_msg("CmdBuilder allocation failed");

    prims_ = new pm4_builder::Gfx12PrimitivesProvider();
    if (prims_ == NULL) throw aql_profile_exc_msg("PrimitivesProvider allocation failed");

    const auto& config = GetArchitecture()->GetConfig();
    pmc_builder_ = new pm4_builder::Gfx12PmcBuilder(config, cmd_builder_, prims_, IsConcurrent());
    if (pmc_builder_ == NULL) throw aql_profile_exc_msg("PmcBuilder allocation failed");

    spm_builder_ = new pm4_builder::Gfx12SpmBuilder(cmd_builder_, prims_);
    if (spm_builder_ == NULL) throw aql_profile_exc_msg("SpmBuilder allocation failed");

    sqtt_builder_ = new pm4_builder::Gfx12SqttBuilder(config, cmd_builder_, prims_,
                                                       agent_info->timestamp_freq);
    if (sqtt_builder_ == NULL) throw aql_profile_exc_msg("SqttBuilder allocation failed");
  }
};

Pm4Factory* Pm4Factory::Gfx12Create(const AgentInfo* agent_info) {
  auto p = new Gfx12Factory(agent_info);
  if (p == NULL) throw aql_profile_exc_msg("Gfx12Factory allocation failed");
  return p;
}

}  // namespace aql_profile
