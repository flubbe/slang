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
            throw cg::codegen_error(fmt::format("Cannot store into int_literal '{}'.", std::get<int>(*value)));
        }
        else if(type == literal_type::fp_literal)
        {
            throw cg::codegen_error(fmt::format("Cannot store into fp_literal '{}'.", std::get<float>(*value)));
        }
        else if(type == literal_type::str_literal)
        {
            throw cg::codegen_error(fmt::format("Cannot store into str_literal '{}'.", std::get<std::string>(*value)));
        }
        else
        {
            throw cg::codegen_error(fmt::format("Cannot store into unknown literal of type id '{}'.", static_cast<int>(type)));
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
        throw cg::codegen_error(fmt::format("Unable to generate code for literal of type id '{}'.", static_cast<int>(type)));
    }

    // TODO
    return {};
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
    throw std::runtime_error("signed_expression::generate_code not implemented.");
}

std::string signed_expression::to_string() const
{
    return fmt::format("Signed(sign={}, expr={})", sign, expr ? expr->to_string() : std::string("<none>"));
}

/*
 * scope_expression.
 */

std::unique_ptr<cg::value> scope_expression::generate_code(cg::context* ctx, memory_context mc) const
{
    // TODO
    throw std::runtime_error("scope_expression::generate_code not implemented.");
}

std::string scope_expression::to_string() const
{
    return fmt::format("Scope(name={}, expr={})", name, expr ? expr->to_string() : std::string("<none>"));
}

/*
 * access_expression.
 */

std::unique_ptr<cg::value> access_expression::generate_code(cg::context* ctx, memory_context mc) const
{
    // TODO
    throw std::runtime_error("access_expression::generate_code not implemented.");
}

std::string access_expression::to_string() const
{
    return fmt::format("Access(name={}, expr={})", name, expr ? expr->to_string() : std::string("<none>"));
}

/*
 * import_expression.
 */

std::unique_ptr<cg::value> import_expression::generate_code(cg::context* ctx, memory_context mc) const
{
    // TODO
    throw std::runtime_error("import_expression::generate_code not implemented.");
}

std::string import_expression::to_string() const
{
    return fmt::format("Import(path={})", slang::utils::join(path, "."));
}

/*
 * variable_reference_expression.
 */

std::unique_ptr<cg::value> variable_reference_expression::generate_code(cg::context* ctx, memory_context mc) const
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
    return fmt::format("VariableReference(name={})", name);
}

/*
 * variable_declaration_expression.
 */

std::unique_ptr<cg::value> variable_declaration_expression::generate_code(cg::context* ctx, memory_context mc) const
{
    if(mc != memory_context::none)
    {
        throw cg::codegen_error("Invalid memory context for variable declaration.");
    }

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

std::string variable_declaration_expression::to_string() const
{
    return fmt::format("VariableDeclaration(name={}, type={}, expr={})", name, type, expr ? expr->to_string() : std::string("<none>"));
}

/*
 * struct_definition_expression.
 */

std::unique_ptr<cg::value> struct_definition_expression::generate_code(cg::context* ctx, memory_context mc) const
{
    // TODO
    throw std::runtime_error("struct_definition_expression::generate_code not implemented.");
}

std::string struct_definition_expression::to_string() const
{
    std::string ret = fmt::format("Struct(name={}, members=(", name);
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
    throw std::runtime_error("struct_anonymous_initializer_expression::generate_code not implemented.");
}

std::string struct_anonymous_initializer_expression::to_string() const
{
    std::string ret = fmt::format("StructAnonymousInitializer(name={}, initializers=(", name);
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
    throw std::runtime_error("struct_named_initializer_expression::generate_code not implemented.");
}

std::string struct_named_initializer_expression::to_string() const
{
    std::string ret = fmt::format("StructNamedInitializer(name={}, initializers=(", name);

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

        // TODO Compound assignments
        throw std::runtime_error("Compound assignments not implemented in binary_expression::generate_code.");
    }

    if(is_assignment)
    {
        if(mc == memory_context::store)
        {
            throw cg::codegen_error("Invalid memory context for assignment.");
        }

        auto rhs_value = rhs->generate_code(ctx, memory_context::load);
        auto lhs_value = lhs->generate_code(ctx, memory_context::store);

        if(lhs_value->get_type() != rhs_value->get_type())
        {
            throw cg::codegen_error(fmt::format("Types don't match in assignment. LHS: {}, RHS: {}.", lhs_value->get_type(), rhs_value->get_type()));
        }

        if(mc == memory_context::load)
        {
            lhs_value = lhs->generate_code(ctx, memory_context::load);
        }

        return lhs_value;
    }

    if(!is_assignment)
    {
        if(mc == memory_context::store)
        {
            throw cg::codegen_error("Invalid memory context for binary operator.");
        }

        auto lhs_value = lhs->generate_code(ctx, memory_context::load);
        auto rhs_value = rhs->generate_code(ctx, memory_context::load);

        if(lhs_value->get_type() != rhs_value->get_type())
        {
            throw cg::codegen_error(fmt::format("Types don't match in binary operation. LHS: {}, RHS: {}.", lhs_value->get_type(), rhs_value->get_type()));
        }

        std::unordered_map<std::string, cg::binary_op> op_map = {
          {"+", cg::binary_op::op_add},
          {"-", cg::binary_op::op_sub},
          {"*", cg::binary_op::op_mul},
          {"/", cg::binary_op::op_div},
          {"%", cg::binary_op::op_mod},
          {"&", cg::binary_op::op_and},
          {"|", cg::binary_op::op_or},
          {"<<", cg::binary_op::op_shl},
          {">>", cg::binary_op::op_shr}};

        auto it = op_map.find(op);
        if(it != op_map.end())
        {
            ctx->generate_binary_op(it->second, *lhs_value);
            return lhs_value;
        }
        else
        {
            throw std::runtime_error(fmt::format("Code generation for binary operator '{}' not implemented.", op));
        }
    }

    throw std::runtime_error(fmt::format("binary_expression::generate_code not implemented for '{}'.", op));
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
        throw cg::codegen_error("Invalid memory context for prototype_ast.");
    }

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

std::string prototype_ast::to_string() const
{
    std::string ret = fmt::format("Prototype(name={}, return_type={}, args=(", name, return_type);
    for(std::size_t i = 0; i < args.size() - 1; ++i)
    {
        ret += fmt::format("(name={}, type={}), ", args[i].first, args[i].second);
    }
    if(args.size() > 0)
    {
        ret += fmt::format("(name={}, type={})", args.back().first, args.back().second);
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
        throw cg::codegen_error("Invalid memory context for code block.");
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
        throw cg::codegen_error("Invalid memory context for function_expression.");
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
    // TODO
    throw std::runtime_error("call_expression::generate_code not implemented.");
}

std::string call_expression::to_string() const
{
    std::string ret = fmt::format("Call(callee={}, args=(", callee);
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
        throw cg::codegen_error("Invalid memory context for return_statement.");
    }

    auto v = expr->generate_code(ctx);
    if(!v)
    {
        throw cg::codegen_error("Expression did not yield a type.");
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
    throw std::runtime_error("if_statement::generate_code not implemented.");
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
    throw std::runtime_error("while_statement::generate_code not implemented.");
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
    throw std::runtime_error("break_statement::generate_code not implemented.");
}

/*
 * continue_statement.
 */

std::unique_ptr<slang::codegen::value> continue_statement::generate_code(cg::context* ctx, memory_context mc) const
{
    // TODO
    throw std::runtime_error("continue_statement::generate_code not implemented.");
}

}    // namespace slang::ast