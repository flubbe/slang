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
#include <tuple>

#include "ast.h"
#include "codegen.h"
#include "module.h"
#include "typing.h"
#include "utils.h"

/*
 * Code generation from AST.
 */

namespace cg = slang::codegen;
namespace ty = slang::typing;

namespace slang::ast
{

/*
 * expression.
 */

access_expression* expression::as_access_expression()
{
    throw std::runtime_error("Expression is not an access expression.");
}

const access_expression* expression::as_access_expression() const
{
    throw std::runtime_error("Expression is not an access expression.");
}

named_expression* expression::as_named_expression()
{
    throw std::runtime_error("Expression is not a named expression.");
}

const named_expression* expression::as_named_expression() const
{
    throw std::runtime_error("Expression is not a named expression.");
}

void expression::push_directive(const token& name, const std::vector<std::pair<token, token>>& args)
{
    if(!supports_directive(name.s))
    {
        throw ty::type_error(name.location, fmt::format("Directive '{}' is not supported by the following expression.", name.s));
    }

    directive_stack.emplace_back(name, args);
}

void expression::pop_directive()
{
    if(directive_stack.size() == 0)
    {
        throw ty::type_error("Cannot pop directive for expression. Directive stack is empty.");
    }

    directive_stack.pop_back();
}

std::vector<directive> expression::get_directives(const std::string& s) const
{
    std::vector<directive> ret;
    for(auto& it: directive_stack)
    {
        if(it.name.s == s)
        {
            ret.emplace_back(it);
        }
    }
    return ret;
}

std::optional<directive> expression::get_unique_directive(const std::string& s) const
{
    auto directives = get_directives(s);
    if(directives.size() > 1)
    {
        throw cg::codegen_error(loc, fmt::format("More than one '{}' directive.", s));
    }

    if(directives.size() == 1)
    {
        return std::make_optional(std::move(directives[0]));
    }
    return std::nullopt;
}

/*
 * literal_expression.
 */

std::unique_ptr<cg::value> literal_expression::generate_code(cg::context& ctx, memory_context mc) const
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
        ctx.generate_const({cg::type{cg::type_class::i32, 0}}, *tok.value);
        return std::make_unique<cg::value>(cg::type{cg::type_class::i32, 0});
    }
    else if(tok.type == token_type::fp_literal)
    {
        ctx.generate_const({cg::type{cg::type_class::f32, 0}}, *tok.value);
        return std::make_unique<cg::value>(cg::type{cg::type_class::f32, 0});
    }
    else if(tok.type == token_type::str_literal)
    {
        ctx.generate_const({cg::type{cg::type_class::str, 0}}, *tok.value);
        return std::make_unique<cg::value>(cg::type{cg::type_class::str, 0});
    }
    else
    {
        throw cg::codegen_error(loc, fmt::format("Unable to generate code for literal of type id '{}'.", static_cast<int>(tok.type)));
    }
}

std::optional<ty::type_info> literal_expression::type_check(ty::context& ctx)
{
    if(!tok.value)
    {
        throw ty::type_error(loc, "Empty literal.");
    }

    if(tok.type == token_type::int_literal)
    {
        expr_type = ctx.get_type("i32", false);
        return expr_type;
    }
    else if(tok.type == token_type::fp_literal)
    {
        expr_type = ctx.get_type("f32", false);
        return expr_type;
    }
    else if(tok.type == token_type::str_literal)
    {
        expr_type = ctx.get_type("str", false);
        return expr_type;
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

std::unique_ptr<cg::value> type_cast_expression::generate_code(cg::context& ctx, memory_context mc) const
{
    auto v = expr->generate_code(ctx, mc);

    // only cast if necessary.
    if(target_type->get_name().s != v->get_type().to_string())    // FIXME namespaces
    {
        if(target_type->get_name().s == "i32" && v->get_type().get_type_class() == cg::type_class::f32)
        {
            ctx.generate_cast(cg::type_cast::f32_to_i32);
        }
        else if(target_type->get_name().s == "f32" && v->get_type().get_type_class() == cg::type_class::i32)
        {
            ctx.generate_cast(cg::type_cast::i32_to_f32);
        }
        else if(target_type->get_name().s == "str" && v->get_type().get_type_class() != cg::type_class::struct_)
        {
            throw cg::codegen_error(loc,
                                    fmt::format("Cannot cast '{}' to 'str'.", v->get_type().to_string()));
        }
        else
        {
            if(target_type->get_name().s == "str")
            {
                return std::make_unique<cg::value>(cg::type{cg::type_class::str, 0}, v->get_name());
            }

            auto cast_target_type = cg::type{
              cg::type_class::struct_,
              0,
              target_type->get_name().s,
              target_type->get_namespace_path()};

            // casts between non-builtin types are checked at run-time.
            ctx.generate_checkcast(cast_target_type);

            return std::make_unique<cg::value>(std::move(cast_target_type), v->get_name());
        }

        return std::make_unique<cg::value>(cg::type{cg::to_type_class(target_type->get_name().s), 0}, v->get_name());
    }

    // identity transformation.
    if(ty::is_builtin_type(target_type->to_string()))
    {
        return std::make_unique<cg::value>(cg::type{cg::to_type_class(target_type->get_name().s), 0}, v->get_name());
    }

    return std::make_unique<cg::value>(cg::type{cg::type_class::struct_, 0, target_type->get_name().s}, v->get_name());
}

std::optional<ty::type_info> type_cast_expression::type_check(ty::context& ctx)
{
    auto type = expr->type_check(ctx);
    if(type == std::nullopt)
    {
        throw ty::type_error(loc, "Invalid cast from untyped expression.");
    }

    // casts for primitive types.
    static const std::unordered_map<std::string, std::set<std::string>> primitive_type_casts = {
      {"i32", {"i32", "f32"}},
      {"f32", {"i32", "f32"}},
      {"str", {}}};

    auto cast_from = primitive_type_casts.find(ty::to_string(*type));
    if(cast_from != primitive_type_casts.end())
    {
        auto cast_to = cast_from->second.find(target_type->get_name().s);
        if(cast_to == cast_from->second.end())
        {
            throw ty::type_error(loc, fmt::format("Invalid cast to non-primitive type '{}'.", target_type->get_name().s));
        }
    }

    // casts for struct types. this is checked at run-time.
    expr_type = ctx.get_type(target_type->get_name().s, false, target_type->get_namespace_path());    // no array casts.
    return expr_type;
}

std::string type_cast_expression::to_string() const
{
    return fmt::format("TypeCast(target_type={}, expr={})", target_type->to_string(), expr ? expr->to_string() : std::string("<none>"));
}

/*
 * namespace_access_expression.
 */

std::unique_ptr<cg::value> namespace_access_expression::generate_code(cg::context& ctx, memory_context mc) const
{
    auto expr_namespace_stack = namespace_stack;
    expr_namespace_stack.push_back(name.s);
    expr->set_namespace(std::move(expr_namespace_stack));
    return expr->generate_code(ctx, mc);
}

std::optional<ty::type_info> namespace_access_expression::type_check(ty::context& ctx)
{
    auto expr_namespace_stack = namespace_stack;
    expr_namespace_stack.push_back(name.s);
    expr->set_namespace(std::move(expr_namespace_stack));
    return expr->type_check(ctx);
}

std::string namespace_access_expression::to_string() const
{
    return fmt::format("Scope(name={}, expr={})", name.s, expr ? expr->to_string() : std::string("<none>"));
}

/*
 * access_expression.
 */
class access_guard
{
    /** The associated context. */
    cg::context& ctx;

public:
    /** Deleted constructors. */
    access_guard() = delete;
    access_guard(const access_guard&) = delete;
    access_guard(access_guard&&) = delete;

    /** Deleted assignments. */
    access_guard& operator=(const access_guard&) = delete;
    access_guard& operator=(access_guard&&) = delete;

    /**
     * Access guard for a struct.
     *
     * @param ctx Code generation context.
     * @param ty The struct.
     */
    access_guard(cg::context& ctx, cg::type ty)
    : ctx{ctx}
    {
        ctx.push_struct_access(std::move(ty));
    }

    /** Destructor. */
    ~access_guard()
    {
        ctx.pop_struct_access();
    }
};

access_expression::access_expression(std::unique_ptr<ast::expression> lhs, std::unique_ptr<ast::expression> rhs)
: expression{lhs->get_location()}
, lhs{std::move(lhs)}
, rhs{std::move(rhs)}
{
}

std::unique_ptr<cg::value> access_expression::generate_code(cg::context& ctx, memory_context mc) const
{
    // validate expression.
    if(!expr_type.has_value())
    {
        throw cg::codegen_error(loc, "Access expression has no type.");
    }
    if(!rhs)
    {
        throw cg::codegen_error(loc, "Access expression has no r.h.s.");
    }

    auto lhs_value = lhs->generate_code(ctx, memory_context::load);

    /*
     * arrays.
     */
    if(lhs_type.is_array())
    {
        if(!rhs->is_named_expression())
        {
            throw cg::codegen_error(loc, fmt::format("Could not find name for element access in array access expression."));
        }
        auto identifier_expr = rhs->as_named_expression();

        if(identifier_expr->get_name().s == "length")
        {
            if(mc == memory_context::store)
            {
                throw cg::codegen_error(rhs->get_location(), "Array length is read only.");
            }

            ctx.generate_arraylength();
            return std::make_unique<cg::value>(cg::type{cg::type_class::i32, 0});
        }
        else
        {
            throw cg::codegen_error(rhs->get_location(), fmt::format("Unknown array property '{}'.", identifier_expr->get_name().s));
        }
    }

    /*
     * structs.
     */
    cg::type lhs_value_type = lhs_value->get_type();

    // validate expression.
    if(lhs_value_type.is_array())
    {
        throw cg::codegen_error(loc,
                                fmt::format(
                                  "Cannot load array '{}' as an object.",
                                  lhs_value->get_name().value_or("<none>")));
    }

    if(!lhs_value_type.is_struct())
    {
        throw cg::codegen_error(loc,
                                fmt::format(
                                  "Cannot access members of non-struct type '{}'.",
                                  lhs_value_type.to_string()));
    }

    // generate access instructions for rhs.
    access_guard ag{ctx, lhs_value_type};
    if(!rhs->is_named_expression())
    {
        return rhs->generate_code(ctx, mc);
    }

    cg::value member_value = ctx.get_struct_member(
      rhs->get_location(),
      lhs_value_type.to_string(),
      rhs->as_named_expression()->get_name().s,
      lhs_value_type.get_import_path());

    if(mc != memory_context::store)
    {
        ctx.generate_get_field(std::make_unique<cg::field_access_argument>(lhs_value_type, member_value));
    }
    // NOTE set_field instructions have to be generated by the caller

    return std::make_unique<cg::value>(member_value);
}

std::optional<ty::type_info> access_expression::type_check(ty::context& ctx)
{
    auto t = lhs->type_check(ctx);
    if(!t.has_value())
    {
        throw ty::type_error(loc, "Could not determine type of access expression.");
    }
    lhs_type = t.value();

    // array built-ins.
    if(lhs_type.is_array())
    {
        const ty::struct_definition* array_struct = ctx.get_struct_definition(lhs_type.get_location(), "@array");
        ctx.push_struct_definition(array_struct);
        expr_type = rhs->type_check(ctx);
        ctx.pop_struct_definition();
        return expr_type;
    }

    // structs.
    const ty::struct_definition* struct_def = ctx.get_struct_definition(
      lhs_type.get_location(), ty::to_string(lhs_type), lhs_type.get_import_path());
    ctx.push_struct_definition(struct_def);
    expr_type = rhs->type_check(ctx);
    ctx.pop_struct_definition();
    return expr_type;
}

std::string access_expression::to_string() const
{
    return fmt::format("Access(lhs={}, rhs={})", lhs->to_string(), rhs->to_string());
}

/*
 * import_expression.
 */

std::unique_ptr<cg::value> import_expression::generate_code([[maybe_unused]] cg::context& ctx, [[maybe_unused]] memory_context mc) const
{
    // import expressions are handled by the import resolver.
    return nullptr;
}

void import_expression::collect_names([[maybe_unused]] cg::context& ctx, ty::context& type_ctx) const
{
    type_ctx.add_import(path);
}

std::optional<ty::type_info> import_expression::type_check([[maybe_unused]] ty::context& ctx)
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

std::unique_ptr<cg::value> directive_expression::generate_code(cg::context& ctx, memory_context mc) const
{
    expr->push_directive(name, args);
    std::unique_ptr<cg::value> ret = expr->generate_code(ctx, mc);
    expr->pop_directive();
    return ret;
}

void directive_expression::collect_names(cg::context& ctx, ty::context& type_ctx) const
{
    expr->collect_names(ctx, type_ctx);
}

std::optional<ty::type_info> directive_expression::type_check(ty::context& ctx)
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

std::unique_ptr<cg::value> variable_reference_expression::generate_code(cg::context& ctx, memory_context mc) const
{
    cg::value v = get_value(ctx);

    if(mc == memory_context::load)
    {
        if(ctx.is_struct_access())
        {
            auto struct_type = ctx.get_accessed_struct();
            auto member_value = ctx.get_struct_member(
              loc,
              struct_type.to_string(),
              *v.get_name(),
              struct_type.get_import_path());

            ctx.generate_get_field(std::make_unique<cg::field_access_argument>(struct_type, member_value));
        }
        else
        {
            ctx.generate_load(std::make_unique<cg::variable_argument>(std::make_unique<cg::value>(v)));
        }

        if(element_expr)
        {
            element_expr->generate_code(ctx, memory_context::load);
            ctx.generate_load(std::make_unique<cg::type_argument>(v.get_type().deref()), true);
        }
    }
    else if(mc == memory_context::store)
    {
        if(element_expr)
        {
            ctx.generate_load(std::make_unique<cg::variable_argument>(std::make_unique<cg::value>(v)));
            element_expr->generate_code(ctx, memory_context::load);

            // if we're storing an element, generation of the store opcode needs to be deferred to the caller.
        }
        else
        {
            ctx.generate_store(std::make_unique<cg::variable_argument>(std::make_unique<cg::value>(v)));
        }
    }
    else
    {
        throw cg::codegen_error(loc, "Invalid memory context for variable reference expression.");
    }

    if(element_expr)
    {
        return std::make_unique<cg::value>(v.deref());
    }
    return std::make_unique<cg::value>(v);
}

std::optional<ty::type_info> variable_reference_expression::type_check(ty::context& ctx)
{
    if(element_expr)
    {
        auto v = element_expr->type_check(ctx);
        if(!v.has_value() || ty::to_string(*v) != "i32")
        {
            throw ty::type_error(loc, "Expected <integer> for array element access.");
        }

        auto t = ctx.get_identifier_type(name);
        if(!t.is_array())
        {
            throw ty::type_error(loc, "Cannot use subscript on non-array type.");
        }

        auto base_type = t.get_element_type();
        return ctx.get_type(base_type->to_string(), base_type->is_array());
    }
    return ctx.get_identifier_type(name);
}

std::string variable_reference_expression::to_string() const
{
    if(element_expr)
    {
        return fmt::format("VariableReference(name={}, element_expr={})", name.s, element_expr->to_string());
    }
    return fmt::format("VariableReference(name={})", name.s);
}

cg::value variable_reference_expression::get_value(cg::context& ctx) const
{
    if(!ctx.is_struct_access())
    {
        auto* scope = ctx.get_scope();
        if(scope == nullptr)
        {
            throw cg::codegen_error(loc, fmt::format("No scope to search for '{}'.", name.s));
        }

        while(scope != nullptr)
        {
            cg::value* v = scope->get_value(name.s);
            if(v != nullptr)
            {
                return *v;
            }
            scope = scope->get_outer();
        }

        throw cg::codegen_error(loc, fmt::format("Cannot find variable '{}' in current scope.", name.s));
    }
    else
    {
        auto struct_type = ctx.get_accessed_struct();
        return ctx.get_struct_member(
          loc,
          struct_type.get_struct_name().value(),
          get_name().s,
          struct_type.get_import_path());
    }
}

/*
 * type_expression.
 */

std::optional<std::string> type_expression::get_namespace_path() const
{
    if(namespace_stack.size() == 0)
    {
        return std::nullopt;
    }

    auto transform = [](const token& t) -> std::string
    {
        return t.s;
    };

    return slang::utils::join(namespace_stack, {transform}, "::");
}

std::string type_expression::to_string() const
{
    std::string namespace_string;
    if(namespace_stack.size() > 0)
    {
        for(std::size_t i = 0; i < namespace_stack.size() - 1; ++i)
        {
            namespace_string += fmt::format("{}, ", namespace_stack[i].s);
        }
        namespace_string += namespace_stack.back().s;
    }

    return fmt::format("TypeExpression(name={}, namespaces=({}), array={})", get_name().s, namespace_string, is_array());
}

/*
 * variable_declaration_expression.
 */

std::unique_ptr<cg::value> variable_declaration_expression::generate_code(cg::context& ctx, memory_context mc) const
{
    if(mc != memory_context::none)
    {
        throw cg::codegen_error(loc, fmt::format("Invalid memory context for variable declaration."));
    }

    cg::scope* s = ctx.get_scope();
    if(s == nullptr)
    {
        throw cg::codegen_error(loc, fmt::format("No scope available for adding locals."));
    }

    std::unique_ptr<cg::value> v;
    if(ty::is_builtin_type(type->get_name().s))
    {
        v = std::make_unique<cg::value>(
          cg::type{cg::to_type_class(type->get_name().s),
                   type->is_array() ? static_cast<std::size_t>(1) : static_cast<std::size_t>(0)},
          name.s);
        s->add_local(std::make_unique<cg::value>(*v));
    }
    else
    {
        v = std::make_unique<cg::value>(
          cg::type{cg::type_class::struct_,
                   type->is_array() ? static_cast<std::size_t>(1) : static_cast<std::size_t>(0),
                   type->get_name().s, type->get_namespace_path()},
          name.s);
        s->add_local(std::make_unique<cg::value>(*v));
    }

    if(is_array())
    {
        ctx.set_array_type(*v);
    }

    if(expr)
    {
        expr->generate_code(ctx, memory_context::load);
        ctx.generate_store(std::make_unique<cg::variable_argument>(std::move(v)));
    }

    if(is_array())
    {
        ctx.clear_array_type();
    }

    return nullptr;
}

std::optional<ty::type_info> variable_declaration_expression::type_check(ty::context& ctx)
{
    auto var_type = ctx.get_type(type->get_name(), is_array(), type->get_namespace_path());
    ctx.add_variable(name, var_type);

    if(expr)
    {
        auto rhs = expr->type_check(ctx);
        if(!rhs)
        {
            throw ty::type_error(name.location, fmt::format("Expression has no type."));
        }

        // Either the types match, or the type is a reference type which is set to 'null'.
        if(*rhs != var_type
           && !(ctx.is_reference_type(var_type) && *rhs == ctx.get_type("@null", false)))
        {
            throw ty::type_error(
              name.location,
              fmt::format("R.h.s. has type '{}' (type id {}), which does not match the variable's type '{}' (type id {}).",
                          ty::to_string(*rhs),
                          rhs->get_type_id(),
                          ty::to_string(var_type),
                          var_type.get_type_id()));
        }
    }

    return std::nullopt;
}

std::string variable_declaration_expression::to_string() const
{
    return fmt::format(
      "VariableDeclaration(name={}, type={}, expr={})",
      name.s,
      type->to_string(),
      expr ? expr->to_string() : std::string("<none>"));
}

/*
 * constant_expression.
 */

std::unique_ptr<cg::value> constant_expression::generate_code(cg::context& ctx, memory_context mc) const
{
    auto& tok = expr->get_token();
    if(tok.type == token_type::int_literal)
    {
        ctx.add_constant(tok.s, std::get<std::int32_t>(*tok.value));
        return std::make_unique<cg::value>(cg::type{cg::type_class::i32, 0});
    }
    else if(tok.type == token_type::fp_literal)
    {
        ctx.add_constant(tok.s, std::get<float>(*tok.value));
        return std::make_unique<cg::value>(cg::type{cg::type_class::f32, 0});
    }
    else if(tok.type == token_type::str_literal)
    {
        ctx.add_constant(tok.s, std::get<std::string>(*tok.value));
        return std::make_unique<cg::value>(cg::type{cg::type_class::str, 0});
    }

    throw cg::codegen_error(loc, fmt::format("Invalid token type id {} for literal.", static_cast<int>(tok.type)));
}

std::optional<ty::type_info> constant_expression::type_check(ty::context& ctx)
{
    auto const_type = ctx.get_type(type->get_name(), false, type->get_namespace_path());
    ctx.add_variable(name, const_type);    // FIXME for the typing context, constants and variables are the same right now.

    auto rhs = expr->type_check(ctx);
    if(!rhs)
    {
        throw ty::type_error(name.location, fmt::format("Expression has no type."));
    }

    // Either the types match, or the type is a reference type which is set to 'null'.
    if(*rhs != const_type
       && !(ctx.is_reference_type(const_type) && *rhs == ctx.get_type("@null", false)))
    {
        throw ty::type_error(
          name.location,
          fmt::format("R.h.s. has type '{}' (type id {}), which does not match the constant's type '{}' (type id {}).",
                      ty::to_string(*rhs),
                      rhs->get_type_id(),
                      ty::to_string(const_type),
                      const_type.get_type_id()));
    }

    return std::nullopt;
}

std::string constant_expression::to_string() const
{
    return fmt::format(
      "Constant(name={}, type={}, expr={})",
      name.s,
      type->to_string(),
      expr->to_string());
}

/*
 * array_initializer_expression.
 */

std::unique_ptr<cg::value> array_initializer_expression::generate_code(cg::context& ctx, [[maybe_unused]] memory_context mc) const
{
    std::unique_ptr<cg::value> v;
    auto array_type = ctx.get_array_type();

    if(exprs.size() >= std::numeric_limits<std::int32_t>::max())
    {
        throw cg::codegen_error(fmt::format("Cannot generate code for array initializer list: list size exceeds numeric limits ({} >= {}).", exprs.size(), std::numeric_limits<std::int32_t>::max()));
    }

    ctx.generate_const({cg::type{cg::type_class::i32, 0}}, static_cast<int>(exprs.size()));
    ctx.generate_newarray(array_type.deref());

    for(std::size_t i = 0; i < exprs.size(); ++i)
    {
        auto& expr = exprs[i];

        // the top of the stack contains the array address.
        ctx.generate_dup(array_type);
        ctx.generate_const({cg::type{cg::type_class::i32, 0}}, static_cast<std::int32_t>(i));

        auto expr_value = expr->generate_code(ctx, memory_context::load);
        if(i >= std::numeric_limits<std::int32_t>::max())
        {
            throw cg::codegen_error(loc, fmt::format("Array index exceeds max i32 size ({}).", std::numeric_limits<std::int32_t>::max()));
        }

        ctx.generate_store(std::make_unique<cg::type_argument>(*expr_value), true);

        if(!v)
        {
            v = std::move(expr_value);
        }
        else
        {
            if(v->get_type().to_string() != expr_value->get_type().to_string())
            {
                throw cg::codegen_error(loc, fmt::format("Inconsistent types in array initialization: '{}' and '{}'.", v->get_type().to_string(), expr_value->get_type().to_string()));
            }
        }
    }

    return v;
}

std::optional<ty::type_info> array_initializer_expression::type_check(ty::context& ctx)
{
    std::optional<ty::type_info> t;
    for(auto& it: exprs)
    {
        auto expr_type = it->type_check(ctx);

        if(!expr_type.has_value())
        {
            throw ty::type_error(loc, "Initializer expression has no type.");
        }

        if(t.has_value())
        {
            if(*t != *expr_type)
            {
                throw ty::type_error(loc, fmt::format("Initializer types do not match. Found '{}' and '{}'.", ty::to_string(*t), ty::to_string(*expr_type)));
            }
        }
        else
        {
            t = expr_type;
        }
    }

    if(!t.has_value())
    {
        throw ty::type_error(loc, "Initializer expression has no type.");
    }

    return ctx.get_type(t->to_string(), true);
}

std::string array_initializer_expression::to_string() const
{
    std::string ret = "ArrayInitializer(exprs=(";

    for(std::size_t i = 0; i < exprs.size() - 1; ++i)
    {
        ret += fmt::format("{}, ", exprs[i]->to_string());
    }
    ret += fmt::format("{}))", exprs.back()->to_string());

    return ret;
}

/*
 * struct_definition_expression.
 */

std::unique_ptr<cg::value> struct_definition_expression::generate_code(cg::context& ctx, memory_context mc) const
{
    if(mc != memory_context::none)
    {
        throw cg::codegen_error(loc, fmt::format("Invalid memory context for struct definition."));
    }

    cg::scope* s = ctx.get_scope();    // cannot return nullptr.
    if(!ctx.is_global_scope(s))
    {
        throw cg::codegen_error(loc, fmt::format("Cannot declare type outside global scope."));
    }

    std::vector<std::pair<std::string, cg::value>> struct_members;
    for(auto& m: members)
    {
        if(ty::is_builtin_type(m->get_type()->get_name().s))
        {
            cg::value v = {cg::type{
                             cg::to_type_class(m->get_type()->get_name().s),
                             m->is_array() ? static_cast<std::size_t>(1) : static_cast<std::size_t>(0),
                             m->get_namespace_path()},
                           m->get_name().s};

            struct_members.emplace_back(std::make_pair(m->get_name().s, std::move(v)));
        }
        else
        {
            cg::value v = {cg::type{
                             cg::type_class::struct_,
                             m->is_array() ? static_cast<std::size_t>(1) : static_cast<std::size_t>(0),
                             m->get_type()->get_name().s,
                             m->get_namespace_path()},
                           m->get_name().s};

            struct_members.emplace_back(std::make_pair(m->get_name().s, std::move(v)));
        }
    }

    std::uint8_t flags = static_cast<std::uint8_t>(module_::struct_flags::none);

    std::optional<directive> d = get_unique_directive("allow_cast");
    if(d.has_value())
    {
        if(struct_members.size() != 0)
        {
            throw cg::codegen_error(
              loc,
              fmt::format(
                "Types marked with 'allow_cast' cannot have members. Found {} member{} in type '{}'.",
                struct_members.size(), struct_members.size() > 1 ? "s" : "", name.s));
        }

        flags |= static_cast<std::uint8_t>(module_::struct_flags::allow_cast);
    }

    d = get_unique_directive("native");
    if(d.has_value())
    {
        flags |= static_cast<std::uint8_t>(module_::struct_flags::native);
    }

    s->add_struct(name.s, struct_members, flags);    // FIXME do we need this?
    ctx.add_struct(name.s, std::move(struct_members), flags);

    return nullptr;
}

void struct_definition_expression::collect_names([[maybe_unused]] cg::context& ctx, ty::context& type_ctx) const
{
    std::vector<std::pair<token, ty::type_info>> struct_members;
    for(auto& m: members)
    {
        struct_members.emplace_back(
          std::make_pair(m->get_name(),
                         type_ctx.get_unresolved_type(m->get_type()->get_name(),
                                                      m->get_type()->is_array() ? ty::type_class::tc_array : ty::type_class::tc_plain,
                                                      m->get_type()->get_namespace_path())));
    }
    type_ctx.add_struct(name, std::move(struct_members));
}

bool struct_definition_expression::supports_directive(const std::string& name) const
{
    if(name == "allow_cast")
    {
        return true;
    }
    else if(name == "native")
    {
        return true;
    }

    return false;
}

std::optional<ty::type_info> struct_definition_expression::type_check(ty::context& ctx)
{
    ctx.enter_function_scope(name);
    for(auto& m: members)
    {
        m->type_check(ctx);
    }
    ctx.exit_named_scope(name);

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

std::unique_ptr<cg::value> struct_anonymous_initializer_expression::generate_code(cg::context& ctx, memory_context mc) const
{
    if(mc == memory_context::store)
    {
        throw cg::codegen_error(loc, "Invalid memory context for struct initializer.");
    }

    cg::scope* s = ctx.get_global_scope();    // cannot return nullptr.

    cg::type struct_type{cg::type_class::struct_, 0, name.s, get_namespace_path()};
    const std::vector<std::pair<std::string, cg::value>>* t{nullptr};

    if(s->contains_struct(name.s, get_namespace_path()))
    {
        t = &s->get_struct(name.s, get_namespace_path());
        ctx.generate_new(struct_type);
    }
    else
    {
        auto type = ctx.get_type(name.s, get_namespace_path());
        struct_type = {cg::type_class::struct_, 0, type->get_name(), type->get_import_path()};

        t = &ctx.get_type(name.s, get_namespace_path())->get_members();
        ctx.generate_new(struct_type);
    }

    for(std::size_t i = 0; i < t->size(); ++i)
    {
        const auto& [member_name, member_type] = (*t)[i];
        const auto& initializer = initializers[i];

        ctx.generate_dup(cg::value{struct_type});

        auto initializer_value = initializer->generate_code(ctx, memory_context::load);
        if(!initializer_value)
        {
            throw cg::codegen_error(loc,
                                    fmt::format("Code generation for '{}.{}' initialization returned no type.",
                                                name.s, member_name));
        }
        if(!initializer_value->get_type().is_null()
           && initializer_value->get_type().to_string() != member_type.get_type().to_string())
        {
            throw cg::codegen_error(loc,
                                    fmt::format("Code generation for '{}.{}' initialization returned '{}' (expected '{}').",
                                                name.s, member_name, initializer_value->get_type().to_string(), member_type.get_type().to_string()));
        }

        ctx.generate_set_field(std::make_unique<cg::field_access_argument>(struct_type, member_type));
    }

    return std::make_unique<cg::value>(cg::type{cg::type_class::struct_, 0, name.s});
}

std::optional<ty::type_info> struct_anonymous_initializer_expression::type_check(ty::context& ctx)
{
    auto struct_def = ctx.get_struct_definition(name.location, name.s, get_namespace_path());

    if(initializers.size() != struct_def->members.size())
    {
        throw ty::type_error(name.location,
                             fmt::format("Struct '{}' has {} members, but {} are initialized.",
                                         name.s, struct_def->members.size(), initializers.size()));
    }

    for(std::size_t i = 0; i < initializers.size(); ++i)
    {
        const auto& initializer = initializers[i];
        const auto& struct_member = struct_def->members[i];

        auto initializer_type = initializer->type_check(ctx);
        if(!initializer_type.has_value())
        {
            throw ty::type_error(name.location,
                                 fmt::format("Initializer expression for struct member '{}.{}' has no type.",
                                             name.s, struct_member.first.s));
        }

        // Either the types match, or the type is a reference types which is set to 'null'.
        if(struct_member.second != initializer_type
           && !(ctx.is_reference_type(struct_member.second) && initializer_type == ctx.get_type("@null", false)))
        {
            throw ty::type_error(name.location,
                                 fmt::format("Struct member '{}.{}' has type '{}', but initializer has type '{}'.",
                                             name.s, struct_member.first.s,
                                             struct_member.second.to_string(), initializer_type->to_string()));
        }
    }

    return ctx.get_type(name.s, false, get_namespace_path());
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

std::unique_ptr<cg::value> struct_named_initializer_expression::generate_code(cg::context& ctx, memory_context mc) const
{
    if(mc == memory_context::store)
    {
        throw cg::codegen_error(loc, "Invalid memory context for struct initializer.");
    }

    cg::scope* s = ctx.get_global_scope();    // cannot return nullptr.

    cg::type struct_type{cg::type_class::struct_, 0, name.s, get_namespace_path()};
    const std::vector<std::pair<std::string, cg::value>>* t{nullptr};

    if(s->contains_struct(name.s, get_namespace_path()))
    {
        t = &s->get_struct(name.s, get_namespace_path());
        ctx.generate_new(struct_type);
    }
    else
    {
        auto type = ctx.get_type(name.s, get_namespace_path());
        struct_type = {cg::type_class::struct_, 0, type->get_name(), type->get_import_path()};

        t = &ctx.get_type(name.s, get_namespace_path())->get_members();
        ctx.generate_new(struct_type);
    }

    for(std::size_t i = 0; i < member_names.size(); ++i)
    {
        const auto& initializer = initializers[i];
        const auto& member_name_expr = member_names[i];

        if(!member_name_expr->is_named_expression())
        {
            throw ty::type_error(member_name_expr->get_location(),
                                 fmt::format("Struct members cannot be initialized using <unnamed-expression>."));
        }
        auto member_name = member_name_expr->as_named_expression()->get_name().s;

        auto it = std::find_if(t->cbegin(), t->cend(),
                               [&member_name](const auto& m) -> bool
                               { return m.first == member_name; });
        if(it == t->cend())
        {
            throw ty::type_error(name.location,
                                 fmt::format("Struct '{}' has no member '{}'.", name.s, member_name));
        }

        const auto& member_type = it->second;

        ctx.generate_dup(cg::value{struct_type});

        auto initializer_value = initializer->generate_code(ctx, memory_context::load);
        if(!initializer_value)
        {
            throw cg::codegen_error(loc,
                                    fmt::format("Code generation for '{}.{}' initialization returned no type.",
                                                name.s, member_name));
        }
        if(!initializer_value->get_type().is_null()
           && initializer_value->get_type().to_string() != member_type.get_type().to_string())
        {
            throw cg::codegen_error(loc,
                                    fmt::format("Code generation for '{}.{}' initialization returned '{}' (expected '{}').",
                                                name.s, member_name, initializer_value->get_type().to_string(), member_type.get_type().to_string()));
        }

        ctx.generate_set_field(std::make_unique<cg::field_access_argument>(struct_type, member_type));
    }

    return std::make_unique<cg::value>(cg::type{cg::type_class::struct_, 0, name.s});
}

std::optional<ty::type_info> struct_named_initializer_expression::type_check(ty::context& ctx)
{
    auto struct_def = ctx.get_struct_definition(name.location, name.s, get_namespace_path());

    if(member_names.size() != struct_def->members.size())
    {
        throw ty::type_error(name.location, fmt::format("Struct '{}' has {} members, but {} are initialized.", name.s, struct_def->members.size(), member_names.size()));
    }

    std::vector<std::string> initialized_member_names;
    for(std::size_t i = 0; i < member_names.size(); ++i)
    {
        const auto& member_name_expr = member_names[i];
        const auto& initializer = initializers[i];

        if(!member_name_expr->is_named_expression())
        {
            throw ty::type_error(member_name_expr->get_location(),
                                 fmt::format("Struct members cannot be initialized using <unnamed-expression>."));
        }
        auto member_name = member_name_expr->as_named_expression()->get_name().s;

        if(std::find_if(initialized_member_names.begin(), initialized_member_names.end(),
                        [&member_name](auto& name) -> bool
                        { return name == member_name; })
           != initialized_member_names.end())
        {
            throw ty::type_error(name.location,
                                 fmt::format("Multiple initializations of struct member '{}::{}'.",
                                             name.s, member_name));
        }
        initialized_member_names.push_back(member_name);

        if(member_name_expr->is_array_element_access())    // this is an array access.
        {
            throw ty::type_error(name.location, fmt::format("Cannot access array elements in struct initializer."));
        }

        auto it = std::find_if(struct_def->members.begin(), struct_def->members.end(),
                               [&member_name](const auto& m) -> bool
                               { return m.first.s == member_name; });
        if(it == struct_def->members.end())
        {
            throw ty::type_error(name.location, fmt::format("Struct '{}' has no member '{}'.", name.s, member_name));
        }

        auto initializer_type = initializer->type_check(ctx);
        if(!initializer_type.has_value())
        {
            throw ty::type_error(name.location,
                                 fmt::format("Initializer expression for struct member '{}.{}' has no type.",
                                             name.s, member_name));
        }

        // Either the types match, or the type is a reference types which is set to 'null'.
        if(it->second != initializer_type
           && !(ctx.is_reference_type(it->second) && initializer_type == ctx.get_type("@null", false)))
        {
            throw ty::type_error(name.location,
                                 fmt::format("Struct member '{}.{}' has type '{}', but initializer has type '{}'.",
                                             name.s, member_name,
                                             it->second.to_string(), initializer_type->to_string()));
        }
    }

    return ctx.get_type(name.s, false, get_namespace_path());
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
                ret += fmt::format("name={}, expr={}, ", member_names[i]->to_string(), initializers[i]->to_string());
            }
            ret += fmt::format("name={}, expr={}", member_names.back()->to_string(), initializers.back()->to_string());
        }
        ret += ")";
    }
    return ret;
}

/*
 * binary_expression.
 */

/**
 * Classify a binary operator. If the operator is a compound assignment, the given operator
 * is reduced to its non-assignment form (and left unchanged otherwise).
 *
 * @param s The binary operator.
 * @returns Returns a tuple `(is_assignment, is_compound, is_comparison, reduced_op)`.
 */
static std::tuple<bool, bool, bool, std::string> classify_binary_op(const std::string& s)
{
    bool is_assignment = (s == "=" || s == "+=" || s == "-="
                          || s == "*=" || s == "/=" || s == "%="
                          || s == "&=" || s == "|=" || s == "<<=" || s == ">>=");
    bool is_compound = is_assignment && (s != "=");
    bool is_comparison = (s == "==" || s == "!=" || s == ">" || s == ">=" || s == "<" || s == "<=");

    std::string reduced_op = s;
    if(is_compound)
    {
        reduced_op.pop_back();
    }

    return {is_assignment, is_compound, is_comparison, reduced_op};
}

static const std::unordered_map<std::string, cg::binary_op> binary_op_map = {
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

bool binary_expression::needs_pop() const
{
    auto [is_assignment, is_compound, is_comparison, reduced_op] = classify_binary_op(op.s);
    return !is_assignment;
}

std::unique_ptr<cg::value> binary_expression::generate_code(cg::context& ctx, memory_context mc) const
{
    /*
     * Code generation for binary expressions
     * --------------------------------------
     *
     * We need to distinguish the following types:
     * 1. Assignments
     *    a. Assignments to variables
     *    b. Assignments to array entries
     *    c. Assignments to struct members
     * 2. Non-assignments
     *
     * Further, an assignment can be a compound assignment, which is composed
     * of a non-assignment binary expression and an assignment.
     *
     * Generated IR
     * ------------
     *
     * 1. Compound assignment to variables
     *
     *    v := <l.h.s. load>
     *    <r.h.s. load>
     *    <binary-op>
     *    <store into v>
     *
     * 2. Compound assignment to array entries
     *
     *    v := <l.h.s. load>
     *    <r.h.s. load>
     *    <binary-op>
     *    <store-element into v>
     *
     * 3. Compound assignment to struct members
     *
     *    v := <l.h.s. object load>
     *    <r.h.s. load>
     *    <binary-op>
     *    <set-field v>
     *
     * 4. Assignment to variables
     *
     *    <r.h.s. load>
     *    <store into l.h.s.>
     *
     * 5. Assignment to array entries
     *
     *    <r.h.s load>
     *    <store-element into v>
     *
     * 6. Assignment to struct members
     *
     *    v := <l.h.s. object load>
     *    <r.h.s. load>
     *    <set-field v>
     *
     * 7. Non-assigning binary operation
     *
     *    <l.h.s. load>
     *    <r.h.s. load>
     *    <binary-op>
     */

    std::unique_ptr<cg::value> lhs_value, lhs_store_value, rhs_value;
    auto [is_assignment, is_compound, is_comparison, reduced_op] = classify_binary_op(op.s);

    if(!is_assignment && mc == memory_context::store)
    {
        throw cg::codegen_error("Invalid memory context for assignment (value needs to be writable).");
    }

    /* Cases 2., 3., 5., 6. */
    if((lhs->is_struct_member_access() || lhs->is_array_element_access())
       && (is_assignment || is_compound))
    {
        // memory_context::store will only generate the object load.
        // set_field is generated below.
        lhs_store_value = lhs->generate_code(ctx, memory_context::store);
    }

    /* Cases 1., 2. (cont.), 3. (cont.), 7. */
    if(!is_assignment || is_compound)
    {
        lhs_value = lhs->generate_code(ctx, memory_context::load);
        rhs_value = rhs->generate_code(ctx, memory_context::load);

        if(lhs_value->get_type() != rhs_value->get_type()
           && !(is_comparison && lhs_value->get_type().is_reference() && rhs_value->get_type().is_null()))
        {
            throw cg::codegen_error(loc,
                                    fmt::format("Types don't match in binary operation. L.h.s.: {}, R.h.s.: {}.",
                                                lhs_value->get_type().to_string(),
                                                rhs_value->get_type().to_string()));
        }

        auto it = binary_op_map.find(reduced_op);
        if(it == binary_op_map.end())
        {
            throw std::runtime_error(fmt::format("{}: Code generation for binary operator '{}' not implemented.", slang::to_string(loc), op.s));
        }

        ctx.generate_binary_op(it->second, *lhs_value);

        /* Case 7. */
        if(is_comparison)
        {
            // comparisons are non-compound, so this must be a non-assignment operation.
            return std::make_unique<cg::value>(cg::type{cg::type_class::i32, 0});
        }
        else if(!is_assignment)
        {
            // non-assignment operation.
            return lhs_value;
        }
    }

    /*
     * assignments: Cases 1.-3. (cont.), 4., 5.-6. (cont.)
     */

    /* Cases 4.-5. (cont.) */
    if(!rhs_value)
    {
        rhs_value = rhs->generate_code(ctx, memory_context::load);
    }

    /* Cases 3. (cont.), 6. (cont.) */
    if(lhs->is_struct_member_access())
    {
        // duplicate the value for chained assignments.
        if(mc == memory_context::load)
        {
            ctx.generate_dup(*rhs_value, {cg::value{cg::type{cg::type_class::addr, 0}}});
        }

        // by the above check, lhs is an access_expression.
        access_expression* ae_lhs = lhs->as_access_expression();
        ty::type_info struct_type_info = ae_lhs->get_struct_type();
        cg::type struct_type{
          cg::type_class::struct_,
          0,
          struct_type_info.to_string(),
          struct_type_info.get_import_path()};    // FIXME get as cg::type directly?

        ctx.generate_set_field(std::make_unique<cg::field_access_argument>(struct_type, *lhs_store_value));
        return rhs_value;
    }
    /* Cases 2. (cont.), 5. (cont.) */
    else if(lhs->is_array_element_access())
    {
        // duplicate the value for chained assignments.
        if(mc == memory_context::load)
        {
            ctx.generate_dup(*rhs_value,
                             {cg::value{cg::type{cg::type_class::i32, 0}},
                              cg::value{cg::type{cg::type_class::addr, 0}}});
        }

        ctx.generate_store(
          std::make_unique<cg::variable_argument>(
            std::make_unique<cg::value>(*rhs_value)),
          true);

        return rhs_value;
    }
    /* Case 1. (cont.), 4. (cont.) */
    else
    {
        // we might need to duplicate the value for chained assignments.
        if(mc == memory_context::load)
        {
            ctx.generate_dup(*rhs_value);
        }

        return lhs->generate_code(ctx, memory_context::store);
    }
}

std::optional<ty::type_info> binary_expression::type_check(ty::context& ctx)
{
    auto [is_assignment, is_compound, is_comparison, reduced_op] = classify_binary_op(op.s);

    auto lhs_type = lhs->type_check(ctx);

    const ty::struct_definition* struct_def = nullptr;
    if(op.s == ".")    // scope access
    {
        struct_def = ctx.get_struct_definition(loc, lhs_type->to_string(), lhs_type->get_import_path());
        ctx.push_struct_definition(struct_def);
    }

    auto rhs_type = rhs->type_check(ctx);

    if(struct_def)
    {
        ctx.pop_struct_definition();
        return rhs_type;
    }

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
        if(ty::to_string(*lhs_type) != "i32" || ty::to_string(*rhs_type) != "i32")
        {
            throw ty::type_error(loc, fmt::format("Got binary expression of type '{}' {} '{}', expected 'i32' {} 'i32'.", ty::to_string(*lhs_type), reduced_op, ty::to_string(*rhs_type), reduced_op));
        }

        // return the restricted type.
        return ctx.get_type("i32", false);
    }

    // assignments and comparisons.
    if(op.s == "=" || op.s == "==" || op.s == "!=")
    {
        // Either the types match, or the type is a reference types which is set to 'null'.
        if(*lhs_type != *rhs_type
           && !(ctx.is_reference_type(*lhs_type) && *rhs_type == ctx.get_type("@null", false)))
        {
            throw ty::type_error(loc, fmt::format("Types don't match in binary expression. Got expression of type '{}' {} '{}'.", ty::to_string(*lhs_type), reduced_op, ty::to_string(*rhs_type)));
        }

        if(op.s == "=")
        {
            // assignments return the type of the l.h.s.
            return lhs_type;
        }

        // comparisons return i32.
        return ctx.get_type("i32", false);
    }

    // check lhs and rhs have supported types (i32 and f32 at the moment).
    if(*lhs_type != ctx.get_type("i32", false) && *lhs_type != ctx.get_type("f32", false))
    {
        throw ty::type_error(loc, fmt::format("Expected 'i32' or 'f32' for l.h.s. of binary operation of type '{}', got '{}'.", reduced_op, ty::to_string(*lhs_type)));
    }
    if(*rhs_type != ctx.get_type("i32", false) && *rhs_type != ctx.get_type("f32", false))
    {
        throw ty::type_error(loc, fmt::format("Expected 'i32' or 'f32' for r.h.s. of binary operation of type '{}', got '{}'.", reduced_op, ty::to_string(*rhs_type)));
    }

    if(*lhs_type != *rhs_type)
    {
        throw ty::type_error(loc, fmt::format("Types don't match in binary expression. Got expression of type '{}' {} '{}'.", ty::to_string(*lhs_type), reduced_op, ty::to_string(*rhs_type)));
    }

    // comparisons return i32.
    if(is_comparison)
    {
        return ctx.get_type("i32", false);
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
 * unary_expression.
 */

std::unique_ptr<cg::value> unary_expression::generate_code(cg::context& ctx, memory_context mc) const
{
    if(mc == memory_context::store)
    {
        throw cg::codegen_error(loc, "Cannot store into unary expression.");
    }

    if(op.s == "++")
    {
        auto v = operand->generate_code(ctx, mc);
        if(v->get_type().get_type_class() != cg::type_class::i32
           && v->get_type().get_type_class() != cg::type_class::f32)
        {
            throw cg::codegen_error(loc, fmt::format("Wrong expression type '{}' for prefix operator '++'. Expected 'i32' or 'f32'.", v->get_type().to_string()));
        }

        if(v->get_type().get_type_class() == cg::type_class::i32)
        {
            ctx.generate_const(*v, 1);
        }
        else if(v->get_type().get_type_class() == cg::type_class::f32)
        {
            ctx.generate_const(*v, 1.f);
        }
        ctx.generate_binary_op(cg::binary_op::op_add, *v);

        ctx.generate_dup(*v);
        ctx.generate_store(std::make_unique<cg::variable_argument>(std::make_unique<cg::value>(*v)));
        return v;
    }
    else if(op.s == "--")
    {
        auto v = operand->generate_code(ctx, mc);
        if(v->get_type().to_string() != "i32" && v->get_type().to_string() != "f32")
        {
            throw cg::codegen_error(loc, fmt::format("Wrong expression type '{}' for prefix operator '--'. Expected 'i32' or 'f32'.", v->get_type().to_string()));
        }

        if(v->get_type().get_type_class() == cg::type_class::i32)
        {
            ctx.generate_const(*v, 1);
        }
        else if(v->get_type().get_type_class() == cg::type_class::f32)
        {
            ctx.generate_const(*v, 1.f);
        }
        ctx.generate_binary_op(cg::binary_op::op_sub, *v);

        ctx.generate_dup(*v);
        ctx.generate_store(std::make_unique<cg::variable_argument>(std::make_unique<cg::value>(*v)));
        return v;
    }
    else if(op.s == "+")
    {
        return operand->generate_code(ctx, mc);
    }
    else if(op.s == "-")
    {
        auto& instrs = ctx.get_insertion_point()->get_instructions();
        std::size_t pos = instrs.size();
        auto v = operand->generate_code(ctx, mc);

        std::vector<std::unique_ptr<cg::argument>> args;
        if(v->get_type().get_type_class() == cg::type_class::i32)
        {
            args.emplace_back(std::make_unique<cg::const_argument>(0));
        }
        else if(v->get_type().get_type_class() == cg::type_class::f32)
        {
            args.emplace_back(std::make_unique<cg::const_argument>(0.f));
        }
        else
        {
            throw cg::codegen_error(loc, fmt::format("Type error for unary operator '-': Expected 'i32' or 'f32', got '{}'.", v->get_type().to_string()));
        }
        instrs.insert(instrs.begin() + pos, std::make_unique<cg::instruction>("const", std::move(args)));

        ctx.generate_binary_op(cg::binary_op::op_sub, *v);
        return v;
    }
    else if(op.s == "!")
    {
        auto v = operand->generate_code(ctx, memory_context::load);
        if(v->get_type().get_type_class() != cg::type_class::i32)
        {
            throw cg::codegen_error(loc, fmt::format("Type error for unary operator '!': Expected 'i32', got '{}'.", v->get_type().to_string()));
        }

        ctx.generate_const({cg::type{cg::type_class::i32, 0}}, 0);
        ctx.generate_binary_op(cg::binary_op::op_equal, *v);

        return std::make_unique<cg::value>(cg::type{cg::type_class::i32, 0});
    }
    else if(op.s == "~")
    {
        auto& instrs = ctx.get_insertion_point()->get_instructions();
        std::size_t pos = instrs.size();

        auto v = operand->generate_code(ctx, mc);
        if(v->get_type().get_type_class() != cg::type_class::i32)
        {
            throw cg::codegen_error(loc, fmt::format("Type error for unary operator '~': Expected 'i32', got '{}'.", v->get_type().to_string()));
        }

        std::vector<std::unique_ptr<cg::argument>> args;
        args.emplace_back(std::make_unique<cg::const_argument>(~0));

        instrs.insert(instrs.begin() + pos, std::make_unique<cg::instruction>("const", std::move(args)));

        ctx.generate_binary_op(cg::binary_op::op_xor, *v);
        return v;
    }

    throw std::runtime_error(fmt::format("{}: Code generation for unary operator '{}' not implemented.", slang::to_string(loc), op.s));
}

std::optional<ty::type_info> unary_expression::type_check(ty::context& ctx)
{
    static const std::unordered_map<std::string, std::vector<std::string>> valid_operand_types = {
      {"++", {"i32", "f32"}},
      {"--", {"i32", "f32"}},
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

    auto type_it = std::find(op_it->second.begin(), op_it->second.end(), ty::to_string(*operand_type));
    if(type_it == op_it->second.end())
    {
        throw ty::type_error(operand->get_location(), fmt::format("Invalid operand type '{}' for unary operator '{}'.", ty::to_string(*operand_type), op.s));
    }

    return ctx.get_type(*type_it, false);
}

std::string unary_expression::to_string() const
{
    return fmt::format("Unary(op=\"{}\", operand={})", op.s, operand ? operand->to_string() : std::string("<none>"));
}

/*
 * new_expression.
 */

std::unique_ptr<cg::value> new_expression::generate_code(cg::context& ctx, memory_context mc) const
{
    if(mc == memory_context::store)
    {
        throw cg::codegen_error(loc, "Cannot store into new expression.");
    }

    if(type.s == "void")
    {
        throw cg::codegen_error(loc, "Cannot create array with entries of type 'void'.");
    }
    else if(!ty::is_builtin_type(type.s))
    {
        throw cg::codegen_error(loc, fmt::format("Cannot create array with entries of non-builtin type '{}'.", type.s));
    }

    std::unique_ptr<cg::value> v = expr->generate_code(ctx, memory_context::load);
    if(v->get_type().get_type_class() != cg::type_class::i32)
    {
        throw cg::codegen_error(loc, fmt::format("Expected <integer> as array size, got '{}'.", v->get_type().to_string()));
    }

    ctx.generate_newarray(cg::value{cg::type{cg::to_type_class(type.s), 0}});

    return std::make_unique<cg::value>(cg::type{cg::to_type_class(type.s), 1});
}

std::optional<ty::type_info> new_expression::type_check(ty::context& ctx)
{
    auto expr_type = expr->type_check(ctx);
    if(expr_type == std::nullopt)
    {
        throw ty::type_error(expr->get_location(), "Array size has no type.");
    }
    if(ty::to_string(*expr_type) != "i32")
    {
        throw ty::type_error(expr->get_location(), fmt::format("Expected array size of type 'i32', got '{}'.", ty::to_string(*expr_type)));
    }

    if(!ty::is_builtin_type(type.s))
    {
        throw ty::type_error(type.location, fmt::format("Invalid type '{}' for new expression.", type.s));
    }

    if(type.s == "void")
    {
        throw ty::type_error(type.location, "Cannot use operator new with type 'void'.");
    }

    return ctx.get_type(type.s, true);
}

std::string new_expression::to_string() const
{
    return fmt::format("NewExpression(type={}, expr={})", type.s, expr->to_string());
}

/*
 * null_expression.
 */

std::unique_ptr<cg::value> null_expression::generate_code(cg::context& ctx, memory_context mc) const
{
    if(mc == memory_context::store)
    {
        throw cg::codegen_error(loc, "Cannot store into null expression.");
    }

    ctx.generate_const_null();

    return std::make_unique<cg::value>(cg::type{cg::type_class::null, 0});
}

std::optional<ty::type_info> null_expression::type_check(ty::context& ctx)
{
    return ctx.get_type("@null", false);
}

std::string null_expression::to_string() const
{
    return "NullExpression()";
}

/*
 * postfix_expression.
 */

std::unique_ptr<cg::value> postfix_expression::generate_code(cg::context& ctx, memory_context mc) const
{
    if(mc == memory_context::store)
    {
        throw cg::codegen_error(loc, "Cannot store into postfix operator expression.");
    }

    auto v = identifier->generate_code(ctx, memory_context::load);
    if(v->get_type().to_string() != "i32" && v->get_type().to_string() != "f32")
    {
        throw cg::codegen_error(loc, fmt::format("Wrong expression type '{}' for postfix operator. Expected 'i32' or 'f32'.", v->get_type().to_string()));
    }

    if(op.s == "++")
    {
        ctx.generate_dup(*v);    // keep the value on the stack.
        if(v->get_type().get_type_class() == cg::type_class::i32)
        {
            ctx.generate_const(cg::value{cg::type{cg::type_class::i32, 0}}, 1);
        }
        else if(v->get_type().get_type_class() == cg::type_class::f32)
        {
            ctx.generate_const(cg::value{cg::type{cg::type_class::f32, 0}}, 1.f);
        }
        ctx.generate_binary_op(cg::binary_op::op_add, *v);

        ctx.generate_store(std::make_unique<cg::variable_argument>(std::make_unique<cg::value>(*v)));
    }
    else if(op.s == "--")
    {
        ctx.generate_dup(*v);    // keep the value on the stack.
        if(v->get_type().get_type_class() == cg::type_class::i32)
        {
            ctx.generate_const(cg::value{cg::type{cg::type_class::i32, 0}}, 1);
        }
        else if(v->get_type().get_type_class() == cg::type_class::f32)
        {
            ctx.generate_const(cg::value{cg::type{cg::type_class::f32, 0}}, 1.f);
        }
        ctx.generate_binary_op(cg::binary_op::op_sub, *v);

        ctx.generate_store(std::make_unique<cg::variable_argument>(std::make_unique<cg::value>(*v)));
    }
    else
    {
        throw cg::codegen_error(op.location, fmt::format("Unknown postfix operator '{}'.", op.s));
    }

    return v;
}

std::optional<ty::type_info> postfix_expression::type_check(ty::context& ctx)
{
    auto identifier_type = identifier->type_check(ctx);
    if(identifier_type == std::nullopt)
    {
        throw ty::type_error(identifier->get_location(), fmt::format("Cannot evaluate type of expression."));
    }
    auto identifier_type_str = ty::to_string(*identifier_type);
    if(identifier_type_str != "i32" && identifier_type_str != "f32")
    {
        throw ty::type_error(identifier->get_location(), fmt::format("Postfix operator '{}' can only operate on 'i32' or 'f32' (found '{}').", op.s, identifier_type_str));
    }
    return identifier_type;
}

std::string postfix_expression::to_string() const
{
    return fmt::format("Postfix(identifier={}, op=\"{}\")", identifier->to_string(), op.s);
}

/*
 * prototype.
 */

cg::function* prototype_ast::generate_code(cg::context& ctx, memory_context mc) const
{
    if(mc != memory_context::none)
    {
        throw cg::codegen_error(loc, "Invalid memory context for prototype.");
    }

    std::vector<std::unique_ptr<cg::value>> function_args;
    for(auto& a: args)
    {
        if(ty::is_builtin_type(std::get<1>(a)->get_name().s))
        {
            function_args.emplace_back(std::make_unique<cg::value>(
              cg::type{cg::to_type_class(std::get<1>(a)->get_name().s),
                       std::get<1>(a)->is_array()
                         ? static_cast<std::size_t>(1)
                         : static_cast<std::size_t>(0)},
              std::get<0>(a).s));
        }
        else
        {
            function_args.emplace_back(std::make_unique<cg::value>(
              cg::type{cg::type_class::struct_,
                       std::get<1>(a)->is_array()
                         ? static_cast<std::size_t>(1)
                         : static_cast<std::size_t>(0),
                       std::get<1>(a)->get_name().s},
              std::get<0>(a).s));
        }
    }

    std::unique_ptr<cg::value> ret_val = ty::is_builtin_type(return_type->get_name().s)
                                           ? std::make_unique<cg::value>(cg::type{cg::to_type_class(return_type->get_name().s),
                                                                                  return_type->is_array()
                                                                                    ? static_cast<std::size_t>(1)
                                                                                    : static_cast<std::size_t>(0)})
                                           : std::make_unique<cg::value>(cg::type{cg::type_class::struct_,
                                                                                  return_type->is_array()
                                                                                    ? static_cast<std::size_t>(1)
                                                                                    : static_cast<std::size_t>(0),
                                                                                  return_type->get_name().s});

    return ctx.create_function(name.s, std::move(ret_val), std::move(function_args));
}

void prototype_ast::generate_native_binding(const std::string& lib_name, cg::context& ctx) const
{
    std::vector<std::unique_ptr<cg::value>> function_args;
    for(auto& a: args)
    {
        if(ty::is_builtin_type(std::get<1>(a)->get_name().s))
        {
            function_args.emplace_back(
              std::make_unique<cg::value>(
                cg::type{cg::to_type_class(std::get<1>(a)->get_name().s),
                         std::get<1>(a)->is_array()
                           ? static_cast<std::size_t>(1)
                           : static_cast<std::size_t>(0)},
                std::get<0>(a).s));
        }
        else
        {
            function_args.emplace_back(
              std::make_unique<cg::value>(
                cg::type{cg::type_class::struct_,
                         std::get<1>(a)->is_array()
                           ? static_cast<std::size_t>(1)
                           : static_cast<std::size_t>(0),
                         std::get<1>(a)->get_name().s},
                std::get<0>(a).s));
        }
    }

    std::unique_ptr<cg::value> ret;
    if(ty::is_builtin_type(return_type->get_name().s))
    {
        ret = std::make_unique<cg::value>(
          cg::type{cg::to_type_class(return_type->get_name().s),
                   return_type->is_array()
                     ? static_cast<std::size_t>(1)
                     : static_cast<std::size_t>(0)});
    }
    else
    {
        ret = std::make_unique<cg::value>(
          cg::type{cg::type_class::struct_,
                   return_type->is_array()
                     ? static_cast<std::size_t>(1)
                     : static_cast<std::size_t>(0),
                   return_type->get_name().s});
    }

    ctx.create_native_function(lib_name, name.s, std::move(ret), std::move(function_args));
}

void prototype_ast::collect_names(cg::context& ctx, ty::context& type_ctx) const
{
    std::vector<cg::value> prototype_arg_types;
    std::transform(args.cbegin(), args.cend(), std::back_inserter(prototype_arg_types),
                   [](const auto& arg) -> cg::value
                   {
                       if(ty::is_builtin_type(std::get<1>(arg)->get_name().s))
                       {
                           return cg::value{
                             cg::type{cg::to_type_class(std::get<1>(arg)->get_name().s),
                                      std::get<1>(arg)->is_array() ? static_cast<std::size_t>(1) : static_cast<std::size_t>(0)}};
                       }

                       return cg::value{
                         cg::type{cg::type_class::struct_,
                                  std::get<1>(arg)->is_array() ? static_cast<std::size_t>(1) : static_cast<std::size_t>(0),
                                  std::get<1>(arg)->get_name().s}};
                   });

    cg::value ret_val = ty::is_builtin_type(return_type->get_name().s)
                          ? cg::value{
                              cg::type{cg::to_type_class(return_type->get_name().s),
                                       return_type->is_array() ? static_cast<std::size_t>(1) : static_cast<std::size_t>(0)}}
                          : cg::value{cg::type{cg::type_class::struct_, return_type->is_array() ? static_cast<std::size_t>(1) : static_cast<std::size_t>(0), return_type->get_name().s}};

    ctx.add_prototype(name.s, std::move(ret_val), prototype_arg_types);

    std::vector<ty::type_info> arg_types;
    std::transform(args.cbegin(), args.cend(), std::back_inserter(arg_types),
                   [&type_ctx](const std::pair<token, std::unique_ptr<type_expression>>& arg) -> ty::type_info
                   { return type_ctx.get_unresolved_type(
                       std::get<1>(arg)->get_name(),
                       std::get<1>(arg)->is_array() ? ty::type_class::tc_array : ty::type_class::tc_plain,
                       std::get<1>(arg)->get_namespace_path()); });
    type_ctx.add_function(name, std::move(arg_types),
                          type_ctx.get_unresolved_type(
                            return_type->get_name(),
                            return_type->is_array() ? ty::type_class::tc_array : ty::type_class::tc_plain,
                            return_type->get_namespace_path()));
}

void prototype_ast::type_check(ty::context& ctx)
{
    // enter function scope. the scope is exited in the type_check for the function's body.
    ctx.enter_function_scope(name);

    // add the arguments to the current scope.
    for(const auto& arg: args)
    {
        ctx.add_variable(std::get<0>(arg),
                         ctx.get_type(std::get<1>(arg)->get_name(),
                                      std::get<1>(arg)->is_array(),
                                      std::get<1>(arg)->get_namespace_path()));
    }

    // check the return type.
    if(!ty::is_builtin_type(return_type->get_name().s) && !ctx.has_type(return_type->get_name().s))
    {
        throw ty::type_error(return_type->get_location(), fmt::format("Unknown return type '{}'.", return_type->get_name().s));
    }
}

void prototype_ast::finish_type_check(ty::context& ctx)
{
    // exit the function's scope.
    ctx.exit_named_scope(name);
}

std::string prototype_ast::to_string() const
{
    std::string ret_type_str = return_type->to_string();
    std::string ret = fmt::format("Prototype(name={}, return_type={}, args=(", name.s, ret_type_str);
    if(args.size() > 0)
    {
        for(std::size_t i = 0; i < args.size() - 1; ++i)
        {
            std::string arg_type_str = std::get<1>(args[i])->to_string();
            ret += fmt::format("(name={}, type={}), ", std::get<0>(args[i]).s, arg_type_str);
        }
        std::string arg_type_str = std::get<1>(args.back())->to_string();
        ret += fmt::format("(name={}, type={})", std::get<0>(args.back()).s, arg_type_str);
    }
    ret += "))";
    return ret;
}

/*
 * block.
 */

std::unique_ptr<cg::value> block::generate_code(cg::context& ctx, memory_context mc) const
{
    if(mc != memory_context::none)
    {
        throw cg::codegen_error(loc, "Invalid memory context for code block.");
    }

    std::unique_ptr<cg::value> v;
    for(auto& expr: this->exprs)
    {
        v = expr->generate_code(ctx, memory_context::none);

        // non-assigning expressions need cleanup.
        if(expr->needs_pop())
        {
            if(!v)
            {
                throw cg::codegen_error(loc, "Expression requires popping the stack, but didn't produce a value.");
            }

            ctx.generate_pop(*v);
        }
    }
    return nullptr;
}

void block::collect_names(cg::context& ctx, ty::context& type_ctx) const
{
    for(auto& expr: exprs)
    {
        expr->collect_names(ctx, type_ctx);
    }
}

std::optional<ty::type_info> block::type_check(ty::context& ctx)
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

std::unique_ptr<cg::value> function_expression::generate_code(cg::context& ctx, memory_context mc) const
{
    auto d = get_unique_directive("native");
    if(!d.has_value())
    {
        if(mc != memory_context::none)
        {
            throw cg::codegen_error(loc, "Invalid memory context for function_expression.");
        }

        cg::function* fn = prototype->generate_code(ctx);
        cg::function_guard fg{ctx, fn};
        cg::scope_guard sg{ctx, fn->get_scope()};

        cg::basic_block* bb = cg::basic_block::create(ctx, "entry");
        fn->append_basic_block(bb);

        ctx.set_insertion_point(bb);

        if(!body)
        {
            throw cg::codegen_error(loc, fmt::format("No function body defined for '{}'.", prototype->get_name().s));
        }

        auto v = body->generate_code(ctx);

        // verify that the break-continue-stack is empty.
        if(ctx.get_break_continue_stack_size() != 0)
        {
            throw cg::codegen_error(loc, fmt::format("Internal error: Break-continue stack is not empty."));
        }

        auto ip = ctx.get_insertion_point(true);
        if(!ip->ends_with_return() && !ip->is_unreachable())
        {
            // for `void` return types, we insert a return instruction. otherwise, the
            // return statement is missing and we throw an error.
            auto ret_type = std::get<0>(fn->get_signature());
            if(!ret_type.is_void())
            {
                throw cg::codegen_error(loc, fmt::format("Missing return statement in function '{}'.", fn->get_name()));
            }

            ctx.generate_ret(v ? std::make_optional(*v) : std::nullopt);
        }
    }
    else
    {
        auto& directive = *d;
        if(directive.args.size() != 1 || directive.args[0].first.s != "lib")
        {
            throw cg::codegen_error(loc, fmt::format("Native function '{}': Expected argument 'lib' for directive.", prototype->get_name().s));
        }

        if(directive.args[0].second.type != token_type::str_literal && directive.args[0].second.type != token_type::identifier)
        {
            throw cg::codegen_error(loc, "Expected 'lib=<identifier>' or 'lib=<string-literal>'.");
        }

        std::string lib_name =
          (directive.args[0].second.type == token_type::str_literal)
            ? std::get<std::string>(*directive.args[0].second.value)
            : directive.args[0].second.s;

        prototype->generate_native_binding(lib_name, ctx);
    }

    return nullptr;
}

void function_expression::collect_names(cg::context& ctx, ty::context& type_ctx) const
{
    prototype->collect_names(ctx, type_ctx);
}

bool function_expression::supports_directive(const std::string& name) const
{
    if(name == "native")
    {
        return true;
    }

    return false;
}

std::optional<ty::type_info> function_expression::type_check(ty::context& ctx)
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

std::unique_ptr<cg::value> call_expression::generate_code(cg::context& ctx, memory_context mc) const
{
    if(mc == memory_context::store)
    {
        throw cg::codegen_error(loc, "Cannot store into call expression.");
    }

    for(auto& arg: args)
    {
        arg->generate_code(ctx, memory_context::load);
    }
    ctx.generate_invoke(std::make_unique<cg::function_argument>(callee.s, get_namespace_path()));

    auto return_type = ctx.get_prototype(callee.s, get_namespace_path()).get_return_type();
    if(index_expr)
    {
        // evaluate the index expression.
        index_expr->generate_code(ctx, memory_context::load);
        ctx.generate_load(std::make_unique<cg::type_argument>(return_type.deref()), true);
        return std::make_unique<cg::value>(return_type.deref());
    }

    return std::make_unique<cg::value>(std::move(return_type));
}

std::optional<ty::type_info> call_expression::type_check(ty::context& ctx)
{
    auto sig = ctx.get_function_signature(callee, get_namespace_path());
    return_type = sig.ret_type;

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

        if(sig.arg_types[i] != *arg_type
           && !ctx.is_convertible(args[i]->get_location(), *arg_type, sig.arg_types[i]))
        {
            throw ty::type_error(args[i]->get_location(), fmt::format("Type of argument {} does not match signature: Expected '{}', got '{}'.", i + 1, ty::to_string(sig.arg_types[i]), ty::to_string(*arg_type)));
        }
    }

    if(index_expr)
    {
        auto v = index_expr->type_check(ctx);
        if(!v.has_value() || ty::to_string(*v) != "i32")
        {
            throw ty::type_error(loc, "Expected <integer> for array element access.");
        }

        if(!sig.ret_type.is_array())
        {
            throw ty::type_error(loc, "Cannot use subscript on non-array type.");
        }

        return ctx.get_type(sig.ret_type.get_element_type()->to_string(), false);
    }

    return sig.ret_type;
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
        ret += fmt::format("{}", args.back() ? args.back()->to_string() : std::string("<none>"));
    }
    ret += "))";
    return ret;
}

/*
 * return_statement.
 */

std::unique_ptr<cg::value> return_statement::generate_code(cg::context& ctx, memory_context mc) const
{
    if(mc != memory_context::none)
    {
        throw cg::codegen_error(loc, "Invalid memory context for return_statement.");
    }

    if(expr)
    {
        auto v = expr->generate_code(ctx, memory_context::load);
        if(!v)
        {
            throw cg::codegen_error(loc, "Expression did not yield a type.");
        }
        ctx.generate_ret(*v);
    }
    else
    {
        ctx.generate_ret();
    }

    return nullptr;
}

std::optional<ty::type_info> return_statement::type_check(ty::context& ctx)
{
    auto sig = ctx.get_current_function();
    if(sig == std::nullopt)
    {
        throw ty::type_error(loc, "Cannot have return statement outside a function.");
    }

    if(ty::to_string(sig->ret_type) == "void")
    {
        if(expr)
        {
            throw ty::type_error(loc, fmt::format("Function '{}' declared as having no return value cannot have a return expression.", sig->name.s));
        }
    }
    else
    {
        auto ret_type = expr->type_check(ctx);

        if(ret_type.has_value() && !ret_type.value().is_resolved())
        {
            throw ty::type_error(loc, fmt::format("Function '{}': Unresolved return type '{}'.", sig->name.s, ty::to_string(*ret_type)));
        }

        if(ret_type == std::nullopt)
        {
            throw ty::type_error(loc, fmt::format("Function '{}': Return expression has no type, expected '{}'.", sig->name.s, ty::to_string(sig->ret_type)));
        }
        if(*ret_type != sig->ret_type)
        {
            throw ty::type_error(loc, fmt::format("Function '{}': Return expression has type '{}', expected '{}'.", sig->name.s, ty::to_string(*ret_type), ty::to_string(sig->ret_type)));
        }
    }

    return sig->ret_type;
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

std::unique_ptr<cg::value> if_statement::generate_code(cg::context& ctx, memory_context mc) const
{
    if(mc != memory_context::none)
    {
        throw cg::codegen_error(loc, "Invalid memory context for if_statement.");
    }

    auto v = condition->generate_code(ctx, memory_context::load);
    if(!v)
    {
        throw cg::codegen_error(loc, "Condition did not yield a type.");
    }
    if(v->get_type().to_string() != "i32")
    {
        throw cg::codegen_error(loc, fmt::format("Expected if condition to be of type 'i32', got '{}", v->get_type().to_string()));
    }

    // store where to insert the branch.
    auto* function_insertion_point = ctx.get_insertion_point(true);

    // set up basic blocks.
    auto if_basic_block = cg::basic_block::create(ctx, ctx.generate_label());
    cg::basic_block* else_basic_block = nullptr;
    auto merge_basic_block = cg::basic_block::create(ctx, ctx.generate_label());

    bool if_ends_with_return = false, else_ends_with_return = false;

    // code generation for if block.
    ctx.get_current_function(true)->append_basic_block(if_basic_block);
    ctx.set_insertion_point(if_basic_block);
    if_block->generate_code(ctx, memory_context::none);
    if_ends_with_return = if_basic_block->ends_with_return();
    ctx.generate_branch(merge_basic_block);

    // code generation for optional else block.
    if(!else_block)
    {
        ctx.set_insertion_point(function_insertion_point);
        ctx.generate_cond_branch(if_basic_block, merge_basic_block);
    }
    else
    {
        else_basic_block = cg::basic_block::create(ctx, ctx.generate_label());
        ctx.get_current_function(true)->append_basic_block(else_basic_block);
        ctx.set_insertion_point(else_basic_block);
        else_block->generate_code(ctx, memory_context::none);
        else_ends_with_return = else_basic_block->ends_with_return();
        ctx.generate_branch(merge_basic_block);

        ctx.set_insertion_point(function_insertion_point);
        ctx.generate_cond_branch(if_basic_block, else_basic_block);
    }

    // emit merge block.
    ctx.get_current_function(true)->append_basic_block(merge_basic_block);
    ctx.set_insertion_point(merge_basic_block);

    // check if the merge block is reachable.
    if(if_ends_with_return && else_ends_with_return)
    {
        merge_basic_block->set_unreachable();
    }

    return nullptr;
}

std::optional<ty::type_info> if_statement::type_check(ty::context& ctx)
{
    auto condition_type = condition->type_check(ctx);
    if(condition_type == std::nullopt)
    {
        throw ty::type_error(loc, "If condition has no type.");
    }

    if(ty::to_string(*condition_type) != "i32")
    {
        throw ty::type_error(loc, fmt::format("Expected if condition to be of type 'i32', got '{}", ty::to_string(*condition_type)));
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

std::unique_ptr<cg::value> while_statement::generate_code(cg::context& ctx, memory_context mc) const
{
    if(mc != memory_context::none)
    {
        throw cg::codegen_error(loc, "Invalid memory context for while_statement.");
    }

    // set up basic blocks.
    auto while_loop_header_basic_block = cg::basic_block::create(ctx, ctx.generate_label());
    auto while_loop_basic_block = cg::basic_block::create(ctx, ctx.generate_label());
    auto merge_basic_block = cg::basic_block::create(ctx, ctx.generate_label());

    // while loop header.
    ctx.get_current_function(true)->append_basic_block(while_loop_header_basic_block);
    ctx.set_insertion_point(while_loop_header_basic_block);

    ctx.push_break_continue({merge_basic_block, while_loop_header_basic_block});

    auto v = condition->generate_code(ctx, memory_context::load);
    if(!v)
    {
        throw cg::codegen_error(loc, "Condition did not yield a type.");
    }
    if(v->get_type().to_string() != "i32")
    {
        throw cg::codegen_error(loc, fmt::format("Expected while condition to be of type 'i32', got '{}'.", v->get_type().to_string()));
    }

    ctx.generate_cond_branch(while_loop_basic_block, merge_basic_block);

    // while loop body.
    ctx.get_current_function(true)->append_basic_block(while_loop_basic_block);
    ctx.set_insertion_point(while_loop_basic_block);
    while_block->generate_code(ctx, memory_context::none);

    ctx.set_insertion_point(ctx.get_current_function(true)->get_basic_blocks().back());
    ctx.generate_branch(while_loop_header_basic_block);

    ctx.pop_break_continue(loc);

    // emit merge block.
    ctx.get_current_function(true)->append_basic_block(merge_basic_block);
    ctx.set_insertion_point(merge_basic_block);

    return nullptr;
}

std::optional<ty::type_info> while_statement::type_check(ty::context& ctx)
{
    auto condition_type = condition->type_check(ctx);
    if(condition_type == std::nullopt)
    {
        throw ty::type_error(loc, "While condition has no type.");
    }

    if(ty::to_string(*condition_type) != "i32")
    {
        throw ty::type_error(loc, fmt::format("Expected while condition to be of type 'i32', got '{}", ty::to_string(*condition_type)));
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

std::unique_ptr<cg::value> break_statement::generate_code(cg::context& ctx, [[maybe_unused]] memory_context mc) const
{
    auto [break_block, continue_block] = ctx.top_break_continue(loc);
    ctx.generate_branch(break_block);
    return nullptr;
}

/*
 * continue_statement.
 */

std::unique_ptr<cg::value> continue_statement::generate_code(cg::context& ctx, [[maybe_unused]] memory_context mc) const
{
    auto [break_block, continue_block] = ctx.top_break_continue(loc);
    ctx.generate_branch(continue_block);
    return nullptr;
}

}    // namespace slang::ast