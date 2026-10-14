#pragma once

#include <string_view>

#include "interpreter/types.h"

namespace slang::jit
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

}    // namespace slang::jit
