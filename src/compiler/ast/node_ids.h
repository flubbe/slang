/**
 * slang - a simple scripting language.
 *
 * Node identifiers for expressions.
 *
 * \author Felix Lubbe
 * \copyright Copyright (c) 2025
 * \license Distributed under the MIT software license (see accompanying LICENSE.txt).
 */

#pragma once

#include <cstdint>
#include <format>

#include "archives/archive.h"

namespace slang::ast
{

/** Identifiers. */
enum class node_identifier : std::uint8_t
{
    null = 0, /** The null object. */

    expression = 1,
    named_expression = 2,
    literal_expression = 3,
    type_cast_expression = 4,
    namespace_access_expression = 5,
    access_expression = 6,
    import_expression = 7,
    directive_expression = 8,
    variable_reference_expression = 9,
    variable_declaration_expression = 10,
    constant_declaration_expression = 11,
    array_initializer_expression = 12,
    struct_definition_expression = 13,
    struct_anonymous_initializer_expression = 14,
    named_initializer = 15,
    struct_named_initializer_expression = 16,
    binary_expression = 17,
    unary_expression = 18,
    new_expression = 19,
    null_expression = 20,
    postfix_expression = 21,
    block = 22,
    function_expression = 23,
    call_expression = 24,
    macro_invocation = 25,
    return_statement = 26,
    if_statement = 27,
    while_statement = 28,
    break_statement = 29,
    continue_statement = 30,
    macro_branch = 31,
    macro_expression_list = 32,
    macro_expression = 33,

    format_macro_expression = 34,

    last = format_macro_expression
};

/**
 * `node_identifier` serializer.
 *
 * @param ar The archive to use for serialization.
 * @param i The node identifier.
 * @returns The input archive.
 */
inline archive& operator&(archive& ar, node_identifier& i)
{
    auto i_u8 = static_cast<std::uint8_t>(i);
    ar & i_u8;
    if(i_u8 > static_cast<std::uint8_t>(node_identifier::last))
    {
        throw serialization_error(
          std::format(
            "Node identifier out of range ({} > {}).",
            i_u8,
            static_cast<std::uint8_t>(i)));
    }
    return ar;
}

}    // namespace slang::ast
