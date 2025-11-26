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
    null, /** The null object. */

    expression,
    named_expression,
    literal_expression,
    type_cast_expression,
    namespace_access_expression,
    access_expression,
    directive_expression,
    variable_reference_expression,
    variable_declaration_expression,
    constant_declaration_expression,
    array_initializer_expression,
    struct_definition_expression,
    struct_anonymous_initializer_expression,
    named_initializer,
    struct_named_initializer_expression,
    assignment_expression,
    binary_expression,
    unary_expression,
    new_expression,
    null_expression,
    postfix_expression,
    block,
    function_expression,
    call_expression,
    macro_invocation,
    expression_statement,
    import_statement,
    return_statement,
    if_statement,
    while_statement,
    break_statement,
    continue_statement,
    macro_branch,
    macro_expression_list,
    macro_expression,

    format_macro_expression,

    last = format_macro_expression
};

/**
 * Convert a `node_identifier` to a readable string.
 *
 * @param node_id The node identifier.
 * @param Returns a readable string for the node identifier.
 */
std::string to_string(node_identifier node_id);

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
