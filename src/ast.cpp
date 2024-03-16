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
#include "typing.h"
#include "utils.h"

/*
 * Code generation from AST.
 */

namespace cg = slang::codegen;
namespace ty = slang::typing;

namespace slang::ast
{

/**
 * Check whether a string represents a built-in type, that is,
 * void, i32, f32 or str.
 */
static bool is_builtin_type(const std::string& s)
{
    return s == "void" || s == "i32" || s == "f32" || s == "str";
}

/*
 * literal_expression.
 */

std::unique_ptr<cg::value> literal_expression::generate_code(cg::context* ctx, memory_context mc) const
{
    if(!tok.value)
    {
        throw cg::codegen_error(loc, "Empty literal.");
    }

    if(mc == memory_context::store)
    {
        if(tok.type == token_type::int_literal)
        {
            throw cg::codegen_error(loc, fmt::format("Cannot store into int_literal '{}'.", std::get<int>(*tok.value)));
        }
        else if(tok.type == token_type::fp_literal)
        {
            throw cg::codegen_error(loc, fmt::format("Cannot store into fp_literal '{}'.", std::get<float>(*tok.value)));
        }
        else if(tok.type == token_type::str_literal)
        {
            throw cg::codegen_error(loc, fmt::format("Cannot store into str_literal '{}'.", std::get<std::string>(*tok.value)));
        }
        else
        {
            throw cg::codegen_error(loc, fmt::format("Cannot store into unknown literal of type id '{}'.", static_cast<int>(tok.type)));
        }
    }

    if(tok.type == token_type::int_literal)
    {
        ctx->generate_const({"i32"}, *tok.value);
        return std::make_unique<cg::value>("i32");
    }
    else if(tok.type == token_type::fp_literal)
    {
        ctx->generate_const({"f32"}, *tok.value);
        return std::make_unique<cg::value>("f32");
    }
    else if(tok.type == token_type::str_literal)
    {
        ctx->generate_const({"str"}, *tok.value);
        return std::make_unique<cg::value>("str");
    }
    else
    {
        throw cg::codegen_error(loc, fmt::format("Unable to generate code for literal of type id '{}'.", static_cast<int>(tok.type)));
    }
}

std::optional<std::string> literal_expression::type_check(ty::context& ctx) const
{
    if(!tok.value)
    {
        throw ty::type_error(loc, "Empty literal.");
    }

    if(tok.type == token_type::int_literal)
    {
        return "i32";
    }
    else if(tok.type == token_type::fp_literal)
    {
        return "f32";
    }
    else if(tok.type == token_type::str_literal)
    {
        return "str";
    }
    else
    {
        throw ty::type_error(tok.location, fmt::format("Unknown literal type with id '{}'.", static_cast<int>(tok.type)));
    }
}

std::string literal_expression::to_string() const
{
    if(tok.type == token_type::fp_literal)
    {
        if(tok.value)
        {
            return fmt::format("FloatLiteral(value={})", std::get<float>(*tok.value));
        }
        else
        {
            return "FloatLiteral(<none>)";
        }
    }
    else if(tok.type == token_type::int_literal)
    {
        if(tok.value)
        {
            return fmt::format("IntLiteral(value={})", std::get<int>(*tok.value));
        }
        else
        {
            return "IntLiteral(<none>)";
        }
    }
    else if(tok.type == token_type::str_literal)
    {
        if(tok.value)
        {
            return fmt::format("StrLiteral(value=\"{}\")", std::get<std::string>(*tok.value));
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

std::optional<std::string> signed_expression::type_check(ty::context& ctx) const
{
    auto expr_type = expr->type_check(ctx);

    if(expr_type == std::nullopt)
    {
        throw ty::type_error(loc, "Unable to determine type of signed expression.");
    }

    if(*expr_type != "i32" && *expr_type != "f32")
    {
        throw ty::type_error(loc, fmt::format("Expected type 'i32' or 'f32' for signed expression, got '{}'.", *expr_type));
    }

    return expr_type;
}

std::string signed_expression::to_string() const
{
    return fmt::format("Signed(sign={}, expr={})", sign.s, expr ? expr->to_string() : std::string("<none>"));
}

/*
 * type_cast_expression.
 */

std::unique_ptr<cg::value> type_cast_expression::generate_code(cg::context* ctx, memory_context mc) const
{
    // TODO
    throw std::runtime_error(fmt::format("{}: type_cast_expression::generate_code not implemented.", slang::to_string(loc)));
}

std::optional<std::string> type_cast_expression::type_check(ty::context& ctx) const
{
    // TODO
    throw std::runtime_error(fmt::format("{}: type_cast_expression::type_check not implemented.", slang::to_string(loc)));
}

std::string type_cast_expression::to_string() const
{
    return fmt::format("TypeCast(expr={}, target_type={})", expr ? expr->to_string() : std::string("<none>"), target_type.s);
}

/*
 * scope_expression.
 */

std::unique_ptr<cg::value> scope_expression::generate_code(cg::context* ctx, memory_context mc) const
{
    // TODO
    throw std::runtime_error(fmt::format("{}: scope_expression::generate_code not implemented.", slang::to_string(loc)));
}

std::optional<std::string> scope_expression::type_check(ty::context& ctx) const
{
    ctx.enter_named_scope(name.s);
    auto type = expr->type_check(ctx);
    ctx.exit_scope(name.s);

    return type;
}

std::string scope_expression::to_string() const
{
    return fmt::format("Scope(name={}, expr={})", name.s, expr ? expr->to_string() : std::string("<none>"));
}

/*
 * access_expression.
 */

std::unique_ptr<cg::value> access_expression::generate_code(cg::context* ctx, memory_context mc) const
{
    // TODO
    throw std::runtime_error(fmt::format("{}: access_expression::generate_code not implemented.", slang::to_string(loc)));
}

std::optional<std::string> access_expression::type_check(ty::context& ctx) const
{
    // TODO
    throw std::runtime_error(fmt::format("{}: access_expression::type_check not implemented.", slang::to_string(loc)));
}

std::string access_expression::to_string() const
{
    return fmt::format("Access(name={}, expr={})", name.s, expr ? expr->to_string() : std::string("<none>"));
}

/*
 * import_expression.
 */

std::unique_ptr<cg::value> import_expression::generate_code(cg::context* ctx, memory_context mc) const
{
    // TODO
    throw std::runtime_error(fmt::format("{}: import_expression::generate_code not implemented.", slang::to_string(loc)));
}

std::optional<std::string> import_expression::type_check(ty::context& ctx) const
{
    return std::nullopt;
}

std::string import_expression::to_string() const
{
    auto transform = [](const token& p) -> std::string
    { return p.s; };

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
        throw cg::codegen_error(loc, fmt::format("No scope to search for '{}'.", name.s));
    }

    const cg::variable* var{nullptr};
    while(s != nullptr)
    {
        if((var = s->get_variable(name.s)) != nullptr)
        {
            break;
        }
        s = s->get_outer();
    }

    if(s == nullptr)
    {
        throw cg::codegen_error(loc, fmt::format("Cannot find variable '{}' in current scope.", name.s));
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

std::optional<std::string> variable_reference_expression::type_check(ty::context& ctx) const
{
    return ctx.get_type(name.s);
}

std::string variable_reference_expression::to_string() const
{
    return fmt::format("VariableReference(name={})", name.s);
}

/*
 * variable_declaration_expression.
 */

std::unique_ptr<cg::value> variable_declaration_expression::generate_code(cg::context* ctx, memory_context mc) const
{
    if(mc != memory_context::none)
    {
        throw cg::codegen_error(loc, fmt::format("Invalid memory context for variable declaration."));
    }

    cg::scope* s = ctx->get_scope();
    if(s == nullptr)
    {
        throw cg::codegen_error(loc, fmt::format("No scope available for adding locals."));
    }
    s->add_local(std::make_unique<cg::variable>(name.s, type.s));

    if(expr)
    {
        auto v = expr->generate_code(ctx);
        if(is_builtin_type(type.s))
        {
            ctx->generate_store(std::make_unique<cg::variable_argument>(cg::variable{name.s, type.s}));
        }
        else
        {
            ctx->generate_store(std::make_unique<cg::variable_argument>(cg::variable{name.s, "composite", type.s}));
        }
    }

    return {};
}

std::optional<std::string> variable_declaration_expression::type_check(ty::context& ctx) const
{
    ctx.add_type(name.s, type.s);

    if(expr)
    {
        auto rhs = expr->type_check(ctx);

        if(!rhs)
        {
            throw ty::type_error(name.location, fmt::format("Expression has no type."));
        }

        if(*rhs != type.s)
        {
            throw ty::type_error(name.location, fmt::format("R.h.s. has type '{}', which does not match the variable type '{}'.", *rhs, type.s));
        }
    }

    return std::nullopt;
}

std::string variable_declaration_expression::to_string() const
{
    return fmt::format("VariableDeclaration(name={}, type={}, expr={})", name.s, type.s, expr ? expr->to_string() : std::string("<none>"));
}

/*
 * struct_definition_expression.
 */

std::unique_ptr<cg::value> struct_definition_expression::generate_code(cg::context* ctx, memory_context mc) const
{
    // TODO
    throw std::runtime_error(fmt::format("{}: struct_definition_expression::generate_code not implemented.", slang::to_string(loc)));
}

std::optional<std::string> struct_definition_expression::type_check(ty::context& ctx) const
{
    ctx.enter_named_scope(name.s);
    for(auto& m: members)
    {
        m->type_check(ctx);
    }
    ctx.exit_scope(name.s);

    return std::nullopt;
}

std::string struct_definition_expression::to_string() const
{
    std::string ret = fmt::format("Struct(name={}, members=(", name.s);
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

std::optional<std::string> struct_anonymous_initializer_expression::type_check(ty::context& ctx) const
{
    // TODO
    throw std::runtime_error(fmt::format("{}: struct_anonymous_initializer_expression::type_check not implemented.", slang::to_string(loc)));
}

std::string struct_anonymous_initializer_expression::to_string() const
{
    std::string ret = fmt::format("StructAnonymousInitializer(name={}, initializers=(", name.s);
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

std::optional<std::string> struct_named_initializer_expression::type_check(ty::context& ctx) const
{
    // TODO
    throw std::runtime_error(fmt::format("{}: struct_named_initializer_expression::type_check not implemented.", slang::to_string(loc)));
}

std::string struct_named_initializer_expression::to_string() const
{
    std::string ret = fmt::format("StructNamedInitializer(name={}, initializers=(", name.s);

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

static std::tuple<bool, bool, std::string> classify_binary_op(const std::string& s)
{
    bool is_assignment = (s == "=" || s == "+=" || s == "-="
                          || s == "*=" || s == "/=" || s == "%="
                          || s == "&=" || s == "|=" || s == "<<=" || s == ">>=");
    bool is_compound = is_assignment && (s != "=");

    std::string reduced_op = s;
    if(is_compound)
    {
        reduced_op.pop_back();
    }

    return {is_assignment, is_compound, reduced_op};
}

std::unique_ptr<cg::value> binary_expression::generate_code(cg::context* ctx, memory_context mc) const
{
    auto [is_assignment, is_compound, reduced_op] = classify_binary_op(op.s);

    if(!is_assignment || is_compound)
    {
        if(mc == memory_context::store)
        {
            throw cg::codegen_error(loc, "Invalid memory context for binary operator.");
        }

        auto lhs_value = lhs->generate_code(ctx, memory_context::load);
        auto rhs_value = rhs->generate_code(ctx, memory_context::load);

        if(lhs_value->get_type() != rhs_value->get_type())
        {
            throw cg::codegen_error(loc, fmt::format("Types don't match in binary operation. LHS: {}, RHS: {}.", lhs_value->get_type(), rhs_value->get_type()));
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
            throw std::runtime_error(fmt::format("{}: Code generation for binary operator '{}' not implemented.", slang::to_string(loc), op.s));
        }
    }

    if(is_assignment)
    {
        if(mc == memory_context::store)
        {
            throw cg::codegen_error(loc, "Invalid memory context for assignment.");
        }

        auto rhs_value = rhs->generate_code(ctx, memory_context::load);
        auto lhs_value = lhs->generate_code(ctx, memory_context::store);

        // TODO This is handled by the type system.
        if(lhs_value->get_type() != rhs_value->get_type())
        {
            throw cg::codegen_error(loc, fmt::format("Types don't match in assignment. LHS: {}, RHS: {}.", lhs_value->get_type(), rhs_value->get_type()));
        }

        if(mc == memory_context::load)
        {
            lhs_value = lhs->generate_code(ctx, memory_context::load);
        }

        return lhs_value;
    }

    throw std::runtime_error(fmt::format("{}: binary_expression::generate_code not implemented for '{}'.", slang::to_string(loc), op.s));
}

std::optional<std::string> binary_expression::type_check(ty::context& ctx) const
{
    auto [is_assignment, is_compound, reduced_op] = classify_binary_op(op.s);

    auto lhs_type = lhs->type_check(ctx);
    auto rhs_type = rhs->type_check(ctx);

    if(lhs_type == std::nullopt || rhs_type == std::nullopt)
    {
        throw ty::type_error(loc, fmt::format("Could not infer types for binary operator '{}'.", reduced_op));
    }

    // some operations restrict the type.
    if(reduced_op == "%"
       || reduced_op == "<<" || reduced_op == ">>"
       || reduced_op == "&" || reduced_op == "^" || reduced_op == "|"
       || reduced_op == "&&" || reduced_op == "||")
    {
        if(*lhs_type != "i32" || *rhs_type != "i32")
        {
            throw ty::type_error(loc, fmt::format("Got binary expression of type '{}' {} '{}', expected 'i32' {} 'i32'.", *lhs_type, reduced_op, *rhs_type, reduced_op));
        }

        // return the restricted type.
        return {"i32"};
    }

    if(*lhs_type != *rhs_type)
    {
        throw ty::type_error(loc, fmt::format("Types don't match in binary expression. Got expression of type '{}' {} '{}'.", *lhs_type, reduced_op, *rhs_type));
    }

    return lhs_type;
}

std::string binary_expression::to_string() const
{
    return fmt::format("Binary(op=\"{}\", lhs={}, rhs={})", op.s,
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
        throw cg::codegen_error(loc, "Invalid memory context for prototype_ast.");
    }

    std::vector<std::unique_ptr<cg::variable>> function_args;
    for(auto& a: args)
    {
        if(is_builtin_type(a.second.s))
        {
            function_args.emplace_back(std::make_unique<cg::variable>(a.first.s, a.second.s));
        }
        else
        {
            function_args.emplace_back(std::make_unique<cg::variable>(a.first.s, "composite", a.second.s));
        }
    }

    return ctx->create_function(name.s, return_type.s, std::move(function_args));
}

std::optional<std::string> prototype_ast::type_check(ty::context& ctx) const
{
    // TODO
    throw std::runtime_error(fmt::format("{}: prototype_ast::type_check not implemented.", slang::to_string(loc)));
}

std::string prototype_ast::to_string() const
{
    std::string ret = fmt::format("Prototype(name={}, return_type={}, args=(", name.s, return_type.s);
    for(std::size_t i = 0; i < args.size() - 1; ++i)
    {
        ret += fmt::format("(name={}, type={}), ", args[i].first.s, args[i].second.s);
    }
    if(args.size() > 0)
    {
        ret += fmt::format("(name={}, type={})", args.back().first.s, args.back().second.s);
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
        throw cg::codegen_error(loc, "Invalid memory context for code block.");
    }

    std::unique_ptr<cg::value> v;
    for(auto& expr: this->exprs)
    {
        v = expr->generate_code(ctx, memory_context::none);
    }
    return v;
}

std::optional<std::string> block::type_check(ty::context& ctx) const
{
    for(auto& expr: exprs)
    {
        expr->type_check(ctx);
    }

    return std::nullopt;
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
        throw cg::codegen_error(loc, "Invalid memory context for function_expression.");
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

std::optional<std::string> function_expression::type_check(ty::context& ctx) const
{
    // TODO
    throw std::runtime_error(fmt::format("{}: function_expression::type_check not implemented.", slang::to_string(loc)));
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
        throw cg::codegen_error(loc, "Cannot store into call expression.");
    }

    for(auto& arg: args)
    {
        arg->generate_code(ctx, memory_context::load);
    }
    ctx->generate_invoke(std::make_unique<cg::function_argument>(callee.s));
    return {};    // FIXME return the return type of the function?
}

std::optional<std::string> call_expression::type_check(ty::context& ctx) const
{
    // TODO
    throw std::runtime_error(fmt::format("{}: call_expression::type_check not implemented.", slang::to_string(loc)));
}

std::string call_expression::to_string() const
{
    std::string ret = fmt::format("Call(callee={}, args=(", callee.s);
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
        throw cg::codegen_error(loc, "Invalid memory context for return_statement.");
    }

    auto v = expr->generate_code(ctx);
    if(!v)
    {
        throw cg::codegen_error(loc, "Expression did not yield a type.");
    }
    ctx->generate_ret(*v);
    return {};
}

std::optional<std::string> return_statement::type_check(ty::context& ctx) const
{
    // TODO
    throw std::runtime_error(fmt::format("{}: return_statement::type_check not implemented.", slang::to_string(loc)));
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

std::optional<std::string> if_statement::type_check(ty::context& ctx) const
{
    auto condition_type = condition->type_check(ctx);
    if(condition_type == std::nullopt)
    {
        throw ty::type_error(loc, "If condition has no type.");
    }

    if(*condition_type != "i32")
    {
        throw ty::type_error(loc, fmt::format("Expected if condition to be of type 'i32', got '{}", *condition_type));
    }

    ctx.enter_anonymous_scope();
    if_block->type_check(ctx);
    ctx.exit_anonymous_scope();

    if(else_block)
    {
        ctx.enter_anonymous_scope();
        else_block->type_check(ctx);
        ctx.exit_anonymous_scope();
    }

    return std::nullopt;
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

std::optional<std::string> while_statement::type_check(ty::context& ctx) const
{
    auto condition_type = condition->type_check(ctx);
    if(condition_type == std::nullopt)
    {
        throw ty::type_error(loc, "While condition has no type.");
    }

    if(*condition_type != "i32")
    {
        throw ty::type_error(loc, fmt::format("Expected while condition to be of type 'i32', got '{}", *condition_type));
    }

    ctx.enter_anonymous_scope();
    while_block->type_check(ctx);
    ctx.exit_anonymous_scope();

    return std::nullopt;
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