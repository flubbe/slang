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

/**
 * Analyse the bytecode to calculate the stack depth at each label.
 * Validates stack-depth consistency across different control-flow paths.
 *
 * @param bytecode The bytecode.
 * @param call_targets Module-local call targets.
 * @param import_call_targets Call targets imported by the module.
 * @returns Returns a map from label IDs to stack depths.
 */
std::unordered_map<std::int64_t, std::int32_t> analyze_bytecode_stack_depths(
  std::span<const std::byte> bytecode,
  const std::vector<std::unique_ptr<jit_call_target>>& call_targets,
  const std::vector<jit_call_target*>& import_call_targets);

}    // namespace slang::jit
