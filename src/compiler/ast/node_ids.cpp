/**
 * slang - a simple scripting language.
 *
 * Node identifiers.
 *
 * \author Felix Lubbe
 * \copyright Copyright (c) 2025
 * \license Distributed under the MIT software license (see accompanying LICENSE.txt).
 */

#include "node_ids.h"

namespace slang::ast
{

std::string to_string(node_identifier node_id)
{
    switch(node_id)
    {
    case node_identifier::null: return "null";
    case node_identifier::expression: return "expression";
    case node_identifier::named_expression: return "named_expression";
    case node_identifier::literal_expression: return "literal_expression";
    case node_identifier::type_cast_expression: return "type_cast_expression";
    case node_identifier::namespace_access_expression: return "namespace_access_expression";
    case node_identifier::access_expression: return "access_expression";
    case node_identifier::import_statement: return "import_statement";
    case node_identifier::directive_expression: return "directive_expression";
    case node_identifier::variable_reference_expression: return "variable_reference_expression";
    case node_identifier::variable_declaration_expression: return "variable_declaration_expression";
    case node_identifier::constant_declaration_expression: return "constant_declaration_expression";
    case node_identifier::array_initializer_expression: return "array_initializer_expression";
    case node_identifier::struct_definition_expression: return "struct_definition_expression";
    case node_identifier::struct_anonymous_initializer_expression: return "struct_anonymous_initializer_expression";
    case node_identifier::named_initializer: return "named_initializer";
    case node_identifier::struct_named_initializer_expression: return "struct_named_initializer_expression";
    case node_identifier::assignment_expression: return "assignment_expression";
    case node_identifier::binary_expression: return "binary_expression";
    case node_identifier::unary_expression: return "unary_expression";
    case node_identifier::new_expression: return "new_expression";
    case node_identifier::null_expression: return "null_expression";
    case node_identifier::postfix_expression: return "postfix_expression";
    case node_identifier::block: return "block";
    case node_identifier::function_expression: return "function_expression";
    case node_identifier::call_expression: return "call_expression";
    case node_identifier::macro_invocation: return "macro_invocation";
    case node_identifier::return_statement: return "return_statement";
    case node_identifier::if_statement: return "if_statement";
    case node_identifier::while_statement: return "while_statement";
    case node_identifier::break_statement: return "break_statement";
    case node_identifier::continue_statement: return "continue_statement";
    case node_identifier::macro_branch: return "macro_branch";
    case node_identifier::macro_expression_list: return "macro_expression_list";
    case node_identifier::macro_expression: return "macro_expression";
    case node_identifier::format_macro_expression: return "format_macro_expression";
    default:; /* fall-through */
    }

    return "unknown";
}

}    // namespace slang::ast