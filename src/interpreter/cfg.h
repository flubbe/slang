/**
 * slang - a simple scripting language.
 *
 * Control Flow Graph analysis for the interpreter.
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

#include "shared/module.h"

namespace slang::interpreter
{

/**
 * Analyse the bytecode to calculate the stack depth at each label.
 * Validates stack-depth consistency across different control-flow paths.
 *
 * @param loader Loader for the current module.
 * @param bytecode The bytecode.
 * @param exports Module exports.
 * @param improts Module imports.
 * @param resolver Resolve a module name into a `module_loader`.
 * @returns Returns a map from label IDs to stack depths.
 */
std::unordered_map<std::int64_t, std::int32_t> analyze_bytecode_stack_depths(
  const module_loader& loader,
  std::span<const std::byte> bytecode,
  const std::__1::vector<slang::module_::exported_symbol>& exports,
  const std::__1::vector<slang::module_::imported_symbol>& imports,
  const std::function<const module_loader&(const std::string&)>& resolver);

}    // namespace slang::interpreter
