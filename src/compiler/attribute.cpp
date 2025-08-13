/**
 * slang - a simple scripting language.
 *
 * Attributes.
 *
 * \author Felix Lubbe
 * \copyright Copyright (c) 2025
 * \license Distributed under the MIT software license (see accompanying LICENSE.txt).
 */

#include "compiler/attribute.h"

namespace slang::attribs
{

std::optional<attribute_kind> get_attribute_kind(const std::string& name)
{
    if(name == "allow_cast")
    {
        return attribute_kind::allow_cast;
    }

    if(name == "native")
    {
        return attribute_kind::native;
    }

    return std::nullopt;
}

}    // namespace slang::attribs