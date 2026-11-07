/**
 * slang - a simple scripting language.
 *
 * source location definition.
 *
 * \author Felix Lubbe
 * \copyright Copyright (c) 2025
 * \license Distributed under the MIT software license (see accompanying LICENSE.txt).
 */

#include "location.h"

#include <format>

namespace slang
{

archive& operator&(archive& ar, source_location& loc)
{
    vle_int i{utils::numeric_cast<std::int64_t>(loc.col)};
    ar & i;
    loc.col = i.i;

    i.i = utils::numeric_cast<std::int64_t>(loc.line);
    ar & i;
    loc.line = i.i;

    return ar;
}

std::string to_string(const source_location& loc)
{
    return std::format("{}:{}", loc.line, loc.col);
}

}    // namespace slang
