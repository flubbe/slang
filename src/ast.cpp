/**
 * slang - a simple scripting language.
 *
 * abstract syntax tree.
 *
 * \author Felix Lubbe
 * \copyright Copyright (c) 2024
 * \license Distributed under the MIT software license (see accompanying LICENSE.txt).
 */

#include "ast.h"
#include "codegen.h"

/*
 * Code generation from AST.
 */

namespace cg = slang::codegen;

namespace slang::ast
{

/**
 * Check whether a string represents a built-in type, that is,
 * void, i32, f32 or str.
 */
static bool is_builtin_type(const std::string& s)
{
    return s == "void" || s == "i32" || s == "f32" || s == "void";
}

/*
 * literal_expression.
 */

void literal_expression::generate_code(cg::context* ctx) const
{
    // TODO
    throw std::runtime_error("literal_expression::generate_code not implemented.");
}

/*
 * signed_expression.
 */

void signed_expression::generate_code(cg::context* ctx) const
{
    // TODO
    throw std::runtime_error("signed_expression::generate_code not implemented.");
}

/*
 * scope_expression.
 */

void scope_expression::generate_code(cg::context* ctx) const
{
    // TODO
    throw std::runtime_error("scope_expression::generate_code not implemented.");
}

/*
 * access_expression.
 */

void access_expression::generate_code(cg::context* ctx) const
{
    // TODO
    throw std::runtime_error("access_expression::generate_code not implemented.");
}

/*
 * import_expression.
 */

void import_expression::generate_code(cg::context* ctx) const
{
    // TODO
    throw std::runtime_error("import_expression::generate_code not implemented.");
}

/*
 * variable_reference_expression.
 */

void variable_reference_expression::generate_code(cg::context* ctx) const
{
    // TODO
    throw std::runtime_error("variable_reference_expression::generate_code not implemented.");
}

/*
 * variable_declaration_expression.
 */

void variable_declaration_expression::generate_code(cg::context* ctx) const
{
    // TODO
    throw std::runtime_error("variable_declaration_expression::generate_code not implemented.");
}

/*
 * struct_definition_expression.
 */

void struct_definition_expression::generate_code(cg::context* ctx) const
{
    // TODO
    throw std::runtime_error("struct_definition_expression::generate_code not implemented.");
}

/*
 * struct_anonymous_initializer_expression.
 */

void struct_anonymous_initializer_expression::generate_code(cg::context* ctx) const
{
    // TODO
    throw std::runtime_error("struct_anonymous_initializer_expression::generate_code not implemented.");
}

/*
 * struct_named_initializer_expression.
 */

void struct_named_initializer_expression::generate_code(cg::context* ctx) const
{
    // TODO
    throw std::runtime_error("struct_named_initializer_expression::generate_code not implemented.");
}

/*
 * binary_expression.
 */

void binary_expression::generate_code(cg::context* ctx) const
{
    // TODO
    throw std::runtime_error("binary_expression::generate_code not implemented.");
}

/*
 * prototype_expression.
 */

void prototype_expression::generate_code(cg::context* ctx) const
{
    std::vector<cg::variable> function_args;
    for(auto& a: args)
    {
        if(is_builtin_type(a.second))
        {
            function_args.emplace_back(cg::variable(a.first, cg::value_type_from_string(a.second)));
        }
        else
        {
            function_args.emplace_back(cg::variable(a.first, cg::value_type::composite, a.second));
        }
    }

    auto* f = ctx->create_function(name, return_type, function_args);

    // TODO
    throw std::runtime_error("prototype_expression::generate_code not implemented.");
}

/*
 * block.
 */

void block::generate_code(cg::context* ctx) const
{
    for(auto& expr: this->exprs)
    {
        expr->generate_code(ctx);
    }
}

/*
 * function_expression.
 */

void function_expression::generate_code(cg::context* ctx) const
{
    prototype->generate_code(ctx);

    // TODO
    throw std::runtime_error("function_expression::generate_code not implemented.");
}

/*
 * call_expression.
 */

void call_expression::generate_code(cg::context* ctx) const
{
    // TODO
    throw std::runtime_error("call_expression::generate_code not implemented.");
}

/*
 * return_statement.
 */

void return_statement::generate_code(cg::context* ctx) const
{
    // TODO
    throw std::runtime_error("return_statement::generate_code not implemented.");
}

/*
 * if_statement.
 */

void if_statement::generate_code(cg::context* ctx) const
{
    // TODO
    throw std::runtime_error("if_statement::generate_code not implemented.");
}

/*
 * while_statement.
 */

void while_statement::generate_code(cg::context* ctx) const
{
    // TODO
    throw std::runtime_error("while_statement::generate_code not implemented.");
}

/*
 * break_statement.
 */

void break_statement::generate_code(cg::context* ctx) const
{
    // TODO
    throw std::runtime_error("break_statement::generate_code not implemented.");
}

/*
 * continue_statement.
 */

void continue_statement::generate_code(cg::context* ctx) const
{
    // TODO
    throw std::runtime_error("continue_statement::generate_code not implemented.");
}

}    // namespace slang::ast