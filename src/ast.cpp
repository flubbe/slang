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
    return s == "void" || s == "i32" || s == "f32";
}

/*
 * literal_expression.
 */

std::unique_ptr<cg::value> literal_expression::generate_code(cg::context* ctx) const
{
    if(type == literal_type::int_literal)
    {
        ctx->generate_const({"i32"}, *value);
        return std::make_unique<cg::value>("i32");
    }
    else if(type == literal_type::fp_literal)
    {
        ctx->generate_const({"f32"}, *value);
        return std::make_unique<cg::value>("f32");
    }
    else if(type == literal_type::str_literal)
    {
        ctx->generate_const({"str"}, *value);
        return std::make_unique<cg::value>("str");
    }
    else
    {
        throw cg::codegen_error(fmt::format("Unable to generate code for literal of type id '{}'.", static_cast<int>(type)));
    }

    // TODO
    return {};
}

/*
 * signed_expression.
 */

std::unique_ptr<cg::value> signed_expression::generate_code(cg::context* ctx) const
{
    // TODO
    throw std::runtime_error("signed_expression::generate_code not implemented.");
}

/*
 * scope_expression.
 */

std::unique_ptr<cg::value> scope_expression::generate_code(cg::context* ctx) const
{
    // TODO
    throw std::runtime_error("scope_expression::generate_code not implemented.");
}

/*
 * access_expression.
 */

std::unique_ptr<cg::value> access_expression::generate_code(cg::context* ctx) const
{
    // TODO
    throw std::runtime_error("access_expression::generate_code not implemented.");
}

/*
 * import_expression.
 */

std::unique_ptr<cg::value> import_expression::generate_code(cg::context* ctx) const
{
    // TODO
    throw std::runtime_error("import_expression::generate_code not implemented.");
}

/*
 * variable_reference_expression.
 */

std::unique_ptr<cg::value> variable_reference_expression::generate_code(cg::context* ctx) const
{
    auto* s = ctx->get_scope();
    if(s == nullptr)
    {
        throw cg::codegen_error(fmt::format("No scope to search for '{}'.", name));
    }

    const cg::variable* var{nullptr};
    while(s != nullptr)
    {
        if((var = s->get_variable(name)) != nullptr)
        {
            break;
        }
        s = s->get_outer();
    }

    if(s == nullptr)
    {
        throw cg::codegen_error(fmt::format("Cannot find variable '{}' in current scope.", name));
    }

    ctx->generate_load(std::make_unique<cg::variable_argument>(*var));
    return std::make_unique<cg::value>(var->get_value());
}

/*
 * variable_declaration_expression.
 */

std::unique_ptr<cg::value> variable_declaration_expression::generate_code(cg::context* ctx) const
{
    cg::scope* s = ctx->get_scope();
    if(s == nullptr)
    {
        throw cg::codegen_error("No scope available for adding locals.");
    }
    s->add_local(std::make_unique<cg::variable>(name, type));

    if(expr)
    {
        auto v = expr->generate_code(ctx);
        if(is_builtin_type(type))
        {
            ctx->generate_store(std::make_unique<cg::variable_argument>(cg::variable{name, type}));
        }
        else
        {
            ctx->generate_store(std::make_unique<cg::variable_argument>(cg::variable{name, "composite", type}));
        }
    }

    return {};
}

/*
 * struct_definition_expression.
 */

std::unique_ptr<cg::value> struct_definition_expression::generate_code(cg::context* ctx) const
{
    // TODO
    throw std::runtime_error("struct_definition_expression::generate_code not implemented.");
}

/*
 * struct_anonymous_initializer_expression.
 */

std::unique_ptr<cg::value> struct_anonymous_initializer_expression::generate_code(cg::context* ctx) const
{
    // TODO
    throw std::runtime_error("struct_anonymous_initializer_expression::generate_code not implemented.");
}

/*
 * struct_named_initializer_expression.
 */

std::unique_ptr<cg::value> struct_named_initializer_expression::generate_code(cg::context* ctx) const
{
    // TODO
    throw std::runtime_error("struct_named_initializer_expression::generate_code not implemented.");
}

/*
 * binary_expression.
 */

std::unique_ptr<cg::value> binary_expression::generate_code(cg::context* ctx) const
{
    // TODO
    throw std::runtime_error("binary_expression::generate_code not implemented.");
}

/*
 * prototype_ast.
 */

cg::function* prototype_ast::generate_code(cg::context* ctx) const
{
    std::vector<std::unique_ptr<cg::variable>> function_args;
    for(auto& a: args)
    {
        if(is_builtin_type(a.second))
        {
            function_args.emplace_back(std::make_unique<cg::variable>(a.first, a.second));
        }
        else
        {
            function_args.emplace_back(std::make_unique<cg::variable>(a.first, "composite", a.second));
        }
    }

    return ctx->create_function(name, return_type, std::move(function_args));
}

/*
 * block.
 */

std::unique_ptr<cg::value> block::generate_code(cg::context* ctx) const
{
    std::unique_ptr<cg::value> v;
    for(auto& expr: this->exprs)
    {
        v = expr->generate_code(ctx);
    }
    return v;
}

/*
 * function_expression.
 */

std::unique_ptr<slang::codegen::value> function_expression::generate_code(cg::context* ctx) const
{
    cg::function* fn = prototype->generate_code(ctx);
    cg::basic_block* bb = fn->create_basic_block("entry");

    cg::scope_guard sg{ctx, fn->get_scope()};

    ctx->set_insertion_point(bb);
    auto v = body->generate_code(ctx);

    // generate return instruction if required.
    if(!ctx->ends_with_return())
    {
        ctx->generate_ret();
        return {};
    }

    return v;
}

/*
 * call_expression.
 */

std::unique_ptr<slang::codegen::value> call_expression::generate_code(cg::context* ctx) const
{
    // TODO
    throw std::runtime_error("call_expression::generate_code not implemented.");
}

/*
 * return_statement.
 */

std::unique_ptr<slang::codegen::value> return_statement::generate_code(cg::context* ctx) const
{
    auto v = expr->generate_code(ctx);
    if(!v)
    {
        throw cg::codegen_error("Expression did not yield a type.");
    }
    ctx->generate_ret(*v);
    return {};
}

/*
 * if_statement.
 */

std::unique_ptr<slang::codegen::value> if_statement::generate_code(cg::context* ctx) const
{
    // TODO
    throw std::runtime_error("if_statement::generate_code not implemented.");
}

/*
 * while_statement.
 */

std::unique_ptr<slang::codegen::value> while_statement::generate_code(cg::context* ctx) const
{
    // TODO
    throw std::runtime_error("while_statement::generate_code not implemented.");
}

/*
 * break_statement.
 */

std::unique_ptr<slang::codegen::value> break_statement::generate_code(cg::context* ctx) const
{
    // TODO
    throw std::runtime_error("break_statement::generate_code not implemented.");
}

/*
 * continue_statement.
 */

std::unique_ptr<slang::codegen::value> continue_statement::generate_code(cg::context* ctx) const
{
    // TODO
    throw std::runtime_error("continue_statement::generate_code not implemented.");
}

}    // namespace slang::ast