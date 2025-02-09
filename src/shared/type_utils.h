/**
 * slang - a simple scripting language.
 *
 * shared type functionality.
 *
 * \author Felix Lubbe
 * \copyright Copyright (c) 2025
 * \license Distributed under the MIT software license (see accompanying LICENSE.txt).
 */

#include <string>

#pragma once

namespace slang::typing
{

/**
 * Return whether a string represents a built-in type, that is,
 * void, i32, f32 or str.
 */
inline bool is_builtin_type(const std::string& s)
{
    return s == "void" || s == "i32" || s == "f32" || s == "str";
}

/** Return whether a base type is a reference type. */
inline bool is_reference_type(const std::string& base_type)
{
    return base_type != "void" && base_type != "i32" && base_type != "f32";
}

}    // namespace slang::typing
