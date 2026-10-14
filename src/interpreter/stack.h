/**
 * slang - a simple scripting language.
 *
 * stack helpers.
 *
 * \author Felix Lubbe
 * \copyright Copyright (c) 2025
 * \license Distributed under the MIT software license (see accompanying LICENSE.txt).
 */

#pragma once

#include "type_utils.h"
#include "utils.h"

namespace slang::interpreter
{

/** Returns the size a type occupies on the execution stack. */
[[nodiscard]]
constexpr std::size_t get_stack_slot_size(
  std::size_t raw_type_size) noexcept
{
    // Minimum stack slot size is 4 bytes (for cat1 / primitive types).
    return utils::align(4, raw_type_size);
}

/**
 * Calculate the stack size delta from a function's signature.
 *
 * @param s The signature.
 * @returns The stack size delta.
 */
[[nodiscard]]
inline std::int32_t get_stack_delta(
  const module_::function_signature& s)
{
    auto return_type_size = interpreter::get_type_or_reference_size(s.return_type);
    std::int32_t arg_size = 0;
    for(const auto& it: s.arg_types)
    {
        arg_size += utils::numeric_cast<std::int32_t>(
          interpreter::get_type_or_reference_size(it));
        // NOTE stack contents are not aligned.
    }
    return utils::numeric_cast<std::int32_t>(return_type_size) - arg_size;
}

}    // namespace slang::interpreter
