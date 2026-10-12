/**
 * slang - a simple scripting language.
 *
 * Control Flow Graph analysis for JIT compiler.
 *
 * \author Felix Lubbe
 * \copyright Copyright (c) 2026
 * \license Distributed under the MIT software license (see accompanying LICENSE.txt).
 */

#pragma once

#include <cstddef>
#include <functional>
#include <unordered_map>
#include <span>
#include <vector>

#include "jit/common.h"
#include "shared/module.h"

namespace slang::jit
{

std::unordered_map<std::int64_t, std::int32_t> analyze_basic_block_stack_depths(
  std::span<const std::byte> bytecode,
  const std::vector<jit_call_target*>& call_targets,
  const std::vector<jit_call_target*>& import_call_targets);

}    // namespace slang::jit