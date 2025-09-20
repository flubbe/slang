/**
 * slang - a simple scripting language.
 *
 * Attributes.
 *
 * \author Felix Lubbe
 * \copyright Copyright (c) 2025
 * \license Distributed under the MIT software license (see accompanying LICENSE.txt).
 */

#pragma once

#include <optional>
#include <stdexcept>
#include <string>
#include <variant>

#include "location.h"

namespace slang::attribs
{

using slang::source_location;

/** Attribute error. */
class attribute_error : public std::runtime_error
{
    using std::runtime_error::runtime_error;
};

/** Attribute kind. */
enum class attribute_kind
{
    allow_cast, /** Allow casting from reference types. */
    builtin,    /** Compiler built-in. */
    disable,    /** Disable a flag, e.g. `disable(const_eval)`. */
    native      /** Native function. */
};

/** Attributes. */
struct attribute_info
{
    /** Payload type. */
    using payload_type = std::variant<
      std::monostate,
      std::vector<
        std::pair<
          std::string,
          std::string>>>;

    /** Attribute kind. */
    attribute_kind kind;

    /** Source location. */
    source_location loc;

    /** Attribute payload. */
    payload_type payload;
};

/**
 * Return the attribute kind for a given name.
 *
 * @param name The attribute name.
 * @returns Returns the attribute kind, or `std::nullopt`.
 */
std::optional<attribute_kind> get_attribute_kind(const std::string& name);

/**
 * Convert the attribute kind to a readable string.
 *
 * @param kind Attribute kind.
 * @returns Returns a readable string.
 */
std::string to_string(attribute_kind kind);

}    // namespace slang::attribs
