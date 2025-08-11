/**
 * slang - a simple scripting language.
 *
 * type representation in the typing system.
 *
 * \author Felix Lubbe
 * \copyright Copyright (c) 2025
 * \license Distributed under the MIT software license (see accompanying LICENSE.txt).
 */

#pragma once

#include <cstdint>
#include <memory>
#include <vector>

#include "token.h"

namespace slang::typing
{

/** Type identifier. */
using type_id = std::uint64_t;

}    // namespace slang::typing
