/**
 * slang - a simple scripting language.
 *
 * type classes.
 *
 * \author Felix Lubbe
 * \copyright Copyright (c) 2025
 * \license Distributed under the MIT software license (see accompanying LICENSE.txt).
 */

#pragma once

#include <format>
#include <utility>

#include "archives/archive.h"

namespace slang
{

/** Type layout error. */

/** Type class base type. */
using type_class_base = std::uint8_t;

/** Value categories. */
enum class type_class : type_class_base
{
    cat1, /** Category 1 (1 slot wide, or 32 bit) */
    cat2, /** Category 2 (2 slots wide, or 64 bit) */
    ref,  /** Reference / address. */

    count /** Type class count. Not a stack value. */
};

/** Type layout. */
struct type_layout
{
    /** Type size, in bytes. */
    uint8_t size;

    /** Type alignment, in bytes */
    uint8_t align;
};

/** Target type layout. */
struct target_type_layout
{
    /** Type layout array. */
    static constexpr std::array<type_layout, std::to_underlying(type_class::count)> layouts = {
      type_layout{4, 4},                                                     // cat1
      type_layout{8, 8},                                                     // cat2
      type_layout{std::alignment_of_v<void*>, std::alignment_of_v<void*>}    // ref,
    };

    /**
     * Get the type layout for a value class.
     *
     * @param cls The value class.
     * @returns Returns the layout for the type class.
     * @throws Throws an `std::invalid_argument` if an invalid type class was specified.
     */
    static type_layout for_class(type_class cls)
    {
        const auto i = std::to_underlying(cls);
        if(i >= layouts.size()) [[unlikely]]
        {
            throw std::invalid_argument("Invalid type_class");
        }

        return layouts[i];
    }
};

/** Get a string representation of the stack value. */
inline std::string to_string(type_class v)
{
    switch(v)
    {
    case type_class::cat1: return "cat1";
    case type_class::cat2: return "cat2";
    case type_class::ref: return "ref";
    case type_class::count:; /* fall-through */
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
inline archive& operator&(archive& ar, type_class& v)
{
    auto v_base = std::to_underlying(v);
    ar & v_base;

    if(v_base >= std::to_underlying(type_class::count))
    {
        throw serialization_error(
          std::format(
            "Invalid stack value '{}'.",
            v_base));
    }

    v = static_cast<type_class>(v_base);

    return ar;
}

}    // namespace slang
