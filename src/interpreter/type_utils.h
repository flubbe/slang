/**
 * slang - a simple scripting language.
 *
 * type helpers.
 *
 * \author Felix Lubbe
 * \copyright Copyright (c) 2025
 * \license Distributed under the MIT software license (see accompanying LICENSE.txt).
 */

#pragma once

#include <string_view>

#include "shared/type_utils.h"
#include "types.h"

namespace slang::interpreter
{

/** Get the byte size and alignment for a built-in type name. */
inline std::optional<type_properties> get_built_in_type_properties(
  std::string_view type_name)
{
    if(type_name == "void")
    {
        return type_properties{
          .size = 0,
          .alignment = 0,
          .flags = 0,
          .layout_id = 0};
    }

    if(type_name == "i8"
       || type_name == "i16"
       || type_name == "i32")
    {
        return type_properties{
          .size = sizeof(std::int32_t),
          .alignment = alignof(std::int32_t),
          .flags = 0,
          .layout_id = 0};
    }

    if(type_name == "i64")
    {
        return type_properties{
          .size = sizeof(std::int64_t),
          .alignment = alignof(std::int64_t),
          .flags = 0,
          .layout_id = 0};
    }

    if(type_name == "f32")
    {
        return type_properties{
          .size = sizeof(float),
          .alignment = alignof(float),
          .flags = 0,
          .layout_id = 0};
    }

    if(type_name == "f64")
    {
        return type_properties{
          .size = sizeof(double),
          .alignment = alignof(double),
          .flags = 0,
          .layout_id = 0};
    }

    if(type_name == "str")
    {
        {
            return type_properties{
              .size = sizeof(std::string*),
              .alignment = alignof(std::string*),
              .flags = 0,
              .layout_id = 0};
        }
    }

    // not a built-in type.
    return std::nullopt;
}

/** Get the byte size and alignment for the native representation of a type. */
inline std::optional<type_properties> get_native_type_properties(
  std::string_view type_name)
{
    if(type_name == "void")
    {
        return type_properties{
          .size = 0,
          .alignment = 0,
          .flags = 0,
          .layout_id = 0};
    }

    if(type_name == "i8")
    {
        return type_properties{
          .size = sizeof(std::int8_t),
          .alignment = alignof(std::int8_t),
          .flags = 0,
          .layout_id = 0};
    }

    if(type_name == "i16")
    {
        return type_properties{
          .size = sizeof(std::int16_t),
          .alignment = alignof(std::int16_t),
          .flags = 0,
          .layout_id = 0};
    }

    if(type_name == "i32")
    {
        return type_properties{
          .size = sizeof(std::int32_t),
          .alignment = alignof(std::int32_t),
          .flags = 0,
          .layout_id = 0};
    }

    if(type_name == "i64")
    {
        return type_properties{
          .size = sizeof(std::int64_t),
          .alignment = alignof(std::int64_t),
          .flags = 0,
          .layout_id = 0};
    }

    if(type_name == "f32")
    {
        return type_properties{
          .size = sizeof(float),
          .alignment = alignof(float),
          .flags = 0,
          .layout_id = 0};
    }

    if(type_name == "f64")
    {
        return type_properties{
          .size = sizeof(double),
          .alignment = alignof(double),
          .flags = 0,
          .layout_id = 0};
    }

    if(type_name == "str")
    {
        {
            return type_properties{
              .size = sizeof(std::string*),
              .alignment = alignof(std::string*),
              .flags = 0,
              .layout_id = 0};
        }
    }

    // not a built-in type.
    return std::nullopt;
}

/** Get the type size (for built-in types) or the size of a type reference (for custom types). */
inline std::size_t get_type_or_reference_size(
  const module_::variable_type& v)
{
    if(v.base_type().empty())
    {
        throw interpreter_error{
          "Unable to determine type size for empty type."};
    }

    if(v.is_array())
    {
        return sizeof(void*);
    }

    auto built_in = get_built_in_type_properties(v.base_type());
    if(built_in.has_value())
    {
        return built_in.value().size;
    }

    // FIXME Assume all other types are references.
    return sizeof(void*);
}

/**
 * Check if a type is garbage collected.
 *
 * @param t The type string.
 * @returns Return whether a type is garbage collected.
 */
inline bool is_garbage_collected(
  const module_::variable_type& t) noexcept
{
    namespace ty = slang::typing;

    if(t.base_type() == "void")
    {
        return false;
    }

    // check for built-in non-gc types.
    return t.is_array()
           || ty::is_reference_type(t.base_type());
}

}    // namespace slang::interpreter
