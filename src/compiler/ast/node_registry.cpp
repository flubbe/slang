/**
 * slang - a simple scripting language.
 *
 * Node registry, associating node identifiers with constructors
 * of subclasses of `expression`.
 *
 * \author Felix Lubbe
 * \copyright Copyright (c) 2025
 * \license Distributed under the MIT software license (see accompanying LICENSE.txt).
 */

#include "node_registry.h"
#include "ast.h"
#include "builtins.h"

namespace slang::ast
{

std::unique_ptr<expression> construct(node_identifier id)
{
    switch(id)
    {
    case node_identifier::null: return {};
    case node_identifier::expression: return std::make_unique<expression>();
    case node_identifier::named_expression: return std::make_unique<named_expression>();
    case node_identifier::literal_expression: return std::make_unique<literal_expression>();
    case node_identifier::type_cast_expression: return std::make_unique<type_cast_expression>();
    case node_identifier::namespace_access_expression: return std::make_unique<namespace_access_expression>();
    case node_identifier::access_expression: return std::make_unique<access_expression>();
    case node_identifier::directive_expression: return std::make_unique<directive_expression>();
    case node_identifier::variable_reference_expression: return std::make_unique<variable_reference_expression>();
    case node_identifier::variable_declaration_expression: return std::make_unique<variable_declaration_expression>();
    case node_identifier::constant_declaration_expression: return std::make_unique<constant_declaration_expression>();
    case node_identifier::array_initializer_expression: return std::make_unique<array_initializer_expression>();
    case node_identifier::struct_definition_expression: return std::make_unique<struct_definition_expression>();
    case node_identifier::struct_anonymous_initializer_expression: return std::make_unique<struct_anonymous_initializer_expression>();
    case node_identifier::named_initializer: return std::make_unique<named_initializer>();
    case node_identifier::struct_named_initializer_expression: return std::make_unique<struct_named_initializer_expression>();
    case node_identifier::binary_expression: return std::make_unique<binary_expression>();
    case node_identifier::unary_expression: return std::make_unique<unary_expression>();
    case node_identifier::new_expression: return std::make_unique<new_expression>();
    case node_identifier::null_expression: return std::make_unique<null_expression>();
    case node_identifier::postfix_expression: return std::make_unique<postfix_expression>();
    case node_identifier::block: return std::make_unique<block>();
    case node_identifier::function_expression: return std::make_unique<function_expression>();
    case node_identifier::call_expression: return std::make_unique<call_expression>();
    case node_identifier::macro_invocation: return std::make_unique<macro_invocation>();
    case node_identifier::expression_statement: return std::make_unique<expression_statement>();
    case node_identifier::import_statement: return std::make_unique<import_statement>();
    case node_identifier::return_statement: return std::make_unique<return_statement>();
    case node_identifier::if_statement: return std::make_unique<if_statement>();
    case node_identifier::while_statement: return std::make_unique<while_statement>();
    case node_identifier::break_statement: return std::make_unique<break_statement>();
    case node_identifier::continue_statement: return std::make_unique<continue_statement>();
    case node_identifier::macro_branch: return std::make_unique<macro_branch>();
    case node_identifier::macro_expression_list: return std::make_unique<macro_expression_list>();
    case node_identifier::macro_expression: return std::make_unique<macro_expression>();
    case node_identifier::format_macro_expression: return std::make_unique<format_macro_expression>();
    }

    throw std::runtime_error(
      std::format(
        "Cannot construct AST node from unknown id {}.",
        std::to_underlying(id)));
}

}    // namespace slang::ast
