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

#include "utils.h"

namespace slang::utils
{

/** Returns the size a type occupies on the execution stack. */
[[nodiscard]]
constexpr std::size_t get_stack_slot_size(
  std::size_t raw_type_size) noexcept
{
    // Minimum stack slot size is 4 bytes (for Cat-1 / primitive types).
    return utils::align(4, raw_type_size);
}

}    // namespace slang::utils
