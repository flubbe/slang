/**
 * slang - a simple scripting language.
 *
 * abstract syntax tree.
 *
 * \author Felix Lubbe
 * \copyright Copyright (c) 2024
 * \license Distributed under the MIT software license (see accompanying LICENSE.txt).
 */

#include <unordered_map>
#include <set>

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
 * expression.
 */

void expression::add_directive(const token& name, const std::vector<std::pair<token, token>>& args)
{
    throw ty::type_error(name.location, fmt::format("Directive '{}' is not supported by the following expression.", name.s));
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
 * type_cast_expression.
 */

std::unique_ptr<cg::value> type_cast_expression::generate_code(cg::context* ctx, memory_context mc) const
{
    // TODO
    throw std::runtime_error(fmt::format("{}: type_cast_expression::generate_code not implemented.", slang::to_string(loc)));
}

std::optional<std::string> type_cast_expression::type_check(ty::context& ctx) const
{
    auto type = expr->type_check(ctx);
    if(type == std::nullopt)
    {
        throw ty::type_error(loc, "Invalid cast from untyped expression.");
    }

    // valid type casts.
    static const std::unordered_map<std::string, std::set<std::string>> valid_casts = {
      {"i32", {"i32", "f32"}},
      {"f32", {"i32", "f32"}}};

    auto cast_from = valid_casts.find(*type);
    if(cast_from == valid_casts.end())
    {
        throw ty::type_error(loc, fmt::format("Invalid cast from non-primitive type '{}'.", *type));
    }

    auto cast_to = cast_from->second.find(target_type.s);
    if(cast_to == cast_from->second.end())
    {
        throw ty::type_error(loc, fmt::format("Invalid cast to non-primitive type '{}'.", target_type.s));
    }

    return target_type.s;
}

std::string type_cast_expression::to_string() const
{
    return fmt::format("TypeCast(target_type={}, expr={})", target_type.s, expr ? expr->to_string() : std::string("<none>"));
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
    ctx.enter_function_scope(name);
    auto type = expr->type_check(ctx);
    ctx.exit_function_scope(name);

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
    auto type = ctx.get_type(name);
    const ty::struct_definition* struct_def = ctx.get_struct_definition(name.location, type);
    ctx.push_struct_definition(struct_def);
    auto expr_type = expr->type_check(ctx);
    ctx.pop_struct_definition();
    return expr_type;
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

void import_expression::collect_names(ty::context& ctx) const
{
    ctx.add_import(path);
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
 * directive_expression.
 */

std::unique_ptr<slang::codegen::value> directive_expression::generate_code(slang::codegen::context* ctx, memory_context mc) const
{
    expr->clear_directives();
    expr->add_directive(name, args);
    return expr->generate_code(ctx, mc);
}

std::optional<std::string> directive_expression::type_check(slang::typing::context& ctx) const
{
    return expr->type_check(ctx);
}

std::string directive_expression::to_string() const
{
    auto transform = [](const std::pair<token, token>& p) -> std::string
    { return fmt::format("{}, {}", p.first.s, p.second.s); };

    return fmt::format("Directive(name={}, args=({}), expr={})", name.s, slang::utils::join(args, {transform}, ","), expr->to_string());
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
    return ctx.get_type(name);
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
    ctx.add_variable(name, type);

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

void struct_definition_expression::collect_names(ty::context& ctx) const
{
    std::vector<std::pair<token, token>> struct_members;
    for(auto& m: members)
    {
        struct_members.emplace_back(m->get_name(), m->get_type());
    }
    ctx.add_type(name, std::move(struct_members));
}

std::optional<std::string> struct_definition_expression::type_check(ty::context& ctx) const
{
    for(auto& m: members)
    {
        m->type_check(ctx);
    }

    return std::nullopt;
}

std::string struct_definition_expression::to_string() const
{
    std::string ret = fmt::format("Struct(name={}, members=(", name.s);
    if(members.size() > 0)
    {
        for(std::size_t i = 0; i < members.size() - 1; ++i)
        {
            ret += fmt::format("{}, ", members[i]->to_string());
        }
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
    if(initializers.size() > 0)
    {
        for(std::size_t i = 0; i < initializers.size() - 1; ++i)
        {
            ret += fmt::format("{}, ", initializers[i]->to_string());
        }
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
        if(initializers.size() > 0)
        {
            for(std::size_t i = 0; i < initializers.size() - 1; ++i)
            {
                ret += fmt::format("{}={}, ", member_names[i]->to_string(), initializers[i]->to_string());
            }
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

        static const std::unordered_map<std::string, cg::binary_op> op_map = {
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
 * unary_ast.
 */

std::unique_ptr<cg::value> unary_ast::generate_code(cg::context* ctx, memory_context mc) const
{
    // TODO
    throw std::runtime_error(fmt::format("{}: unary_ast::generate_code not implemented", slang::to_string(loc)));
}

std::optional<std::string> unary_ast::type_check(slang::typing::context& ctx) const
{
    static const std::unordered_map<std::string, std::vector<std::string>> valid_operand_types = {
      {"+", {"i32", "f32"}},
      {"-", {"i32", "f32"}},
      {"!", {"i32"}},
      {"~", {"i32"}}};

    auto op_it = valid_operand_types.find(op.s);
    if(op_it == valid_operand_types.end())
    {
        throw ty::type_error(op.location, fmt::format("Unknown unary operator '{}'.", op.s));
    }

    auto operand_type = operand->type_check(ctx);
    if(operand_type == std::nullopt)
    {
        throw ty::type_error(op.location, fmt::format("Operand of unary operator '{}' has no type.", op.s));
    }

    auto type_it = std::find(op_it->second.begin(), op_it->second.end(), *operand_type);
    if(type_it == op_it->second.end())
    {
        throw ty::type_error(operand->get_location(), fmt::format("Invalid operand type '{}' for unary operator '{}'.", *operand_type, op.s));
    }

    return *type_it;
}

std::string unary_ast::to_string() const
{
    return fmt::format("Unary(op=\"{}\", operand={})", op.s, operand ? operand->to_string() : std::string("<none>"));
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

void prototype_ast::collect_names(ty::context& ctx) const
{
    std::vector<token> arg_types;
    std::transform(args.cbegin(), args.cend(), std::back_inserter(arg_types),
                   [](const auto& arg)
                   { return arg.second; });
    ctx.add_function(name, std::move(arg_types), return_type);
}

void prototype_ast::type_check(ty::context& ctx) const
{
    // enter function scope. the scope is exited in the type_check for the function's body.
    ctx.enter_function_scope(name);

    // add the arguments to the current scope.
    for(auto arg: args)
    {
        ctx.add_variable(arg.first, arg.second);
    }

    // check the return type.
    if(!is_builtin_type(return_type.s) && !ctx.has_type(return_type.s))
    {
        throw ty::type_error(return_type.location, fmt::format("Unknown return type '{}'.", return_type.s));
    }
}

void prototype_ast::finish_type_check(ty::context& ctx) const
{
    // exit the function's scope.
    ctx.exit_function_scope(name);
}

std::string prototype_ast::to_string() const
{
    std::string ret = fmt::format("Prototype(name={}, return_type={}, args=(", name.s, return_type.s);
    if(args.size() > 0)
    {
        for(std::size_t i = 0; i < args.size() - 1; ++i)
        {
            ret += fmt::format("(name={}, type={}), ", args[i].first.s, args[i].second.s);
        }
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

void block::collect_names(ty::context& ctx) const
{
    for(auto& expr: exprs)
    {
        expr->collect_names(ctx);
    }
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
    if(exprs.size() > 0)
    {
        for(std::size_t i = 0; i < exprs.size() - 1; ++i)
        {
            ret += fmt::format("{}, ", exprs[i] ? exprs[i]->to_string() : std::string("<none>"));
        }
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

void function_expression::collect_names(ty::context& ctx) const
{
    prototype->collect_names(ctx);
}

void function_expression::clear_directives()
{
    // TODO
    throw std::runtime_error("function_expression::clear_directives not implemented.");
}

void function_expression::add_directive(const token& name, const std::vector<std::pair<token, token>>& args)
{
    // TODO
    throw std::runtime_error("function_expression::add_directive not implemented.");
}

std::optional<std::string> function_expression::type_check(ty::context& ctx) const
{
    prototype->type_check(ctx);
    if(body)
    {
        body->type_check(ctx);
    }
    prototype->finish_type_check(ctx);

    return std::nullopt;
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
    auto sig = ctx.get_function_signature(callee);

    if(sig.arg_types.size() != args.size())
    {
        throw ty::type_error(callee.location, fmt::format("Wrong number of arguments in function call. Expected {}, got {}.", sig.arg_types.size(), args.size()));
    }

    for(std::size_t i = 0; i < args.size(); ++i)
    {
        auto arg_type = args[i]->type_check(ctx);
        if(arg_type == std::nullopt)
        {
            throw ty::type_error(args[i]->get_location(), fmt::format("Cannot evaluate type of argument {}.", i + 1));
        }

        if(sig.arg_types[i].s != *arg_type)
        {
            throw ty::type_error(args[i]->get_location(), fmt::format("Type of argument {} does not match signature: Expected '{}', got '{}'.", i + 1, sig.arg_types[i].s, *arg_type));
        }
    }

    return sig.ret_type.s;
}

std::string call_expression::to_string() const
{
    std::string ret = fmt::format("Call(callee={}, args=(", callee.s);
    if(args.size() > 0)
    {
        for(std::size_t i = 0; i < args.size() - 1; ++i)
        {
            ret += fmt::format("{}, ", args[i] ? args[i]->to_string() : std::string("<none>"));
        }
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
    auto sig = ctx.get_current_function();
    if(sig == std::nullopt)
    {
        throw ty::type_error(loc, "Cannot have return statement outside a function.");
    }

    if(sig->ret_type.s == "void")
    {
        if(expr)
        {
            throw ty::type_error(loc, fmt::format("Function '{}' declared as having no return value cannot have a return expression.", sig->name.s));
        }
    }
    else
    {
        auto ret_type = expr->type_check(ctx);
        if(ret_type == std::nullopt)
        {
            throw ty::type_error(loc, fmt::format("Function '{}': Return expression has no type, expected '{}'.", sig->name.s, sig->ret_type.s));
        }
        if(ret_type != sig->ret_type.s)
        {
            throw ty::type_error(loc, fmt::format("Function '{}': Return expression has type '{}', expected '{}'.", sig->name.s, *ret_type, sig->ret_type.s));
        }
    }

    return sig->ret_type.s;
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

    ctx.enter_anonymous_scope(if_block->get_location());
    if_block->type_check(ctx);
    ctx.exit_anonymous_scope();

    if(else_block)
    {
        ctx.enter_anonymous_scope(else_block->get_location());
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

    ctx.enter_anonymous_scope(while_block->get_location());
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