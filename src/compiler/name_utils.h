/**
 * slang - a simple scripting language.
 *
 * name utilities.
 *
 * \author Felix Lubbe
 * \copyright Copyright (c) 2025
 * \license Distributed under the MIT software license (see accompanying LICENSE.txt).
 */

#pragma once

#include <format>
#include <stdexcept>
#include <string>
#include <string_view>

#include "compiler/errors.h"

namespace slang::name
{

/**
 * A name invariant violation.
 *
 * Raised on malformed names (an empty unqualified variant, or an unqualified
 * name where a qualified one is required). These are internal compiler
 * conditions, not user errors.
 */
class name_error : public internal_error
{
    using internal_error::internal_error;
};

/**
 * Return a qualified name.
 *
 * @param path Name path.
 * @param name The name.
 * @returns Returns a qualified name.
 */
inline std::string qualified_name(
  std::string_view path,
  std::string_view name)
{
    return std::format(
      "{}::{}",
      path,
      name);
}

/**
 * Return an unqualified name. For example, `test::L -> L`, and `T -> T`.
 *
 * @param name The qualified or unqualified name.
 * @returns An unqualified name.
 * @throws Throws a `name_error` if the unqualified name is empty.
 */
inline std::string unqualified_name(std::string_view qualified)
{
    auto pos = qualified.rfind("::");
    auto unqualified = (pos == std::string::npos)
                         ? std::string(qualified)
                         : std::string(qualified.substr(pos + 2));
    if(unqualified.empty())
    {
        throw name_error(
          std::format(
            "Invalid name '{}' has no unqualified variant.",
            qualified));
    }
    return unqualified;
}

/**
 * Get the module path of a qualified name.
 *
 * @param qualified The qualified name.
 * @returns A module path.
 * @throws Throws a `name_error` if the name was not qualified.
 */
inline std::string_view module_path(std::string_view qualified)
{
    auto pos = qualified.rfind("::");
    if(pos == qualified.npos)
    {
        throw name_error(
          std::format(
            "Name '{}' was not qualified.",
            qualified));
    }
    return qualified.substr(0, pos);
}

}    // namespace slang::name
