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
#include "utils.h"

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

std::unique_ptr<cg::value> literal_expression::generate_code(cg::context* ctx, memory_context mc) const
{
    if(mc == memory_context::store)
    {
        if(type == literal_type::int_literal)
        {
            throw cg::codegen_error(fmt::format("{}: Cannot store into int_literal '{}'.", slang::to_string(loc), std::get<int>(*value)));
        }
        else if(type == literal_type::fp_literal)
        {
            throw cg::codegen_error(fmt::format("{}: Cannot store into fp_literal '{}'.", slang::to_string(loc), std::get<float>(*value)));
        }
        else if(type == literal_type::str_literal)
        {
            throw cg::codegen_error(fmt::format("{}: Cannot store into str_literal '{}'.", slang::to_string(loc), std::get<std::string>(*value)));
        }
        else
        {
            throw cg::codegen_error(fmt::format("{}: Cannot store into unknown literal of type id '{}'.", slang::to_string(loc), static_cast<int>(type)));
        }
    }

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
        throw cg::codegen_error(fmt::format("{}: Unable to generate code for literal of type id '{}'.", slang::to_string(loc), static_cast<int>(type)));
    }
}

std::string literal_expression::to_string() const
{
    if(type == literal_type::fp_literal)
    {
        if(value)
        {
            return fmt::format("FloatLiteral(value={})", std::get<float>(*value));
        }
        else
        {
            return "FloatLiteral(<none>)";
        }
    }
    else if(type == literal_type::int_literal)
    {
        if(value)
        {
            return fmt::format("IntLiteral(value={})", std::get<int>(*value));
        }
        else
        {
            return "IntLiteral(<none>)";
        }
    }
    else if(type == literal_type::str_literal)
    {
        if(value)
        {
            return fmt::format("StrLiteral(value=\"{}\")", std::get<std::string>(*value));
        }
        else
        {
            return "StrLiteral(<none>)";
        }
    }
    else
    {
        return "UnknownLiteral";
    }
}

/*
 * signed_expression.
 */

std::unique_ptr<cg::value> signed_expression::generate_code(cg::context* ctx, memory_context mc) const
{
    // TODO
    throw std::runtime_error(fmt::format("{}: signed_expression::generate_code not implemented.", slang::to_string(loc)));
}

std::string signed_expression::to_string() const
{
    return fmt::format("Signed(sign={}, expr={})", std::get<1>(sign), expr ? expr->to_string() : std::string("<none>"));
}

/*
 * scope_expression.
 */

std::unique_ptr<cg::value> scope_expression::generate_code(cg::context* ctx, memory_context mc) const
{
    // TODO
    throw std::runtime_error(fmt::format("{}: scope_expression::generate_code not implemented.", slang::to_string(loc)));
}

std::string scope_expression::to_string() const
{
    return fmt::format("Scope(name={}, expr={})", std::get<1>(name), expr ? expr->to_string() : std::string("<none>"));
}

/*
 * access_expression.
 */

std::unique_ptr<cg::value> access_expression::generate_code(cg::context* ctx, memory_context mc) const
{
    // TODO
    throw std::runtime_error(fmt::format("{}: access_expression::generate_code not implemented.", slang::to_string(loc)));
}

std::string access_expression::to_string() const
{
    return fmt::format("Access(name={}, expr={})", std::get<1>(name), expr ? expr->to_string() : std::string("<none>"));
}

/*
 * import_expression.
 */

std::unique_ptr<cg::value> import_expression::generate_code(cg::context* ctx, memory_context mc) const
{
    // TODO
    throw std::runtime_error(fmt::format("{}: import_expression::generate_code not implemented.", slang::to_string(loc)));
}

std::string import_expression::to_string() const
{
    auto transform = [](const std::pair<token_location, std::string>& p) -> std::string
    { return p.second; };

    return fmt::format("Import(path={})", slang::utils::join(path, {transform}, "."));
}

/*
 * variable_reference_expression.
 */

std::unique_ptr<cg::value> variable_reference_expression::generate_code(cg::context* ctx, memory_context mc) const
{
    auto* s = ctx->get_scope();
    if(s == nullptr)
    {
        throw cg::codegen_error(fmt::format("{}: No scope to search for '{}'.", slang::to_string(loc), std::get<1>(name)));
    }

    const cg::variable* var{nullptr};
    while(s != nullptr)
    {
        if((var = s->get_variable(std::get<1>(name))) != nullptr)
        {
            break;
        }
        s = s->get_outer();
    }

    if(s == nullptr)
    {
        throw cg::codegen_error(fmt::format("{}: Cannot find variable '{}' in current scope.", slang::to_string(loc), std::get<1>(name)));
    }

    if(mc == memory_context::none || mc == memory_context::load)
    {
        ctx->generate_load(std::make_unique<cg::variable_argument>(*var));
    }
    else
    {
        ctx->generate_store(std::make_unique<cg::variable_argument>(*var));
    }

    return std::make_unique<cg::value>(var->get_value());
}

std::string variable_reference_expression::to_string() const
{
    return fmt::format("VariableReference(name={})", std::get<1>(name));
}

/*
 * variable_declaration_expression.
 */

std::unique_ptr<cg::value> variable_declaration_expression::generate_code(cg::context* ctx, memory_context mc) const
{
    if(mc != memory_context::none)
    {
        throw cg::codegen_error(fmt::format("{}: Invalid memory context for variable declaration.", slang::to_string(loc)));
    }

    cg::scope* s = ctx->get_scope();
    if(s == nullptr)
    {
        throw cg::codegen_error(fmt::format("{}: No scope available for adding locals.", slang::to_string(loc)));
    }
    s->add_local(std::make_unique<cg::variable>(std::get<1>(name), std::get<1>(type)));

    if(expr)
    {
        auto v = expr->generate_code(ctx);
        if(is_builtin_type(std::get<1>(type)))
        {
            ctx->generate_store(std::make_unique<cg::variable_argument>(cg::variable{std::get<1>(name), std::get<1>(type)}));
        }
        else
        {
            ctx->generate_store(std::make_unique<cg::variable_argument>(cg::variable{std::get<1>(name), "composite", std::get<1>(type)}));
        }
    }

    return {};
}

std::string variable_declaration_expression::to_string() const
{
    return fmt::format("VariableDeclaration(name={}, type={}, expr={})", std::get<1>(name), std::get<1>(type), expr ? expr->to_string() : std::string("<none>"));
}

/*
 * struct_definition_expression.
 */

std::unique_ptr<cg::value> struct_definition_expression::generate_code(cg::context* ctx, memory_context mc) const
{
    // TODO
    throw std::runtime_error(fmt::format("{}: struct_definition_expression::generate_code not implemented.", slang::to_string(loc)));
}

std::string struct_definition_expression::to_string() const
{
    std::string ret = fmt::format("Struct(name={}, members=(", std::get<1>(name));
    for(std::size_t i = 0; i < members.size() - 1; ++i)
    {
        ret += fmt::format("{}, ", members[i]->to_string());
    }
    if(members.size() > 0)
    {
        ret += fmt::format("{}", members.back()->to_string());
    }
    ret += ")";
    return ret;
}

/*
 * struct_anonymous_initializer_expression.
 */

std::unique_ptr<cg::value> struct_anonymous_initializer_expression::generate_code(cg::context* ctx, memory_context mc) const
{
    // TODO
    throw std::runtime_error(fmt::format("{}: struct_anonymous_initializer_expression::generate_code not implemented.", slang::to_string(loc)));
}

std::string struct_anonymous_initializer_expression::to_string() const
{
    std::string ret = fmt::format("StructAnonymousInitializer(name={}, initializers=(", std::get<1>(name));
    for(std::size_t i = 0; i < initializers.size() - 1; ++i)
    {
        ret += fmt::format("{}, ", initializers[i]->to_string());
    }
    if(initializers.size() > 0)
    {
        ret += fmt::format("{}", initializers.back()->to_string());
    }
    ret += ")";
    return ret;
}

/*
 * struct_named_initializer_expression.
 */

std::unique_ptr<cg::value> struct_named_initializer_expression::generate_code(cg::context* ctx, memory_context mc) const
{
    // TODO
    throw std::runtime_error(fmt::format("{}: struct_named_initializer_expression::generate_code not implemented.", slang::to_string(loc)));
}

std::string struct_named_initializer_expression::to_string() const
{
    std::string ret = fmt::format("StructNamedInitializer(name={}, initializers=(", std::get<1>(name));

    if(member_names.size() != initializers.size())
    {
        ret += "<name/initializer mismatch>";
    }
    else
    {
        for(std::size_t i = 0; i < initializers.size() - 1; ++i)
        {
            ret += fmt::format("{}={}, ", member_names[i]->to_string(), initializers[i]->to_string());
        }
        if(initializers.size() > 0)
        {
            ret += fmt::format("{}={}", member_names.back()->to_string(), initializers.back()->to_string());
        }
        ret += ")";
    }
    return ret;
}

/*
 * binary_expression.
 */

std::unique_ptr<cg::value> binary_expression::generate_code(cg::context* ctx, memory_context mc) const
{
    bool is_assignment = (op == "=" || op == "+=" || op == "-=" || op == "*=" || op == "/=" || op == "%=" || op == "&=" || op == "|=" || op == "<<=" || op == ">>=");
    bool is_compound = is_assignment && (op != "=");

    std::string reduced_op = op;
    if(is_compound)
    {
        reduced_op.pop_back();
    }

    if(!is_assignment || is_compound)
    {
        if(mc == memory_context::store)
        {
            throw cg::codegen_error(fmt::format("{}: Invalid memory context for binary operator.", slang::to_string(loc)));
        }

        auto lhs_value = lhs->generate_code(ctx, memory_context::load);
        auto rhs_value = rhs->generate_code(ctx, memory_context::load);

        if(lhs_value->get_type() != rhs_value->get_type())
        {
            throw cg::codegen_error(fmt::format("{}: Types don't match in binary operation. LHS: {}, RHS: {}.", slang::to_string(loc), lhs_value->get_type(), rhs_value->get_type()));
        }

        std::unordered_map<std::string, cg::binary_op> op_map = {
          {"*", cg::binary_op::op_mul},
          {"/", cg::binary_op::op_div},
          {"%", cg::binary_op::op_mod},
          {"+", cg::binary_op::op_add},
          {"-", cg::binary_op::op_sub},
          {"<<", cg::binary_op::op_shl},
          {">>", cg::binary_op::op_shr},
          {"<", cg::binary_op::op_less},
          {"<=", cg::binary_op::op_less_equal},
          {">", cg::binary_op::op_greater},
          {">=", cg::binary_op::op_greater_equal},
          {"==", cg::binary_op::op_equal},
          {"!=", cg::binary_op::op_not_equal},
          {"&", cg::binary_op::op_and},
          {"^", cg::binary_op::op_xor},
          {"|", cg::binary_op::op_or},
          {"&&", cg::binary_op::op_logical_and},
          {"||", cg::binary_op::op_logical_or}};

        auto it = op_map.find(reduced_op);
        if(it != op_map.end())
        {
            ctx->generate_binary_op(it->second, *lhs_value);

            if(is_compound)
            {
                lhs_value = lhs->generate_code(ctx, memory_context::store);

                if(mc == memory_context::load)
                {
                    lhs_value = lhs->generate_code(ctx, memory_context::load);
                }
            }

            return lhs_value;
        }
        else
        {
            throw std::runtime_error(fmt::format("{}: Code generation for binary operator '{}' not implemented.", slang::to_string(loc), op));
        }
    }

    if(is_assignment)
    {
        if(mc == memory_context::store)
        {
            throw cg::codegen_error(fmt::format("{}: Invalid memory context for assignment.", slang::to_string(loc)));
        }

        auto rhs_value = rhs->generate_code(ctx, memory_context::load);
        auto lhs_value = lhs->generate_code(ctx, memory_context::store);

        if(lhs_value->get_type() != rhs_value->get_type())
        {
            throw cg::codegen_error(fmt::format("{}: Types don't match in assignment. LHS: {}, RHS: {}.", slang::to_string(loc), lhs_value->get_type(), rhs_value->get_type()));
        }

        if(mc == memory_context::load)
        {
            lhs_value = lhs->generate_code(ctx, memory_context::load);
        }

        return lhs_value;
    }

    throw std::runtime_error(fmt::format("{}: binary_expression::generate_code not implemented for '{}'.", slang::to_string(loc), op));
}

std::string binary_expression::to_string() const
{
    return fmt::format("Binary(op=\"{}\", lhs={}, rhs={})", op,
                       lhs ? lhs->to_string() : std::string("<none>"),
                       rhs ? rhs->to_string() : std::string("<none>"));
}

/*
 * prototype_ast.
 */

cg::function* prototype_ast::generate_code(cg::context* ctx, memory_context mc) const
{
    if(mc != memory_context::none)
    {
        throw cg::codegen_error(fmt::format("{}: Invalid memory context for prototype_ast.", slang::to_string(loc)));
    }

    std::vector<std::unique_ptr<cg::variable>> function_args;
    for(auto& a: args)
    {
        if(is_builtin_type(std::get<2>(a)))
        {
            function_args.emplace_back(std::make_unique<cg::variable>(std::get<1>(a), std::get<2>(a)));
        }
        else
        {
            function_args.emplace_back(std::make_unique<cg::variable>(std::get<1>(a), "composite", std::get<2>(a)));
        }
    }

    return ctx->create_function(std::get<1>(name), std::get<1>(return_type), std::move(function_args));
}

std::string prototype_ast::to_string() const
{
    std::string ret = fmt::format("Prototype(name={}, return_type={}, args=(", std::get<1>(name), std::get<1>(return_type));
    for(std::size_t i = 0; i < args.size() - 1; ++i)
    {
        ret += fmt::format("(name={}, type={}), ", std::get<1>(args[i]), std::get<2>(args[i]));
    }
    if(args.size() > 0)
    {
        ret += fmt::format("(name={}, type={})", std::get<1>(args.back()), std::get<2>(args.back()));
    }
    ret += "))";
    return ret;
}

/*
 * block.
 */

std::unique_ptr<cg::value> block::generate_code(cg::context* ctx, memory_context mc) const
{
    if(mc != memory_context::none)
    {
        throw cg::codegen_error(fmt::format("{}: Invalid memory context for code block.", slang::to_string(loc)));
    }

    std::unique_ptr<cg::value> v;
    for(auto& expr: this->exprs)
    {
        v = expr->generate_code(ctx, memory_context::none);
    }
    return v;
}

std::string block::to_string() const
{
    std::string ret = "Block(exprs=(";
    for(std::size_t i = 0; i < exprs.size() - 1; ++i)
    {
        ret += fmt::format("{}, ", exprs[i] ? exprs[i]->to_string() : std::string("<none>"));
    }
    if(exprs.size() > 0)
    {
        ret += fmt::format("{}", exprs.back() ? exprs.back()->to_string() : std::string("<none>"));
    }
    ret += "))";
    return ret;
}

/*
 * function_expression.
 */

std::unique_ptr<slang::codegen::value> function_expression::generate_code(cg::context* ctx, memory_context mc) const
{
    if(mc != memory_context::none)
    {
        throw cg::codegen_error(fmt::format("{}: Invalid memory context for function_expression.", slang::to_string(loc)));
    }

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

std::string function_expression::to_string() const
{
    return fmt::format("Function(prototype={}, body={})",
                       prototype ? prototype->to_string() : std::string("<none>"),
                       body ? body->to_string() : std::string("<none>"));
}

/*
 * call_expression.
 */

std::unique_ptr<slang::codegen::value> call_expression::generate_code(cg::context* ctx, memory_context mc) const
{
    if(mc == memory_context::store)
    {
        throw cg::codegen_error(fmt::format("{}: Cannot store into call expression.", slang::to_string(loc)));
    }

    for(auto& arg: args)
    {
        arg->generate_code(ctx, memory_context::load);
    }
    ctx->generate_invoke(std::make_unique<cg::function_argument>(std::get<1>(callee)));
    return {};    // FIXME return the return type of the function?
}

std::string call_expression::to_string() const
{
    std::string ret = fmt::format("Call(callee={}, args=(", std::get<1>(callee));
    for(std::size_t i = 0; i < args.size() - 1; ++i)
    {
        ret += fmt::format("{}, ", args[i] ? args[i]->to_string() : std::string("<none>"));
    }
    if(args.size() > 0)
    {
        ret += fmt::format("{}, ", args.back() ? args.back()->to_string() : std::string("<none>"));
    }
    ret += "))";
    return ret;
}

/*
 * return_statement.
 */

std::unique_ptr<slang::codegen::value> return_statement::generate_code(cg::context* ctx, memory_context mc) const
{
    if(mc != memory_context::none)
    {
        throw cg::codegen_error(fmt::format("{}: Invalid memory context for return_statement.", slang::to_string(loc)));
    }

    auto v = expr->generate_code(ctx);
    if(!v)
    {
        throw cg::codegen_error(fmt::format("{}: Expression did not yield a type.", slang::to_string(loc)));
    }
    ctx->generate_ret(*v);
    return {};
}

std::string return_statement::to_string() const
{
    if(expr)
    {
        return fmt::format("Return(expr={})", expr->to_string());
    }
    else
    {
        return "Return()";
    }
}

/*
 * if_statement.
 */

std::unique_ptr<slang::codegen::value> if_statement::generate_code(cg::context* ctx, memory_context mc) const
{
    // TODO
    throw std::runtime_error(fmt::format("{}: if_statement::generate_code not implemented.", slang::to_string(loc)));
}

std::string if_statement::to_string() const
{
    return fmt::format("If(condition={}, if_block={}, else_block={})",
                       condition ? condition->to_string() : std::string("<none>"),
                       if_block ? if_block->to_string() : std::string("<none>"),
                       else_block ? else_block->to_string() : std::string("<none>"));
}

/*
 * while_statement.
 */

std::unique_ptr<slang::codegen::value> while_statement::generate_code(cg::context* ctx, memory_context mc) const
{
    // TODO
    throw std::runtime_error(fmt::format("{}: while_statement::generate_code not implemented.", slang::to_string(loc)));
}

std::string while_statement::to_string() const
{
    return fmt::format("While(condition={}, while_block={})",
                       condition ? condition->to_string() : std::string("<none>"),
                       while_block ? while_block->to_string() : std::string("<none>"));
}

/*
 * break_statement.
 */

std::unique_ptr<slang::codegen::value> break_statement::generate_code(cg::context* ctx, memory_context mc) const
{
    // TODO
    throw std::runtime_error(fmt::format("{}: break_statement::generate_code not implemented.", slang::to_string(loc)));
}

/*
 * continue_statement.
 */

std::unique_ptr<slang::codegen::value> continue_statement::generate_code(cg::context* ctx, memory_context mc) const
{
    // TODO
    throw std::runtime_error(fmt::format("{}: continue_statement::generate_code not implemented.", slang::to_string(loc)));
}

}    // namespace slang::ast