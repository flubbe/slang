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
 * void, i8, i16, i32, i64, f32, f64 or str.
 */
inline bool is_builtin_type(const std::string& s) noexcept
{
    return s == "void"
           || s == "i8"
           || s == "i16"
           || s == "i32"
           || s == "i64"
           || s == "f32"
           || s == "f64"
           || s == "str";
}

/** Return whether a base type is a reference type. */
inline bool is_reference_type(const std::string& base_type) noexcept
{
    return base_type != "void"
           && base_type != "i8"
           && base_type != "i16"
           && base_type != "i32"
           && base_type != "i64"
           && base_type != "f32"
           && base_type != "f64";
}

}    // namespace slang::typing
