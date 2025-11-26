/**
 * slang - a simple scripting language.
 *
 * Attributes.
 *
 * \author Felix Lubbe
 * \copyright Copyright (c) 2025
 * \license Distributed under the MIT software license (see accompanying LICENSE.txt).
 */

#include <algorithm>
#include <format>
#include <unordered_map>
#include <utility>

#include "compiler/attribute.h"

namespace slang::attribs
{

/** Kind-string map. */
static const std::unordered_map<std::string, attribute_kind> attribute_string_map =
  {
    {"allow_cast", attribute_kind::allow_cast},
    {"builtin", attribute_kind::builtin},
    {"disable", attribute_kind::disable},
    {"native", attribute_kind::native}};

std::optional<attribute_kind> get_attribute_kind(const std::string& name)
{
    auto it = attribute_string_map.find(name);
    if(it != attribute_string_map.end())
    {
        return it->second;
    }

    return std::nullopt;
}

std::string to_string(attribute_kind kind)
{
    auto it = std::ranges::find_if(
      attribute_string_map,
      [&kind](const auto& p) -> bool
      {
          return p.second == kind;
      });

    if(it == attribute_string_map.end())
    {
        throw std::runtime_error(
          std::format(
            "Missing string in value-string conversion for attribute with value '{}'.",
            std::to_underlying(kind)));
    }

    return it->first;
}

}    // namespace slang::attribs