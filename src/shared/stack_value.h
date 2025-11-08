/**
 * slang - a simple scripting language.
 *
 * stack value types.
 *
 * \author Felix Lubbe
 * \copyright Copyright (c) 2025
 * \license Distributed under the MIT software license (see accompanying LICENSE.txt).
 */

#pragma once

#include <format>

#include "archives/archive.h"

namespace slang
{

/** Stack value base type. */
using stack_value_base = std::uint8_t;

/** Stack value type information. */
enum class stack_value : stack_value_base
{
    cat1, /** Category 1 (1 slot wide, or 32 bit) */
    cat2, /** Category 2 (2 slots wide, or 64 bit) */
    ref,  /** Reference / address. */

    stack_value_count /** Stack value count. Not a stack value. */
};

/** Get a string representation of the stack value. */
inline std::string to_string(stack_value v)
{
    switch(v)
    {
    case stack_value::cat1: return "cat1";
    case stack_value::cat2: return "cat2";
    case stack_value::ref: return "ref";
    case stack_value::stack_value_count:; /* fall-through */
    }

    return "unknown";
}

/**
 * Stack value serializer.
 *
 * @param ar The archive to use for serialization.
 * @param v The stack value to serialize.
 *
 * @throws Throws an `serialization_error` if the (read) stack value is invalid.
 */
inline archive& operator&(archive& ar, stack_value& v)
{
    auto v_base = static_cast<stack_value_base>(v);
    ar & v_base;

    if(v_base >= static_cast<stack_value_base>(stack_value::stack_value_count))
    {
        throw serialization_error(
          std::format(
            "Invalid stack value '{}'.",
            v_base));
    }

    v = static_cast<stack_value>(v_base);

    return ar;
}

}    // namespace slang
