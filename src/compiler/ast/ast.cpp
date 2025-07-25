/**
 * slang - a simple scripting language.
 *
 * abstract syntax tree.
 *
 * \author Felix Lubbe
 * \copyright Copyright (c) 2025
 * \license Distributed under the MIT software license (see accompanying LICENSE.txt).
 */

#include <print>
#include <ranges>
#include <set>
#include <stack>

#include <gsl/gsl>

#include "compiler/codegen.h"
#include "compiler/typing.h"
#include "shared/module.h"
#include "shared/type_utils.h"
#include "ast.h"
#include "node_registry.h"
#include "utils.h"

namespace cg = slang::codegen;
namespace ty = slang::typing;

namespace slang::ast
{

/**
 * Create a `cg::value` from a `type_info`.
 *
 * @param ti The type information.
 * @param name An optional name for the value.
 * @return A unique pointer to a `cg::value`.
 */
static std::unique_ptr<cg::value> type_info_to_value(
  const ty::type_info& ti,
  std::optional<std::string> name = std::nullopt)
{
    std::string base_type = ti.is_array() ? ti.get_element_type()->to_string()
                                          : ti.to_string();

    if(ty::is_builtin_type(base_type))
    {
        return std::make_unique<cg::value>(
          cg::type{cg::to_type_class(base_type),
                   ti.is_array()
                     ? static_cast<std::size_t>(1)
                     : static_cast<std::size_t>(0)},
          std::move(name));
    }

    return std::make_unique<cg::value>(
      cg::type{cg::type_class::struct_,
               ti.is_array()
                 ? static_cast<std::size_t>(1)
                 : static_cast<std::size_t>(0),
               std::move(base_type),
               ti.get_import_path()},
      std::move(name));
}

/*
 * expression.
 */

std::unique_ptr<expression> expression::clone() const
{
    return std::make_unique<expression>(*this);
}

void expression::serialize(archive& ar)
{
    ar & loc;
    ar & namespace_stack;
    ar & expr_type;
}

call_expression* expression::as_call_expression()
{
    throw std::runtime_error("Expression is not a call expression.");
}

macro_expression* expression::as_macro_expression()
{
    throw std::runtime_error("Expression is not a macro.");
}

macro_branch* expression::as_macro_branch()
{
    throw std::runtime_error("Expression is not a macro branch.");
}

const macro_branch* expression::as_macro_branch() const
{
    throw std::runtime_error("Expression is not a macro branch.");
}

macro_expression_list* expression::as_macro_expression_list()
{
    throw std::runtime_error("Expression is not a macro expression list.");
}

const macro_expression_list* expression::as_macro_expression_list() const
{
    throw std::runtime_error("Expression is not a macro expression list.");
}

variable_declaration_expression* expression::as_variable_declaration()
{
    throw std::runtime_error("Expression is not a variable declaration.");
}

variable_reference_expression* expression::as_variable_reference()
{
    throw std::runtime_error("Expression is not a variable reference.");
}

macro_invocation* expression::as_macro_invocation()
{
    throw std::runtime_error("Expression is not a macro invocation.");
}

literal_expression* expression::as_literal()
{
    throw std::runtime_error("Expression is not a literal.");
}

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

std::unique_ptr<cg::value> expression::evaluate([[maybe_unused]] cg::context& ctx) const
{
    return {};
}

std::unique_ptr<cg::value> expression::generate_code(
  [[maybe_unused]] cg::context& ctx,
  [[maybe_unused]] memory_context mc) const
{
    throw std::runtime_error(
      std::format(
        "{}: Expression does not generate code.",
        ::slang::to_string(loc)));
}

void expression::push_directive(
  cg::context& ctx,
  const token& name,
  const std::vector<std::pair<token, token>>& args)
{
    if(!supports_directive(name.s))
    {
        auto transform = [](const std::pair<token, token>& arg) -> std::string
        { 
            if(arg.second.s.empty())
            {
                return arg.first.s;
            }
            return std::format("{}={}", arg.first.s, arg.second.s); };

        auto arg_string = slang::utils::join(args, {transform}, ", ");

        // FIXME This should print the source instead of the AST.
        constexpr std::size_t MAX_SUBSTRING_LENGTH = 80;    // FIXME arbitrary limit.

        auto expr_string = to_string();
        if(expr_string.empty())
        {
            expr_string = "<unknown>";
        }
        else if(expr_string.size() > MAX_SUBSTRING_LENGTH)
        {
            expr_string = expr_string.substr(0, MAX_SUBSTRING_LENGTH) + "...";
        }

        throw ty::type_error(
          name.location,
          std::format(
            "Directive '{}'{} is not supported by the expression with AST '{}'.",
            name.s,
            !arg_string.empty()
              ? std::format(" with arguments '{}'", arg_string)
              : std::string{},
            expr_string));
    }

    ctx.push_directive({name, args});
}

void expression::pop_directive(cg::context& ctx)
{
    ctx.pop_directive();
}

bool expression::supports_directive(const std::string& name) const
{
    return name == "disable";
}

std::string expression::to_string() const
{
    throw std::runtime_error("Expression has no string conversion");
}

/**
 * Templated visit helper. Implements DFS to walk the AST.
 * The visitor function is called for each node in the AST.
 * The nodes are visited in pre-order or post-order.
 *
 * @param expr The expression to visit.
 * @param visitor The visitor function.
 * @param visit_self Whether to visit the expression itself.
 * @param post_order Whether to visit the nodes in post-order.
 * @param filter An optional filter that returns `true` if a node
 *               should be traversed. Defaults to traversing all nodes.
 * @tparam T The expression type. Must be a subclass of `expression`.
 */
template<typename T>
    requires(std::is_same_v<std::decay_t<T>, expression>
             || std::is_base_of_v<expression, std::decay_t<T>>)
void visit_nodes(
  T& expr,
  std::function<void(T&)>& visitor,
  bool visit_self,
  bool post_order,
  const std::function<bool(std::add_const_t<T>&)>& filter = nullptr)
{
    // use DFS to topologically sort the AST.
    std::stack<T*> stack;
    stack.push(&expr);

    std::deque<T*> sorted_ast;

    while(!stack.empty())
    {
        T* current = stack.top();
        stack.pop();

        if(current == nullptr)
        {
            throw std::runtime_error("Null expression in AST.");
        }

        if(!filter || filter(*current))
        {
            sorted_ast.emplace_back(current);

            for(auto* child: current->get_children())
            {
                stack.push(child);
            }
        }
    }

    if(!visit_self)
    {
        sorted_ast.pop_front();
    }

    if(post_order)
    {
        for(auto it = sorted_ast.rbegin(); it != sorted_ast.rend(); ++it)
        {
            visitor(**it);
        }
    }
    else
    {
        for(auto* it: sorted_ast)
        {
            visitor(*it);
        }
    }
}

void expression::visit_nodes(
  std::function<void(expression&)> visitor,
  bool visit_self,
  bool post_order,
  const std::function<bool(const expression&)>& filter)
{
    slang::ast::visit_nodes(*this, visitor, visit_self, post_order, filter);
}

void expression::visit_nodes(
  std::function<void(const expression&)> visitor,
  bool visit_self,
  bool post_order,
  const std::function<bool(const expression&)>& filter) const
{
    slang::ast::visit_nodes(*this, visitor, visit_self, post_order, filter);
}

/*
 * named_expression.
 */

std::unique_ptr<expression> named_expression::clone() const
{
    return std::make_unique<named_expression>(*this);
}

void named_expression::serialize(archive& ar)
{
    super::serialize(ar);
    ar & name;
}

/*
 * literal_expression.
 */

std::unique_ptr<expression> literal_expression::clone() const
{
    return std::make_unique<literal_expression>(*this);
}

void literal_expression::serialize(archive& ar)
{
    super::serialize(ar);
    ar & tok;
}

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
            throw cg::codegen_error(
              loc,
              std::format(
                "Cannot store into int_literal '{}'.",
                std::get<int>(*tok.value)));
        }

        if(tok.type == token_type::fp_literal)
        {
            throw cg::codegen_error(
              loc,
              std::format(
                "Cannot store into fp_literal '{}'.",
                std::get<float>(*tok.value)));
        }

        if(tok.type == token_type::str_literal)
        {
            throw cg::codegen_error(
              loc,
              std::format(
                "Cannot store into str_literal '{}'.",
                std::get<std::string>(*tok.value)));
        }

        throw cg::codegen_error(
          loc,
          std::format(
            "Cannot store into unknown literal of type id '{}'.",
            static_cast<int>(tok.type)));
    }

    if(tok.type == token_type::int_literal)
    {
        ctx.generate_const({cg::type{cg::type_class::i32, 0}}, *tok.value);
        return std::make_unique<cg::value>(cg::type{cg::type_class::i32, 0});
    }

    if(tok.type == token_type::fp_literal)
    {
        ctx.generate_const({cg::type{cg::type_class::f32, 0}}, *tok.value);
        return std::make_unique<cg::value>(cg::type{cg::type_class::f32, 0});
    }

    if(tok.type == token_type::str_literal)
    {
        ctx.generate_const({cg::type{cg::type_class::str, 0}}, *tok.value);
        return std::make_unique<cg::value>(cg::type{cg::type_class::str, 0});
    }

    throw cg::codegen_error(
      loc,
      std::format(
        "Unable to generate code for literal of type id '{}'.",
        static_cast<int>(tok.type)));
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
        ctx.set_expression_type(this, *expr_type);
    }
    else if(tok.type == token_type::fp_literal)
    {
        expr_type = ctx.get_type("f32", false);
        ctx.set_expression_type(this, *expr_type);
    }
    else if(tok.type == token_type::str_literal)
    {
        expr_type = ctx.get_type("str", false);
        ctx.set_expression_type(this, *expr_type);
    }
    else
    {
        throw ty::type_error(
          tok.location,
          std::format(
            "Unknown literal type with id '{}'.",
            static_cast<int>(tok.type)));
    }

    return expr_type;
}

std::string literal_expression::to_string() const
{
    if(tok.type == token_type::fp_literal)
    {
        if(tok.value)
        {
            return std::format("FloatLiteral(value={})", std::get<float>(*tok.value));
        }

        return "FloatLiteral(<none>)";
    }

    if(tok.type == token_type::int_literal)
    {
        if(tok.value)
        {
            return std::format("IntLiteral(value={})", std::get<int>(*tok.value));
        }

        return "IntLiteral(<none>)";
    }

    if(tok.type == token_type::str_literal)
    {
        if(tok.value)
        {
            return std::format("StrLiteral(value=\"{}\")", std::get<std::string>(*tok.value));
        }

        return "StrLiteral(<none>)";
    }

    return "UnknownLiteral";
}

/*
 * type_expression.
 */

std::unique_ptr<type_expression> type_expression::clone() const
{
    return std::make_unique<type_expression>(*this);
}

void type_expression::serialize(archive& ar)
{
    ar & loc;
    ar & type_name;
    ar & namespace_stack;
    ar & array;
}

/*
 * type_cast_expression.
 */

std::unique_ptr<expression> type_cast_expression::clone() const
{
    return std::make_unique<type_cast_expression>(*this);
}

void type_cast_expression::serialize(archive& ar)
{
    super::serialize(ar);
    ar& expression_serializer{expr};
    ar & target_type;
}

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
            throw cg::codegen_error(
              loc,
              std::format(
                "Cannot cast '{}' to 'str'.",
                v->get_type().to_string()));
        }
        else
        {
            auto cast_target_type = target_type->to_type();
            if(target_type->get_name().s == "str")
            {
                return std::make_unique<cg::value>(std::move(cast_target_type));
            }

            // casts between non-builtin types are checked at run-time.
            ctx.generate_checkcast(cast_target_type);

            return std::make_unique<cg::value>(std::move(cast_target_type));
        }

        return std::make_unique<cg::value>(cg::type{cg::to_type_class(target_type->get_name().s), 0});
    }

    // identity transformation.
    if(ty::is_builtin_type(target_type->to_string()))
    {
        return std::make_unique<cg::value>(cg::type{cg::to_type_class(target_type->get_name().s), 0});
    }

    return std::make_unique<cg::value>(cg::type{cg::type_class::struct_, 0, target_type->get_name().s});
}

std::optional<ty::type_info> type_cast_expression::type_check(ty::context& ctx)
{
    auto type = expr->type_check(ctx);
    if(!type.has_value())
    {
        throw ty::type_error(loc, "Type cast expression has no type.");
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
            throw ty::type_error(
              loc,
              std::format(
                "Invalid cast to non-primitive type '{}'.",
                target_type->get_name().s));
        }
    }

    // casts for struct types. this is checked at run-time.
    expr_type = target_type->to_type_info(ctx);    // no array casts.
    ctx.set_expression_type(this, *expr_type);

    return expr_type;
}

std::string type_cast_expression::to_string() const
{
    return std::format(
      "TypeCast(target_type={}, expr={})",
      target_type->to_string(),
      expr ? expr->to_string() : std::string("<none>"));
}

/*
 * namespace_access_expression.
 */

std::unique_ptr<expression> namespace_access_expression::clone() const
{
    return std::make_unique<namespace_access_expression>(*this);
}

void namespace_access_expression::serialize(archive& ar)
{
    super::serialize(ar);
    ar & name;
    ar& expression_serializer{expr};
}

std::unique_ptr<cg::value> namespace_access_expression::generate_code(cg::context& ctx, memory_context mc) const
{
    // NOTE update_namespace is (intentionally) not const, so we cannot use it here.
    auto expr_namespace_stack = namespace_stack;
    expr_namespace_stack.push_back(name.s);
    expr->set_namespace(std::move(expr_namespace_stack));
    return expr->generate_code(ctx, mc);
}

std::optional<ty::type_info> namespace_access_expression::type_check(ty::context& ctx)
{
    update_namespace();

    expr_type = expr->type_check(ctx);
    if(!expr_type.has_value())
    {
        throw std::runtime_error("Type check: Expression has no type in namespace access.");
    }
    ctx.set_expression_type(this, expr_type.value());
    return expr_type;
}

std::string namespace_access_expression::to_string() const
{
    return std::format(
      "Scope(name={}, expr={})",
      name.s,
      expr ? expr->to_string() : std::string("<none>"));
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

std::unique_ptr<expression> access_expression::clone() const
{
    return std::make_unique<access_expression>(*this);
}

void access_expression::serialize(archive& ar)
{
    super::serialize(ar);
    ar& expression_serializer{lhs};
    ar& expression_serializer{rhs};
    ar & lhs_type;
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
            throw cg::codegen_error(
              loc,
              std::format(
                "Could not find name for element access in array access expression."));
        }
        auto* identifier_expr = rhs->as_named_expression();

        if(identifier_expr->get_name().s == "length")
        {
            if(mc == memory_context::store)
            {
                throw cg::codegen_error(rhs->get_location(), "Array length is read only.");
            }

            ctx.generate_arraylength();
            return std::make_unique<cg::value>(cg::type{cg::type_class::i32, 0});
        }

        throw cg::codegen_error(
          rhs->get_location(),
          std::format(
            "Unknown array property '{}'.",
            identifier_expr->get_name().s));
    }

    /*
     * structs.
     */
    cg::type lhs_value_type = lhs_value->get_type();

    // validate expression.
    if(lhs_value_type.is_array())
    {
        throw cg::codegen_error(
          loc,
          std::format(
            "Cannot load array '{}' as an object.",
            lhs_value->get_name().value_or("<none>")));
    }

    if(!lhs_value_type.is_struct())
    {
        throw cg::codegen_error(
          loc,
          std::format(
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
    auto type = lhs->type_check(ctx);
    if(!type.has_value())
    {
        throw ty::type_error(loc, "Access expression has no l.h.s.");
    }
    lhs_type = *type;

    const ty::struct_definition* struct_def =
      lhs_type.is_array()
        ? ctx.get_struct_definition(
            lhs_type.get_location(),
            "@array")    // array built-ins.
        : ctx.get_struct_definition(
            lhs_type.get_location(),
            ty::to_string(lhs_type),
            lhs_type.get_import_path());

    ctx.push_struct_definition(struct_def);
    expr_type = rhs->type_check(ctx);
    ctx.pop_struct_definition();

    if(!expr_type.has_value())
    {
        throw std::runtime_error("Type check: Expression has no type in member access.");
    }
    ctx.set_expression_type(this, expr_type.value());
    return expr_type;
}

std::string access_expression::to_string() const
{
    return std::format("Access(lhs={}, rhs={})", lhs->to_string(), rhs->to_string());
}

/*
 * import_expression.
 */

std::unique_ptr<expression> import_expression::clone() const
{
    return std::make_unique<import_expression>(*this);
}

void import_expression::serialize(archive& ar)
{
    super::serialize(ar);
    ar & path;
}

std::unique_ptr<cg::value> import_expression::generate_code([[maybe_unused]] cg::context& ctx, [[maybe_unused]] memory_context mc) const
{
    // import expressions are handled by the import resolver.
    return nullptr;
}

void import_expression::collect_names([[maybe_unused]] cg::context& ctx, ty::context& type_ctx) const
{
    type_ctx.add_import(path, false);
}

std::string import_expression::to_string() const
{
    auto transform = [](const token& p) -> std::string
    { return p.s; };

    return std::format("Import(path={})", slang::utils::join(path, {transform}, "."));
}

/*
 * directive_expression.
 */

std::unique_ptr<expression> directive_expression::clone() const
{
    return std::make_unique<directive_expression>(*this);
}

void directive_expression::serialize(archive& ar)
{
    super::serialize(ar);
    ar & args;
    ar& expression_serializer{expr};
}

std::unique_ptr<cg::value> directive_expression::generate_code(cg::context& ctx, memory_context mc) const
{
    expr->push_directive(ctx, name, args);
    std::unique_ptr<cg::value> ret = expr->generate_code(ctx, mc);
    expr->pop_directive(ctx);
    return ret;
}

void directive_expression::collect_names(cg::context& ctx, ty::context& type_ctx) const
{
    expr->push_directive(ctx, name, args);
    expr->collect_names(ctx, type_ctx);
    expr->pop_directive(ctx);
}

std::optional<ty::type_info> directive_expression::type_check(ty::context& ctx)
{
    expr_type = expr->type_check(ctx);
    if(expr_type.has_value())
    {
        ctx.set_expression_type(this, *expr_type);
    }
    return expr_type;
}

std::string directive_expression::to_string() const
{
    auto transform = [](const std::pair<token, token>& p) -> std::string
    { return std::format("{}, {}", p.first.s, p.second.s); };

    return std::format(
      "Directive(name={}, args=({}), expr={})",
      name.s,
      slang::utils::join(args, {transform}, ","),
      expr->to_string());
}

/*
 * variable_reference_expression.
 */

std::unique_ptr<expression> variable_reference_expression::clone() const
{
    return std::make_unique<variable_reference_expression>(*this);
}

void variable_reference_expression::serialize(archive& ar)
{
    super::serialize(ar);
    ar& expression_serializer{element_expr};
    ar& expression_serializer{expansion};
}

std::unique_ptr<cg::value> variable_reference_expression::generate_code(cg::context& ctx, memory_context mc) const
{
    // check for macro expansions first.
    if(expansion)
    {
        return expansion->generate_code(ctx, mc);
    }

    // First check for variables, then for constants.
    std::optional<cg::value> v = get_value(ctx);
    if(!v.has_value())
    {
        // check if we're loading a constant.

        std::optional<std::string> import_path = get_namespace_path();
        std::optional<cg::constant_table_entry> const_v = ctx.get_constant(name.s, import_path);
        if(!const_v.has_value())
        {
            throw cg::codegen_error(
              loc,
              std::format(
                "Cannot find variable or constant '{}{}' in current scope.",
                import_path.has_value()
                  ? std::format("{}::", *import_path)
                  : "",
                name.s));
        }

        if(mc != memory_context::load)
        {
            throw cg::codegen_error(
              loc,
              std::format(
                "Cannot assign to constant '{}{}'.",
                import_path.has_value()
                  ? std::format("{}::", *import_path)
                  : "",
                name.s));
        }

        // load the constant directly.

        if(const_v->type == module_::constant_type::i32)
        {
            ctx.generate_const({cg::type{cg::type_class::i32, 0}}, const_v->data);
            return std::make_unique<cg::value>(cg::type{cg::type_class::i32, 0});
        }

        if(const_v->type == module_::constant_type::f32)
        {
            ctx.generate_const({cg::type{cg::type_class::f32, 0}}, const_v->data);
            return std::make_unique<cg::value>(cg::type{cg::type_class::f32, 0});
        }

        if(const_v->type == module_::constant_type::str)
        {
            ctx.generate_const({cg::type{cg::type_class::str, 0}}, const_v->data);
            return std::make_unique<cg::value>(cg::type{cg::type_class::str, 0});
        }

        throw cg::codegen_error(
          loc,
          std::format(
            "Cannot load constant '{}{}' of unknown type {}.",
            import_path.has_value()
              ? std::format("{}::", *import_path)
              : "",
            name.s,
            static_cast<int>(const_v->type)));
    }

    if(mc != memory_context::store)
    {
        if(ctx.is_struct_access())
        {
            if(!v.value().get_name().has_value())
            {
                throw cg::codegen_error(
                  loc,
                  std::format("Cannot access struct: No member name."));
            }

            auto struct_type = ctx.get_accessed_struct();
            auto member_value = ctx.get_struct_member(
              loc,
              struct_type.to_string(),
              v.value().get_name().value(),    // NOLINT(bugprone-unchecked-optional-access)
              struct_type.get_import_path());

            ctx.generate_get_field(std::make_unique<cg::field_access_argument>(struct_type, member_value));
        }
        else
        {
            ctx.generate_load(std::make_unique<cg::variable_argument>(std::make_unique<cg::value>(v.value())));
        }

        if(element_expr)
        {
            element_expr->generate_code(ctx, memory_context::load);
            ctx.generate_load(std::make_unique<cg::type_argument>(v.value().get_type().deref()), true);
        }
    }
    else
    {
        if(element_expr)
        {
            ctx.generate_load(std::make_unique<cg::variable_argument>(std::make_unique<cg::value>(v.value())));
            element_expr->generate_code(ctx, memory_context::load);

            // if we're storing an element, generation of the store opcode needs to be deferred to the caller.
        }
        else
        {
            ctx.generate_store(std::make_unique<cg::variable_argument>(std::make_unique<cg::value>(v.value())));
        }
    }

    if(element_expr)
    {
        return std::make_unique<cg::value>(v.value().deref());
    }
    return std::make_unique<cg::value>(v.value());
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

        ty::type_info t;
        if(expansion)
        {
            bool scope_type_check = expansion->is_macro_branch();
            if(scope_type_check)
            {
                ctx.enter_anonymous_scope(get_location());
            }
            std::optional<ty::type_info> opt_t = expansion->type_check(ctx);
            if(scope_type_check)
            {
                ctx.exit_anonymous_scope();
            }

            if(!opt_t.has_value())
            {
                throw ty::type_error(loc, "Expression has no type.");
            }
            t = opt_t.value();
        }
        else
        {
            t = ctx.get_identifier_type(name, get_namespace_path());
        }

        if(!t.is_array())
        {
            throw ty::type_error(loc, "Cannot use subscript on non-array type.");
        }

        auto* base_type = t.get_element_type();
        expr_type = ctx.get_type(
          base_type->to_string(),
          base_type->is_array(),
          base_type->get_import_path());
        ctx.set_expression_type(this, *expr_type);
    }
    else
    {
        if(expansion)
        {
            bool scope_type_check = expansion->is_macro_branch();
            if(scope_type_check)
            {
                ctx.enter_anonymous_scope(get_location());
            }
            expr_type = expansion->type_check(ctx);
            if(scope_type_check)
            {
                ctx.exit_anonymous_scope();
            }

            if(expr_type.has_value())
            {
                ctx.set_expression_type(this, *expr_type);
            }
        }
        else
        {
            expr_type = ctx.get_identifier_type(name, get_namespace_path());
            ctx.set_expression_type(this, *expr_type);
        }
    }

    return expr_type;
}

std::string variable_reference_expression::to_string() const
{
    std::string ret = std::format("VariableReference(name={}", name.s);

    if(element_expr)
    {
        ret += std::format(
          ", element_expr={}",
          element_expr->to_string());
    }
    if(expansion)
    {
        ret += std::format(
          ", expansion={}",
          expansion->to_string());
    }
    ret += ")";

    return ret;
}

std::optional<cg::value> variable_reference_expression::get_value(cg::context& ctx) const
{
    if(!ctx.is_struct_access())
    {
        auto* scope = ctx.get_scope();
        if(scope == nullptr)
        {
            throw cg::codegen_error(
              loc,
              std::format("No scope to search for '{}'.", name.s));
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

        // name not in scope.
        return std::nullopt;
    }

    auto struct_type = ctx.get_accessed_struct();
    if(!struct_type.get_struct_name().has_value())
    {
        throw cg::codegen_error(
          loc,
          std::format("Cannot access struct: No struct name."));
    }

    return ctx.get_struct_member(
      loc,
      struct_type.get_struct_name().value(),    // NOLINT(bugprone-unchecked-optional-access)
      get_name().s,
      struct_type.get_import_path());
}

/*
 * type_expression.
 */

std::string type_expression::get_qualified_name() const
{
    std::optional<std::string> namespace_path = get_namespace_path();
    if(namespace_path.has_value())
    {
        return std::format("{}::{}", namespace_path.value(), type_name.s);
    }
    return type_name.s;
}

std::optional<std::string> type_expression::get_namespace_path() const
{
    if(namespace_stack.empty())
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
    if(!namespace_stack.empty())
    {
        for(std::size_t i = 0; i < namespace_stack.size() - 1; ++i)
        {
            namespace_string += std::format("{}, ", namespace_stack[i].s);
        }
        namespace_string += namespace_stack.back().s;
    }

    return std::format(
      "TypeExpression(name={}, namespaces=({}), array={})",
      get_name().s,
      namespace_string,
      array);
}

cg::type type_expression::to_type() const
{
    if(ty::is_builtin_type(type_name.s))
    {
        if(!namespace_stack.empty())
        {
            throw cg::codegen_error(
              loc,
              std::format(
                "Type '{}' cannot occur in a namespace.",
                type_name.s));
        }

        return cg::type{cg::to_type_class(type_name.s), array ? static_cast<std::size_t>(1) : static_cast<std::size_t>(0)};
    }

    return cg::type{
      cg::type_class::struct_,
      array ? static_cast<std::size_t>(1) : static_cast<std::size_t>(0),    // FIXME Change if more array dimensions need to be supported.
      type_name.s,
      get_namespace_path()};
}

ty::type_info type_expression::to_type_info(ty::context& ctx) const
{
    return ctx.get_type(type_name.s, array, get_namespace_path());
}

ty::type_info type_expression::to_unresolved_type_info(ty::context& ctx) const
{
    return ctx.get_unresolved_type(
      type_name,
      array ? ty::type_class::tc_array : ty::type_class::tc_plain,
      get_namespace_path());
}

/*
 * variable_declaration_expression.
 */

std::unique_ptr<expression> variable_declaration_expression::clone() const
{
    return std::make_unique<variable_declaration_expression>(*this);
}

void variable_declaration_expression::serialize(archive& ar)
{
    super::serialize(ar);
    ar & type;
    ar& expression_serializer{expr};
}

std::unique_ptr<cg::value> variable_declaration_expression::generate_code(cg::context& ctx, memory_context mc) const
{
    if(mc != memory_context::none)
    {
        throw cg::codegen_error(
          loc,
          std::format(
            "Invalid memory context for variable declaration."));
    }

    cg::scope* s = ctx.get_scope();
    if(s == nullptr)
    {
        throw cg::codegen_error(
          loc,
          std::format(
            "No scope available for adding locals."));
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
          type->to_type(),
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
    // Prevent double declaration of variable.
    if(ctx.has_expression_type(*this))
    {
        return ctx.get_expression_type(*this);
    }

    auto var_type = type->to_type_info(ctx);
    ctx.add_variable(name, var_type);

    if(expr)
    {
        auto rhs = expr->type_check(ctx);
        if(!rhs)
        {
            throw ty::type_error(name.location, std::format("Expression has no type."));
        }

        // Either the types match, or the type is a reference type which is set to 'null'.
        if(*rhs != var_type
           && !(ctx.is_reference_type(var_type) && *rhs == ctx.get_type("@null", false)))
        {
            throw ty::type_error(
              name.location,
              std::format(
                "R.h.s. has type '{}' (type id {}), which does not match the variable's type '{}' (type id {}).",
                ty::to_string(*rhs),
                rhs->get_type_id(),
                ty::to_string(var_type),
                var_type.get_type_id()));
        }
    }

    expr_type = std::make_optional<ty::type_info>(
      name, ty::type_class::tc_declaration, std::nullopt);
    ctx.set_expression_type(this, expr_type.value());

    return expr_type;
}

std::string variable_declaration_expression::to_string() const
{
    return std::format(
      "VariableDeclaration(name={}, type={}, expr={})",
      name.s,
      type->to_string(),
      expr ? expr->to_string() : std::string("<none>"));
}

/*
 * constant_declaration_expression.
 */

std::unique_ptr<expression> constant_declaration_expression::clone() const
{
    return std::make_unique<constant_declaration_expression>(*this);
}

void constant_declaration_expression::serialize(archive& ar)
{
    super::serialize(ar);
    ar & type;
    ar& expression_serializer{expr};
}

void constant_declaration_expression::push_directive(
  cg::context& ctx,
  const token& name,
  const std::vector<std::pair<token, token>>& args)
{
    // verify that 'const_eval' is not disabled.
    if(ctx.get_directive_flag("disable", "const_eval", false))
    {
        throw cg::codegen_error(loc, "Cannot declare constant with 'const_eval' disabled.");
    }

    super::push_directive(ctx, name, args);
}

std::unique_ptr<cg::value> constant_declaration_expression::generate_code(cg::context& ctx, [[maybe_unused]] memory_context mc) const
{
    if(!expr->is_const_eval(ctx))
    {
        throw cg::codegen_error(
          expr->get_location(),
          "Expression in constant declaration is not compile-time computable.");
    }

    auto v = expr->evaluate(ctx);
    if(!v)
    {
        throw cg::codegen_error(
          expr->get_location(),
          "Expression in constant declaration is not compile-time computable.");
    }

    auto t = v->get_type();

    if(t.is_array())
    {
        throw cg::codegen_error(
          expr->get_location(),
          "Compile-time assignment not supported for arrays.");
    }
    if(t.is_reference())
    {
        throw cg::codegen_error(
          expr->get_location(),
          "Compile-time assignment not supported for references.");
    }

    if(t.to_string() == "i32")
    {
        auto* v_i32 = static_cast<cg::constant_int*>(v.get());    // NOLINT(cppcoreguidelines-pro-type-static-cast-downcast)
        ctx.add_constant(name.s, v_i32->get_int());
        return std::make_unique<cg::value>(cg::type{cg::type_class::i32, 0});
    }

    if(t.to_string() == "f32")
    {
        auto* v_f32 = static_cast<cg::constant_float*>(v.get());    // NOLINT(cppcoreguidelines-pro-type-static-cast-downcast)
        ctx.add_constant(name.s, v_f32->get_float());
        return std::make_unique<cg::value>(cg::type{cg::type_class::f32, 0});
    }

    if(t.to_string() == "str")
    {
        auto* v_str = static_cast<cg::constant_str*>(v.get());    // NOLINT(cppcoreguidelines-pro-type-static-cast-downcast)
        ctx.add_constant(name.s, v_str->get_str());
        return std::make_unique<cg::value>(cg::type{cg::type_class::str, 0});
    }

    throw cg::codegen_error(
      expr->get_location(),
      std::format(
        "Invalid token type '{}' for constant declaration.",
        t.to_string()));
}

void constant_declaration_expression::collect_names(cg::context& ctx, ty::context& type_ctx) const
{
    auto const_type = type->to_type_info(type_ctx);
    type_ctx.add_variable(name, const_type);    // FIXME for the typing context, constants and variables are the same right now.
    ctx.register_constant_name(name);
}

std::optional<ty::type_info> constant_declaration_expression::type_check(ty::context& ctx)
{
    // Prevent double declaration of constant.
    if(ctx.has_expression_type(*this))
    {
        return ctx.get_expression_type(*this);
    }

    auto const_type = type->to_type_info(ctx);
    auto rhs = expr->type_check(ctx);
    if(!rhs)
    {
        throw ty::type_error(
          name.location,
          "Expression has no type.");
    }

    // Either the types match, or the type is a reference type which is set to 'null'.
    if(*rhs != const_type
       && !(ctx.is_reference_type(const_type) && *rhs == ctx.get_type("@null", false)))
    {
        throw ty::type_error(
          name.location,
          std::format(
            "R.h.s. has type '{}' (type id {}), which does not match the constant's type '{}' (type id {}).",
            ty::to_string(*rhs),
            rhs->get_type_id(),
            ty::to_string(const_type),
            const_type.get_type_id()));
    }

    return std::nullopt;
}

std::string constant_declaration_expression::to_string() const
{
    return std::format(
      "Constant(name={}, type={}, expr={})",
      name.s,
      type->to_string(),
      expr->to_string());
}

/*
 * array_initializer_expression.
 */

std::unique_ptr<expression> array_initializer_expression::clone() const
{
    return std::make_unique<array_initializer_expression>(*this);
}

void array_initializer_expression::serialize(archive& ar)
{
    super::serialize(ar);
    ar& expression_vector_serializer{exprs};
}

std::unique_ptr<cg::value> array_initializer_expression::generate_code(cg::context& ctx, [[maybe_unused]] memory_context mc) const
{
    std::unique_ptr<cg::value> v;
    auto array_type = ctx.get_array_type();

    if(exprs.size() >= std::numeric_limits<std::int32_t>::max())
    {
        throw cg::codegen_error(
          std::format(
            "Cannot generate code for array initializer list: list size exceeds numeric limits ({} >= {}).",
            exprs.size(),
            std::numeric_limits<std::int32_t>::max()));
    }

    ctx.generate_const({cg::type{cg::type_class::i32, 0}}, static_cast<int>(exprs.size()));
    ctx.generate_newarray(array_type.deref());

    for(std::size_t i = 0; i < exprs.size(); ++i)
    {
        const auto& expr = exprs[i];

        // the top of the stack contains the array address.
        ctx.generate_dup(array_type);
        ctx.generate_const({cg::type{cg::type_class::i32, 0}}, static_cast<std::int32_t>(i));

        auto expr_value = expr->generate_code(ctx, memory_context::load);
        if(i >= std::numeric_limits<std::int32_t>::max())
        {
            throw cg::codegen_error(
              loc,
              std::format(
                "Array index exceeds max i32 size ({}).",
                std::numeric_limits<std::int32_t>::max()));
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
                throw cg::codegen_error(
                  loc,
                  std::format(
                    "Inconsistent types in array initialization: '{}' and '{}'.",
                    v->get_type().to_string(),
                    expr_value->get_type().to_string()));
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
                throw ty::type_error(
                  loc,
                  std::format(
                    "Initializer types do not match. Found '{}' and '{}'.",
                    ty::to_string(*t),
                    ty::to_string(*expr_type)));
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

    expr_type = ctx.get_type(t->to_string(), true);
    ctx.set_expression_type(this, *expr_type);

    return expr_type;
}

std::string array_initializer_expression::to_string() const
{
    std::string ret = "ArrayInitializer(exprs=(";

    for(std::size_t i = 0; i < exprs.size() - 1; ++i)
    {
        ret += std::format("{}, ", exprs[i]->to_string());
    }
    ret += std::format("{}))", exprs.back()->to_string());

    return ret;
}

/*
 * struct_definition_expression.
 */

std::unique_ptr<expression> struct_definition_expression::clone() const
{
    return std::make_unique<struct_definition_expression>(*this);
}

void struct_definition_expression::serialize(archive& ar)
{
    super::serialize(ar);
    ar& expression_vector_serializer{members};
}

std::unique_ptr<cg::value> struct_definition_expression::generate_code(cg::context& ctx, memory_context mc) const
{
    if(mc != memory_context::none)
    {
        throw cg::codegen_error(
          loc,
          "Invalid memory context for struct definition.");
    }

    cg::scope* s = ctx.get_scope();    // cannot return nullptr.
    if(!ctx.is_global_scope(s))
    {
        throw cg::codegen_error(
          loc,
          "Cannot declare type outside global scope.");
    }

    std::vector<std::pair<std::string, cg::value>> struct_members =
      members
      | std::views::transform(
        [](const auto& m)
        {
            return std::make_pair(
              m->get_name().s,
              cg::value{
                m->get_type()->to_type(),
                m->get_name().s});
        })
      | std::ranges::to<std::vector>();

    auto flags = static_cast<std::uint8_t>(module_::struct_flags::none);
    if(ctx.get_last_directive("allow_cast").has_value())
    {
        flags |= static_cast<std::uint8_t>(module_::struct_flags::allow_cast);
    }
    if(ctx.get_last_directive("native").has_value())
    {
        flags |= static_cast<std::uint8_t>(module_::struct_flags::native);
    }

    s->add_struct(name.s, struct_members, flags);    // FIXME do we need this?
    ctx.add_struct(name.s, std::move(struct_members), flags);

    return nullptr;
}

void struct_definition_expression::collect_names([[maybe_unused]] cg::context& ctx, ty::context& type_ctx) const
{
    std::vector<std::pair<token, ty::type_info>> struct_members =
      members
      | std::views::transform(
        [&type_ctx](const auto& m)
        {
            return std::make_pair(
              m->get_name(),
              m->get_type()->to_unresolved_type_info(type_ctx));
        })
      | std::ranges::to<std::vector>();

    type_ctx.add_struct(name, std::move(struct_members));
}

bool struct_definition_expression::supports_directive(const std::string& name) const
{
    return super::supports_directive(name)
           || name == "allow_cast"
           || name == "native";
}

std::optional<ty::type_info> struct_definition_expression::type_check(ty::context& ctx)
{
    ctx.enter_struct_scope(name);
    for(auto& m: members)
    {
        m->type_check(ctx);
    }
    ctx.exit_named_scope(name);

    return std::nullopt;
}

std::string struct_definition_expression::to_string() const
{
    std::string ret = std::format("Struct(name={}, members=(", name.s);
    if(!members.empty())
    {
        for(std::size_t i = 0; i < members.size() - 1; ++i)
        {
            ret += std::format("{}, ", members[i]->to_string());
        }
        ret += std::format("{}", members.back()->to_string());
    }
    ret += "))";
    return ret;
}

/*
 * struct_anonymous_initializer_expression.
 */

std::unique_ptr<expression> struct_anonymous_initializer_expression::clone() const
{
    return std::make_unique<struct_anonymous_initializer_expression>(*this);
}

void struct_anonymous_initializer_expression::serialize(archive& ar)
{
    super::serialize(ar);
    ar& expression_vector_serializer{initializers};
}

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
        auto* type = ctx.get_type(name.s, get_namespace_path());
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
            throw cg::codegen_error(
              loc,
              std::format(
                "Code generation for '{}.{}' initialization returned no type.",
                name.s,
                member_name));
        }
        if(!initializer_value->get_type().is_null()
           && initializer_value->get_type().to_string() != member_type.get_type().to_string())
        {
            throw cg::codegen_error(
              loc,
              std::format(
                "Code generation for '{}.{}' initialization returned '{}' (expected '{}').",
                name.s,
                member_name,
                initializer_value->get_type().to_string(),
                member_type.get_type().to_string()));
        }

        ctx.generate_set_field(std::make_unique<cg::field_access_argument>(struct_type, member_type));
    }

    return std::make_unique<cg::value>(cg::type{cg::type_class::struct_, 0, name.s});
}

std::optional<ty::type_info> struct_anonymous_initializer_expression::type_check(ty::context& ctx)
{
    const auto* struct_def = ctx.get_struct_definition(name.location, name.s, get_namespace_path());

    if(initializers.size() != struct_def->members.size())
    {
        throw ty::type_error(
          name.location,
          std::format(
            "Struct '{}' has {} members, but {} are initialized.",
            name.s,
            struct_def->members.size(),
            initializers.size()));
    }

    for(std::size_t i = 0; i < initializers.size(); ++i)
    {
        const auto& initializer = initializers[i];
        const auto& struct_member = struct_def->members[i];

        auto initializer_type = initializer->type_check(ctx);
        if(!initializer_type.has_value())
        {
            throw ty::type_error(initializer->get_location(), "Initializer has no type.");
        }

        // Either the types match, or the type is a reference types which is set to 'null'.
        if(struct_member.second != initializer_type
           && !(ctx.is_reference_type(struct_member.second)
                && initializer_type == ctx.get_type("@null", false)))
        {
            throw ty::type_error(
              name.location,
              std::format(
                "Struct member '{}.{}' has type '{}', but initializer has type '{}'.",
                name.s,
                struct_member.first.s,
                struct_member.second.to_string(),
                initializer_type->to_string()));
        }
    }

    expr_type = ctx.get_type(name.s, false, get_namespace_path());
    ctx.set_expression_type(this, *expr_type);

    return expr_type;
}

std::string struct_anonymous_initializer_expression::to_string() const
{
    std::string ret = std::format("StructAnonymousInitializer(name={}, initializers=(", name.s);
    if(!initializers.empty())
    {
        for(std::size_t i = 0; i < initializers.size() - 1; ++i)
        {
            ret += std::format("{}, ", initializers[i]->to_string());
        }
        ret += std::format("{}", initializers.back()->to_string());
    }
    ret += "))";
    return ret;
}

/*
 * named_initializer.
 */

std::unique_ptr<expression> named_initializer::clone() const
{
    return std::make_unique<named_initializer>(*this);
}

void named_initializer::serialize(archive& ar)
{
    super::serialize(ar);
    ar& expression_serializer{expr};
}

std::unique_ptr<cg::value> named_initializer::generate_code(
  cg::context& ctx,
  memory_context mc) const
{
    return expr->generate_code(ctx, mc);
}

std::optional<ty::type_info> named_initializer::type_check(ty::context& ctx)
{
    return expr->type_check(ctx);
}

std::string named_initializer::to_string() const
{
    return std::format(
      "NamedInitializer(name={}, expr={})",
      get_name().s,
      expr->to_string());
}

/*
 * struct_named_initializer_expression.
 */

std::unique_ptr<expression> struct_named_initializer_expression::clone() const
{
    return std::make_unique<struct_named_initializer_expression>(*this);
}

void struct_named_initializer_expression::serialize(archive& ar)
{
    super::serialize(ar);
    ar& expression_vector_serializer{initializers};
}

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
        auto* type = ctx.get_type(name.s, get_namespace_path());
        struct_type = {cg::type_class::struct_, 0, type->get_name(), type->get_import_path()};

        t = &ctx.get_type(name.s, get_namespace_path())->get_members();
        ctx.generate_new(struct_type);
    }

    for(const auto& initializer: initializers)
    {
        const auto& member_name = initializer->get_name().s;

        auto it = std::ranges::find_if(
          *t,
          [&member_name](const std::pair<std::string, cg::value>& m) -> bool
          { return m.first == member_name; });
        if(it == t->cend())
        {
            throw ty::type_error(
              name.location,
              std::format(
                "Struct '{}' has no member '{}'.",
                name.s, member_name));
        }

        const auto& member_type = it->second;

        ctx.generate_dup(cg::value{struct_type});

        auto initializer_value = initializer->generate_code(ctx, memory_context::load);
        if(!initializer_value)
        {
            throw cg::codegen_error(
              loc,
              std::format(
                "Code generation for '{}.{}' initialization returned no type.",
                name.s, member_name));
        }
        if(!initializer_value->get_type().is_null()
           && initializer_value->get_type().to_string() != member_type.get_type().to_string())
        {
            throw cg::codegen_error(
              loc,
              std::format(
                "Code generation for '{}.{}' initialization returned '{}' (expected '{}').",
                name.s,
                member_name,
                initializer_value->get_type().to_string(),
                member_type.get_type().to_string()));
        }

        ctx.generate_set_field(std::make_unique<cg::field_access_argument>(struct_type, member_type));
    }

    return std::make_unique<cg::value>(cg::type{cg::type_class::struct_, 0, name.s});
}

std::optional<ty::type_info> struct_named_initializer_expression::type_check(ty::context& ctx)
{
    const auto* struct_def = ctx.get_struct_definition(name.location, name.s, get_namespace_path());

    if(initializers.size() != struct_def->members.size())
    {
        throw ty::type_error(
          name.location,
          std::format(
            "Struct '{}' has {} members, but {} are initialized.",
            name.s, struct_def->members.size(), initializers.size()));
    }

    std::vector<std::string> initialized_member_names;    // used in check for duplicates
    for(const auto& initializer: initializers)
    {
        const auto& member_name = initializer->get_name().s;

        if(std::ranges::find_if(
             initialized_member_names,
             [&member_name](auto& name) -> bool
             { return name == member_name; })
           != initialized_member_names.end())
        {
            throw ty::type_error(
              name.location,
              std::format(
                "Multiple initializations of struct member '{}::{}'.",
                name.s,
                member_name));
        }
        initialized_member_names.push_back(member_name);

        auto it = std::ranges::find_if(
          struct_def->members,
          [&member_name](const auto& m) -> bool
          { return m.first.s == member_name; });
        if(it == struct_def->members.end())
        {
            throw ty::type_error(
              name.location,
              std::format(
                "Struct '{}' has no member '{}'.",
                name.s, member_name));
        }

        auto initializer_type = initializer->type_check(ctx);
        if(!initializer_type.has_value())
        {
            throw ty::type_error(initializer->get_location(), "Initializer has no type.");
        }

        // Either the types match, or the type is a reference types which is set to 'null'.
        if(it->second != initializer_type
           && !(ctx.is_reference_type(it->second) && initializer_type == ctx.get_type("@null", false)))
        {
            throw ty::type_error(
              name.location,
              std::format(
                "Struct member '{}.{}' has type '{}', but initializer has type '{}'.",
                name.s,
                member_name,
                it->second.to_string(),
                initializer_type->to_string()));
        }
    }

    expr_type = ctx.get_type(name.s, false, get_namespace_path());
    ctx.set_expression_type(this, *expr_type);

    return expr_type;
}

std::string struct_named_initializer_expression::to_string() const
{
    std::string ret = std::format("StructNamedInitializer(name={}, initializers=(", name.s);

    if(!initializers.empty())
    {
        for(std::size_t i = 0; i < initializers.size() - 1; ++i)
        {
            ret += std::format(
              "name={}, expr={}, ",
              initializers[i]->get_name().s,
              initializers[i]->get_expression()->to_string());
        }
        ret += std::format(
          "name={}, expr={}",
          initializers.back()->get_name().s,
          initializers.back()->get_expression()->to_string());
    }
    ret += ")";

    return ret;
}

/*
 * binary_expression.
 */

std::unique_ptr<expression> binary_expression::clone() const
{
    return std::make_unique<binary_expression>(*this);
}

void binary_expression::serialize(archive& ar)
{
    super::serialize(ar);
    ar & op;
    ar& expression_serializer{lhs};
    ar& expression_serializer{rhs};
}

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

bool binary_expression::is_pure(cg::context& ctx) const
{
    auto [is_assignment, is_compound, is_comparison, reduced_op] = classify_binary_op(op.s);
    return !is_assignment && lhs->is_pure(ctx) && rhs->is_pure(ctx);
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

    std::unique_ptr<cg::value> lhs_value, lhs_store_value, rhs_value;    // NOLINT(readability-isolate-declaration)
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
        // Evaluate constant subexpressions.
        bool ctx_const_eval = ctx.evaluate_constant_subexpressions;
        bool disable_const_eval = !ctx_const_eval;

        disable_const_eval = ctx.get_directive_flag(
          "disable",
          "const_eval",
          disable_const_eval);

        {
            // Reset constant expression evaluation on scope exit.
            auto _ = gsl::finally(
              [&ctx, ctx_const_eval]
              { ctx.evaluate_constant_subexpressions = ctx_const_eval; });
            ctx.evaluate_constant_subexpressions = !disable_const_eval;

            if(ctx.evaluate_constant_subexpressions
               && is_const_eval(ctx))
            {
                auto v = evaluate(ctx);
                if(v)
                {
                    auto t = v->get_type();
                    if(t.to_string() == "i32")
                    {
                        ctx.generate_const(*v, static_cast<cg::constant_int*>(v.get())->get_int());    // NOLINT(cppcoreguidelines-pro-type-static-cast-downcast)
                        return v;
                    }
                    if(t.to_string() == "f32")
                    {
                        ctx.generate_const(*v, static_cast<cg::constant_float*>(v.get())->get_float());    // NOLINT(cppcoreguidelines-pro-type-static-cast-downcast)
                        return v;
                    }

                    // fall-through
                }

                std::println("{}: Warning: Attempted constant expression computation failed.", ::slang::to_string(loc));
                // fall-through
            }
        }

        lhs_value = lhs->generate_code(ctx, memory_context::load);
        rhs_value = rhs->generate_code(ctx, memory_context::load);

        if(lhs_value->get_type() != rhs_value->get_type()
           && !(is_comparison && lhs_value->get_type().is_reference() && rhs_value->get_type().is_null()))
        {
            throw cg::codegen_error(
              loc,
              std::format(
                "Types don't match in binary operation. L.h.s.: {}, R.h.s.: {}.",
                lhs_value->get_type().to_string(),
                rhs_value->get_type().to_string()));
        }

        auto it = binary_op_map.find(reduced_op);
        if(it == binary_op_map.end())
        {
            throw std::runtime_error(
              std::format(
                "{}: Code generation for binary operator '{}' not implemented.",
                slang::to_string(loc), op.s));
        }

        ctx.generate_binary_op(it->second, *lhs_value);

        /* Case 7. */
        if(is_comparison)
        {
            // comparisons are non-compound, so this must be a non-assignment operation.
            return std::make_unique<cg::value>(cg::type{cg::type_class::i32, 0});
        }
        if(!is_assignment)
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
            if(ty::is_builtin_type(rhs_value->get_type().to_string()))
            {
                ctx.generate_dup(*rhs_value, {cg::value{cg::type{cg::type_class::addr, 0}}});
            }
            else if(
              rhs_value->get_type().is_array()
              || rhs_value->get_type().is_reference()
              || rhs_value->get_type().is_null())
            {
                ctx.generate_dup(
                  {cg::value{cg::type{cg::type_class::addr, 0}}},
                  {cg::value{cg::type{cg::type_class::addr, 0}}});
            }
            else
            {
                throw cg::codegen_error(
                  loc,
                  std::format(
                    "Unexpected type '{}' when generating dup instruction.",
                    rhs_value->to_string()));
            }
        }

        // by the above check, lhs is an access_expression.
        access_expression* ae_lhs = lhs->as_access_expression();
        ty::type_info struct_type_info = ae_lhs->get_struct_type();
        cg::type struct_type{
          cg::type_class::struct_,
          0,
          struct_type_info.to_string(),
          struct_type_info.get_import_path()};

        ctx.generate_set_field(
          std::make_unique<cg::field_access_argument>(
            struct_type,
            *lhs_store_value));
        return rhs_value;
    }
    /* Cases 2. (cont.), 5. (cont.) */
    if(lhs->is_array_element_access())
    {
        // duplicate the value for chained assignments.
        if(mc == memory_context::load)
        {
            if(ty::is_builtin_type(rhs_value->get_type().to_string()))
            {
                ctx.generate_dup(*rhs_value,
                                 {cg::value{cg::type{cg::type_class::i32, 0}},
                                  cg::value{cg::type{cg::type_class::addr, 0}}});
            }
            else if(
              rhs_value->get_type().is_array()
              || rhs_value->get_type().is_reference()
              || rhs_value->get_type().is_null())
            {
                ctx.generate_dup(cg::value{cg::type{cg::type_class::addr, 0}},
                                 {cg::value{cg::type{cg::type_class::i32, 0}},
                                  cg::value{cg::type{cg::type_class::addr, 0}}});
            }
            else
            {
                throw cg::codegen_error(
                  loc,
                  std::format(
                    "Unexpected type '{}' when generating dup instruction.",
                    rhs_value->to_string()));
            }
        }

        ctx.generate_store(
          std::make_unique<cg::variable_argument>(
            std::make_unique<cg::value>(*rhs_value)),
          true);

        return rhs_value;
    }
    /* Case 1. (cont.), 4. (cont.) */
    // we might need to duplicate the value for chained assignments.
    if(mc == memory_context::load)
    {
        ctx.generate_dup(*rhs_value);
    }

    return lhs->generate_code(ctx, memory_context::store);
}

std::optional<ty::type_info> binary_expression::type_check(ty::context& ctx)
{
    if(!ctx.has_expression_type(*lhs) || !ctx.has_expression_type(*rhs))
    {
        // visit the nodes to get the types. note that if we are here,
        // no type has been set yet, so we can traverse all nodes without
        // doing evaluation twice.
        visit_nodes(
          [&ctx](ast::expression& node)
          {
              node.type_check(ctx);
          },
          false, /* don't visit this node */
          true   /* post-order traversal */
        );
    }

    auto [is_assignment, is_compound, is_comparison, reduced_op] = classify_binary_op(op.s);

    auto lhs_type = ctx.get_expression_type(*lhs);

    const ty::struct_definition* struct_def = nullptr;
    if(op.s == ".")    // scope access
    {
        struct_def = ctx.get_struct_definition(loc, lhs_type.to_string(), lhs_type.get_import_path());
        ctx.push_struct_definition(struct_def);
    }

    auto rhs_type = ctx.get_expression_type(*rhs);

    if(struct_def != nullptr)
    {
        ctx.pop_struct_definition();

        expr_type = rhs_type;
        ctx.set_expression_type(this, *expr_type);
        return expr_type;
    }

    // some operations restrict the type.
    if(reduced_op == "%"
       || reduced_op == "<<" || reduced_op == ">>"
       || reduced_op == "&" || reduced_op == "^" || reduced_op == "|"
       || reduced_op == "&&" || reduced_op == "||")
    {
        if(ty::to_string(lhs_type) != "i32" || ty::to_string(rhs_type) != "i32")
        {
            throw ty::type_error(
              loc,
              std::format(
                "Got binary expression of type '{}' {} '{}', expected 'i32' {} 'i32'.",
                ty::to_string(lhs_type),
                reduced_op,
                ty::to_string(rhs_type),
                reduced_op));
        }

        // set the restricted type.
        expr_type = ctx.get_type("i32", false);
        ctx.set_expression_type(this, *expr_type);
        return expr_type;
    }

    // assignments and comparisons.
    if(op.s == "=" || op.s == "==" || op.s == "!=")
    {
        // Either the types match, or the type is a reference types which is set to 'null'.
        if(lhs_type != rhs_type
           && !(ctx.is_reference_type(lhs_type) && rhs_type == ctx.get_type("@null", false)))
        {
            throw ty::type_error(
              loc,
              std::format(
                "Types don't match in binary expression. Got expression of type '{}' {} '{}'.",
                ty::to_string(lhs_type),
                reduced_op,
                ty::to_string(rhs_type)));
        }

        if(op.s == "=")
        {
            // assignments return the type of the l.h.s.
            expr_type = lhs_type;
            ctx.set_expression_type(this, *expr_type);
            return expr_type;
        }

        // comparisons return i32.
        expr_type = ctx.get_type("i32", false);
        ctx.set_expression_type(this, *expr_type);
        return expr_type;
    }

    // check lhs and rhs have supported types (i32 and f32 at the moment).
    if(lhs_type != ctx.get_type("i32", false) && lhs_type != ctx.get_type("f32", false))
    {
        throw ty::type_error(
          loc,
          std::format(
            "Expected 'i32' or 'f32' for l.h.s. of binary operation of type '{}', got '{}'.",
            reduced_op,
            ty::to_string(lhs_type)));
    }
    if(rhs_type != ctx.get_type("i32", false) && rhs_type != ctx.get_type("f32", false))
    {
        throw ty::type_error(
          loc,
          std::format(
            "Expected 'i32' or 'f32' for r.h.s. of binary operation of type '{}', got '{}'.",
            reduced_op,
            ty::to_string(rhs_type)));
    }

    if(lhs_type != rhs_type)
    {
        throw ty::type_error(
          loc,
          std::format(
            "Types don't match in binary expression. Got expression of type '{}' {} '{}'.",
            ty::to_string(lhs_type),
            reduced_op,
            ty::to_string(rhs_type)));
    }

    if(is_comparison)
    {
        // comparisons return i32.
        expr_type = ctx.get_type("i32", false);
    }
    else
    {
        // set the type of the binary expression.
        expr_type = lhs_type;
    }

    ctx.set_expression_type(this, *expr_type);
    return expr_type;
}

std::string binary_expression::to_string() const
{
    return std::format(
      "Binary(op=\"{}\", lhs={}, rhs={})", op.s,
      lhs ? lhs->to_string() : std::string("<none>"),
      rhs ? rhs->to_string() : std::string("<none>"));
}

/*
 * unary_expression.
 */

std::unique_ptr<expression> unary_expression::clone() const
{
    return std::make_unique<unary_expression>(*this);
}

void unary_expression::serialize(archive& ar)
{
    super::serialize(ar);
    ar & op;
    ar& expression_serializer{operand};
}

bool unary_expression::is_pure(cg::context& ctx) const
{
    return operand->is_pure(ctx);
}

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
            throw cg::codegen_error(
              loc,
              std::format(
                "Wrong expression type '{}' for prefix operator '++'. Expected 'i32' or 'f32'.",
                v->get_type().to_string()));
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

    if(op.s == "--")
    {
        auto v = operand->generate_code(ctx, mc);
        if(v->get_type().to_string() != "i32" && v->get_type().to_string() != "f32")
        {
            throw cg::codegen_error(
              loc,
              std::format(
                "Wrong expression type '{}' for prefix operator '--'. Expected 'i32' or 'f32'.",
                v->get_type().to_string()));
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

    // Evaluate constant subexpressions.
    bool ctx_const_eval = ctx.evaluate_constant_subexpressions;
    bool disable_const_eval = !ctx_const_eval;

    disable_const_eval = ctx.get_directive_flag(
      "disable",
      "const_eval",
      disable_const_eval);

    {
        // Reset constant expression evaluation on scope exit.
        auto _ = gsl::finally(
          [&ctx, ctx_const_eval]
          { ctx.evaluate_constant_subexpressions = ctx_const_eval; });
        ctx.evaluate_constant_subexpressions = !disable_const_eval;

        if(ctx.evaluate_constant_subexpressions
           && is_const_eval(ctx))
        {
            auto v = evaluate(ctx);
            if(v)
            {
                auto t = v->get_type();
                if(t.to_string() == "i32")
                {
                    ctx.generate_const(*v, static_cast<cg::constant_int*>(v.get())->get_int());    // NOLINT(cppcoreguidelines-pro-type-static-cast-downcast)
                    return v;
                }
                if(t.to_string() == "f32")
                {
                    ctx.generate_const(*v, static_cast<cg::constant_float*>(v.get())->get_float());    // NOLINT(cppcoreguidelines-pro-type-static-cast-downcast)
                    return v;
                }

                // fall-through
            }

            std::println("{}: Warning: Attempted constant expression computation failed.", ::slang::to_string(loc));
            // fall-through
        }
    }

    if(op.s == "+")
    {
        return operand->generate_code(ctx, mc);
    }

    if(op.s == "-")
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
            throw cg::codegen_error(
              loc,
              std::format(
                "Type error for unary operator '-': Expected 'i32' or 'f32', got '{}'.",
                v->get_type().to_string()));
        }
        instrs.insert(instrs.begin() + pos, std::make_unique<cg::instruction>("const", std::move(args)));    // NOLINT(bugprone-narrowing-conversions,cppcoreguidelines-narrowing-conversions)

        ctx.generate_binary_op(cg::binary_op::op_sub, *v);
        return v;
    }

    if(op.s == "!")
    {
        auto v = operand->generate_code(ctx, memory_context::load);
        if(v->get_type().get_type_class() != cg::type_class::i32)
        {
            throw cg::codegen_error(
              loc,
              std::format(
                "Type error for unary operator '!': Expected 'i32', got '{}'.",
                v->get_type().to_string()));
        }

        ctx.generate_const({cg::type{cg::type_class::i32, 0}}, 0);
        ctx.generate_binary_op(cg::binary_op::op_equal, *v);

        return std::make_unique<cg::value>(cg::type{cg::type_class::i32, 0});
    }

    if(op.s == "~")
    {
        auto& instrs = ctx.get_insertion_point()->get_instructions();
        std::size_t pos = instrs.size();

        auto v = operand->generate_code(ctx, mc);
        if(v->get_type().get_type_class() != cg::type_class::i32)
        {
            throw cg::codegen_error(
              loc,
              std::format(
                "Type error for unary operator '~': Expected 'i32', got '{}'.",
                v->get_type().to_string()));
        }

        std::vector<std::unique_ptr<cg::argument>> args;
        args.emplace_back(std::make_unique<cg::const_argument>(~0));

        instrs.insert(instrs.begin() + pos, std::make_unique<cg::instruction>("const", std::move(args)));    // NOLINT(bugprone-narrowing-conversions,cppcoreguidelines-narrowing-conversions)

        ctx.generate_binary_op(cg::binary_op::op_xor, *v);
        return v;
    }

    throw std::runtime_error(
      std::format(
        "{}: Code generation for unary operator '{}' not implemented.",
        slang::to_string(loc),
        op.s));
}

std::optional<ty::type_info> unary_expression::type_check(ty::context& ctx)
{
    if(!ctx.has_expression_type(*operand))
    {
        // visit the nodes to get the types. note that if we are here,
        // no type has been set yet, so we can traverse all nodes without
        // doing evaluation twice.
        visit_nodes(
          [&ctx](ast::expression& node)
          {
              node.type_check(ctx);
          },
          false, /* don't visit this node */
          true   /* post-order traversal */
        );
    }

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
        throw ty::type_error(
          op.location,
          std::format(
            "Unknown unary operator '{}'.",
            op.s));
    }

    auto operand_type = ctx.get_expression_type(*operand);
    auto type_it = std::ranges::find(op_it->second, ty::to_string(operand_type));
    if(type_it == op_it->second.end())
    {
        throw ty::type_error(
          operand->get_location(),
          std::format(
            "Invalid operand type '{}' for unary operator '{}'.",
            ty::to_string(operand_type),
            op.s));
    }

    expr_type = operand_type;
    ctx.set_expression_type(this, *expr_type);

    return expr_type;
}

std::string unary_expression::to_string() const
{
    return std::format(
      "Unary(op=\"{}\", operand={})",
      op.s,
      operand ? operand->to_string() : std::string("<none>"));
}

/*
 * new_expression.
 */

std::unique_ptr<expression> new_expression::clone() const
{
    return std::make_unique<new_expression>(*this);
}

void new_expression::serialize(archive& ar)
{
    super::serialize(ar);
    ar & type_expr;
    ar& expression_serializer{expr};
}

bool new_expression::is_pure(cg::context& ctx) const
{
    return expr->is_pure(ctx);
}

std::unique_ptr<cg::value> new_expression::generate_code(cg::context& ctx, memory_context mc) const
{
    if(mc == memory_context::store)
    {
        throw cg::codegen_error(loc, "Cannot store into new expression.");
    }

    token type_name = type_expr->get_name();
    if(ty::is_builtin_type(type_name.s) && type_expr->get_namespace_path().has_value())
    {
        throw cg::codegen_error(
          loc,
          "Cannot use namespaces with built-in types.");
    }

    if(type_name.s == "void")
    {
        throw cg::codegen_error(
          loc,
          "Cannot create array with entries of type 'void'.");
    }

    // generate array size.
    std::unique_ptr<cg::value> v = expr->generate_code(ctx, memory_context::load);
    if(v->get_type().get_type_class() != cg::type_class::i32)
    {
        throw cg::codegen_error(
          loc,
          std::format(
            "Expected <integer> as array size, got '{}'.",
            v->get_type().to_string()));
    }

    if(ty::is_builtin_type(type_name.s))
    {
        ctx.generate_newarray(cg::value{cg::type{cg::to_type_class(type_name.s), 0}});
        return std::make_unique<cg::value>(cg::type{cg::to_type_class(type_name.s), 1});
    }

    // custom type.
    ctx.generate_anewarray(cg::value{type_expr->to_type()});
    return std::make_unique<cg::value>(type_expr->to_type());
}

std::optional<ty::type_info> new_expression::type_check(ty::context& ctx)
{
    token type_name = type_expr->get_name();
    if(ty::is_builtin_type(type_name.s) && type_expr->get_namespace_path().has_value())
    {
        throw cg::codegen_error(
          loc,
          "Cannot use namespaces with built-in types.");
    }

    if(type_name.s == "void")
    {
        throw ty::type_error(
          type_name.location,
          "Cannot use operator new with type 'void'.");
    }

    auto array_size_type = expr->type_check(ctx);
    if(!array_size_type.has_value())
    {
        throw ty::type_error(
          expr->get_location(),
          "Array size expression has no type.");
    }

    if(ty::to_string(*array_size_type) != "i32")
    {
        throw ty::type_error(
          expr->get_location(),
          std::format(
            "Expected array size of type 'i32', got '{}'.",
            ty::to_string(*array_size_type)));
    }

    expr_type = ctx.get_type(type_name.s, true, type_expr->get_namespace_path());
    ctx.set_expression_type(this, *expr_type);

    return expr_type;
}

std::string new_expression::to_string() const
{
    return std::format(
      "NewExpression(type={}, expr={})",
      type_expr->to_string(), expr->to_string());
}

/*
 * null_expression.
 */

std::unique_ptr<expression> null_expression::clone() const
{
    return std::make_unique<null_expression>(*this);
}

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
    expr_type = ctx.get_type("@null", false);
    ctx.set_expression_type(this, *expr_type);
    return expr_type;
}

std::string null_expression::to_string() const
{
    return "NullExpression()";
}

/*
 * postfix_expression.
 */

std::unique_ptr<expression> postfix_expression::clone() const
{
    return std::make_unique<postfix_expression>(*this);
}

void postfix_expression::serialize(archive& ar)
{
    super::serialize(ar);
    ar& expression_serializer{identifier};
    ar & op;
}

std::unique_ptr<cg::value> postfix_expression::generate_code(cg::context& ctx, memory_context mc) const
{
    if(mc == memory_context::store)
    {
        throw cg::codegen_error(loc, "Cannot store into postfix operator expression.");
    }

    auto v = identifier->generate_code(ctx, memory_context::load);
    if(v->get_type().to_string() != "i32" && v->get_type().to_string() != "f32")
    {
        throw cg::codegen_error(
          loc,
          std::format(
            "Wrong expression type '{}' for postfix operator. Expected 'i32' or 'f32'.",
            v->get_type().to_string()));
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
        throw cg::codegen_error(
          op.location,
          std::format(
            "Unknown postfix operator '{}'.",
            op.s));
    }

    return v;
}

std::optional<ty::type_info> postfix_expression::type_check(ty::context& ctx)
{
    auto identifier_type = identifier->type_check(ctx);
    if(!identifier_type.has_value())
    {
        throw ty::type_error(identifier->get_location(), "Identifier has no type.");
    }

    auto identifier_type_str = ty::to_string(*identifier_type);
    if(identifier_type_str != "i32" && identifier_type_str != "f32")
    {
        throw ty::type_error(
          identifier->get_location(),
          std::format(
            "Postfix operator '{}' can only operate on 'i32' or 'f32' (found '{}').",
            op.s,
            identifier_type_str));
    }

    expr_type = identifier_type;
    ctx.set_expression_type(this, *expr_type);

    return expr_type;
}

std::string postfix_expression::to_string() const
{
    return std::format("Postfix(identifier={}, op=\"{}\")", identifier->to_string(), op.s);
}

/*
 * prototype.
 */

std::unique_ptr<prototype_ast> prototype_ast::clone() const
{
    return std::make_unique<prototype_ast>(*this);
}

void prototype_ast::serialize(archive& ar)
{
    ar & loc;
    ar & name;
    ar & args;
    ar & return_type;
    ar & args_type_info;
    ar & return_type_info;
}

cg::function* prototype_ast::generate_code(cg::context& ctx, memory_context mc) const
{
    if(mc != memory_context::none)
    {
        throw cg::codegen_error(loc, "Invalid memory context for prototype.");
    }

    if(args.size() != args_type_info.size())
    {
        throw cg::codegen_error(loc, "Argument count differs from argument type count.");
    }

    std::vector<std::unique_ptr<cg::value>> function_args;
    function_args.reserve(args.size());
    for(std::size_t i = 0; i < args.size(); ++i)
    {
        function_args.emplace_back(type_info_to_value(args_type_info[i], args[i].first.s));
    }

    std::unique_ptr<cg::value> ret = type_info_to_value(return_type_info);
    return ctx.create_function(name.s, std::move(ret), std::move(function_args));
}

void prototype_ast::generate_native_binding(const std::string& lib_name, cg::context& ctx) const
{
    if(args.size() != args_type_info.size())
    {
        throw cg::codegen_error(loc, "Argument count differs from argument type count.");
    }

    std::vector<std::unique_ptr<cg::value>> function_args;
    function_args.reserve(args.size());
    for(std::size_t i = 0; i < args.size(); ++i)
    {
        function_args.emplace_back(type_info_to_value(args_type_info[i], args[i].first.s));
    }

    std::unique_ptr<cg::value> ret = type_info_to_value(return_type_info);
    ctx.create_native_function(lib_name, name.s, std::move(ret), std::move(function_args));
}

void prototype_ast::collect_names(cg::context& ctx, ty::context& type_ctx) const
{
    std::vector<cg::value> prototype_arg_types =
      args
      | std::views::transform(
        [](const std::pair<token, std::unique_ptr<type_expression>>& arg) -> cg::value
        {
            if(ty::is_builtin_type(std::get<1>(arg)->get_name().s))
            {
                return cg::value{
                  cg::type{cg::to_type_class(std::get<1>(arg)->get_name().s),
                           std::get<1>(arg)->is_array()
                             ? static_cast<std::size_t>(1)
                             : static_cast<std::size_t>(0)}};
            }

            return cg::value{std::get<1>(arg)->to_type()};
        })
      | std::ranges::to<std::vector>();

    cg::value ret_val = cg::value{return_type->to_type()};
    ctx.add_prototype(name.s, std::move(ret_val), std::move(prototype_arg_types));

    std::vector<ty::type_info> arg_types =
      args
      | std::views::transform(
        [&type_ctx](const std::pair<token, std::unique_ptr<type_expression>>& arg) -> ty::type_info
        { return std::get<1>(arg)->to_unresolved_type_info(type_ctx); })
      | std::ranges::to<std::vector>();

    type_ctx.add_function(
      name,
      std::move(arg_types),
      return_type->to_unresolved_type_info(type_ctx));
}

void prototype_ast::type_check(ty::context& ctx)
{
    // get the argument types and add them to the current scope.
    args_type_info.clear();
    for(const auto& arg: args)
    {
        args_type_info.push_back(std::get<1>(arg)->to_type_info(ctx));
        ctx.add_variable(std::get<0>(arg), args_type_info.back());
    }

    // get and check the return type.
    return_type_info = return_type->to_type_info(ctx);
}

std::string prototype_ast::to_string() const
{
    std::string ret_type_str = return_type->to_string();
    std::string ret = std::format("Prototype(name={}, return_type={}, args=(", name.s, ret_type_str);
    if(!args.empty())
    {
        for(std::size_t i = 0; i < args.size() - 1; ++i)
        {
            std::string arg_type_str = std::get<1>(args[i])->to_string();
            ret += std::format("(name={}, type={}), ", std::get<0>(args[i]).s, arg_type_str);
        }
        std::string arg_type_str = std::get<1>(args.back())->to_string();
        ret += std::format("(name={}, type={})", std::get<0>(args.back()).s, arg_type_str);
    }
    ret += "))";
    return ret;
}

/*
 * block.
 */

std::unique_ptr<expression> block::clone() const
{
    return std::make_unique<block>(*this);
}

void block::serialize(archive& ar)
{
    super::serialize(ar);
    ar& expression_vector_serializer{exprs};
}

bool block::is_pure(cg::context& ctx) const
{
    return std::ranges::all_of(
      exprs,
      [&ctx](const auto& e) -> bool
      {
          return e->is_pure(ctx);
      });
}

std::unique_ptr<cg::value> block::generate_code(cg::context& ctx, memory_context mc) const
{
    if(mc == memory_context::none)
    {
        bool was_terminated = false;

        for(const auto& expr: exprs)
        {
            if(was_terminated)
            {
                auto* fn = ctx.get_current_function();
                if(fn != nullptr)
                {
                    cg::basic_block* bb = cg::basic_block::create(ctx, ctx.generate_label());
                    fn->append_basic_block(bb);
                    ctx.set_insertion_point(bb);
                }
            }

            if(expr->is_pure(ctx))
            {
                std::println("{}: Expression has no effect.", ::slang::to_string(expr->get_location()));

                // don't generate code.
                continue;
            }

            std::unique_ptr<cg::value> v = expr->generate_code(ctx, memory_context::none);

            // non-assigning expressions need cleanup.
            if(expr->needs_pop())
            {
                if(!v)
                {
                    throw cg::codegen_error(loc, "Expression requires popping the stack, but didn't produce a value.");
                }

                ctx.generate_pop(*v);
            }

            auto* bb = ctx.get_insertion_point();
            if(ctx.get_current_function() != nullptr
               && bb != nullptr)
            {
                was_terminated = bb->is_terminated();
            }
            else
            {
                was_terminated = false;
            }
        }

        return nullptr;
    }

    if(mc == memory_context::load)
    {
        for(std::size_t i = 0; i < exprs.size() - 1; ++i)
        {
            const auto& expr = exprs[i];
            if(expr->is_pure(ctx))
            {
                std::println("{}: Expression has no effect.", ::slang::to_string(expr->get_location()));

                // don't generate code.
                continue;
            }
            std::unique_ptr<cg::value> v = expr->generate_code(ctx, memory_context::none);

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

        // the last expression is loaded.
        return exprs.back()->generate_code(ctx, memory_context::load);
    }

    throw cg::codegen_error(loc, "Invalid memory context for code block.");
}

void block::collect_names(cg::context& ctx, ty::context& type_ctx) const
{
    for(const auto& expr: exprs)
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
    if(!exprs.empty())
    {
        for(std::size_t i = 0; i < exprs.size() - 1; ++i)
        {
            ret += std::format("{}, ", exprs[i] ? exprs[i]->to_string() : std::string("<none>"));
        }
        ret += std::format("{}", exprs.back() ? exprs.back()->to_string() : std::string("<none>"));
    }
    ret += "))";
    return ret;
}

/*
 * function_expression.
 */

std::unique_ptr<expression> function_expression::clone() const
{
    return std::make_unique<function_expression>(*this);
}

void function_expression::serialize(archive& ar)
{
    super::serialize(ar);
    bool has_prototype = static_cast<bool>(prototype);
    ar & has_prototype;
    if(has_prototype)
    {
        if(!static_cast<bool>(prototype))
        {
            prototype = std::make_unique<prototype_ast>();
        }
        prototype->serialize(ar);
    }
    else
    {
        prototype = {};
    }
    ar& expression_serializer{body};
}

std::unique_ptr<cg::value> function_expression::generate_code(cg::context& ctx, memory_context mc) const
{
    auto d = ctx.get_last_directive("native");
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
            throw cg::codegen_error(
              loc,
              std::format(
                "No function body defined for '{}'.",
                prototype->get_name().s));
        }

        auto v = body->generate_code(ctx);

        // verify that the break-continue-stack is empty.
        if(ctx.get_break_continue_stack_size() != 0)
        {
            throw cg::codegen_error(
              loc,
              "Internal error: Break-continue stack is not empty.");
        }

        auto* ip = ctx.get_insertion_point(true);
        if(!ip->ends_with_return())
        {
            // for `void` return types, we insert a return instruction. otherwise, the
            // return statement is missing and we throw an error.
            auto ret_type = std::get<0>(fn->get_signature());
            if(!ret_type.is_void())
            {
                throw cg::codegen_error(
                  loc,
                  std::format(
                    "Missing return statement in function '{}'.",
                    fn->get_name()));
            }

            ctx.generate_ret(v ? std::make_optional(*v) : std::nullopt);
        }
    }
    else
    {
        auto& directive = *d;
        if(directive.args.size() != 1 || directive.args[0].first.s != "lib")
        {
            throw cg::codegen_error(
              loc,
              std::format(
                "Native function '{}': Expected argument 'lib' for directive.",
                prototype->get_name().s));
        }

        if(directive.args[0].second.type != token_type::str_literal && directive.args[0].second.type != token_type::identifier)
        {
            throw cg::codegen_error(
              loc,
              "Expected 'lib=<identifier>' or 'lib=<string-literal>'.");
        }

        if(directive.args[0].second.type == token_type::str_literal
           && !directive.args[0].second.value.has_value())
        {
            throw cg::codegen_error(
              loc,
              "Directive argument has no value for <string-literal>.");
        }

        std::string lib_name =
          (directive.args[0].second.type == token_type::str_literal)
            ? std::get<std::string>(*directive.args[0].second.value)    // NOLINT(bugprone-unchecked-optional-access)
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
    return super::supports_directive(name)
           || name == "native";
}

std::optional<ty::type_info> function_expression::type_check(ty::context& ctx)
{
    // enter function scope. the scope is exited in the type_check for the function's body.
    ctx.enter_function_scope(prototype->get_name());

    prototype->type_check(ctx);
    if(body)
    {
        body->type_check(ctx);
    }

    ctx.exit_named_scope(prototype->get_name());

    return std::nullopt;
}

std::string function_expression::to_string() const
{
    return std::format(
      "Function(prototype={}, body={})",
      prototype ? prototype->to_string() : std::string("<none>"),
      body ? body->to_string() : std::string("<none>"));
}

/*
 * call_expression.
 */

std::unique_ptr<expression> call_expression::clone() const
{
    return std::make_unique<call_expression>(*this);
}

void call_expression::serialize(archive& ar)
{
    super::serialize(ar);
    ar & callee;
    ar& expression_vector_serializer{args};
    ar& expression_serializer{index_expr};
    ar & return_type;
}

bool call_expression::is_pure([[maybe_unused]] cg::context& ctx) const
{
    // TODO Check in context. Functions from the current module can be checked,
    //      imported functions and native functions should be seen as impure.
    return false;
}

std::unique_ptr<cg::value> call_expression::generate_code(cg::context& ctx, memory_context mc) const
{
    // Code generation for function calls.
    if(mc == memory_context::store)
    {
        throw cg::codegen_error(loc, "Cannot store into call expression.");
    }

    for(const auto& arg: args)
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
        throw ty::type_error(
          callee.location,
          std::format(
            "Wrong number of arguments in function call. Expected {}, got {}.",
            sig.arg_types.size(),
            args.size()));
    }

    for(std::size_t i = 0; i < args.size(); ++i)
    {
        auto arg_type = args[i]->type_check(ctx);
        if(!arg_type.has_value())
        {
            throw ty::type_error(args[i]->get_location(), "Argument has no type.");
        }

        if(sig.arg_types[i] != *arg_type
           && !ctx.is_convertible(args[i]->get_location(), *arg_type, sig.arg_types[i]))
        {
            throw ty::type_error(
              args[i]->get_location(),
              std::format(
                "Type of argument {} does not match signature: Expected '{}', got '{}'.",
                i + 1,
                ty::to_string(sig.arg_types[i]),
                ty::to_string(*arg_type)));
        }
    }

    if(index_expr)
    {
        auto v = index_expr->type_check(ctx);
        if(!v.has_value())
        {
            throw ty::type_error(index_expr->get_location(), "Index expression has no type.");
        }

        if(ty::to_string(*v) != "i32")
        {
            throw ty::type_error(loc, "Expected <integer> for array element access.");
        }

        if(!sig.ret_type.is_array())
        {
            throw ty::type_error(loc, "Cannot use subscript on non-array type.");
        }

        expr_type = ctx.get_type(sig.ret_type.get_element_type()->to_string(), false);
    }
    else
    {
        expr_type = sig.ret_type;
    }

    ctx.set_expression_type(this, *expr_type);
    return expr_type;
}

std::string call_expression::to_string() const
{
    std::string ret = std::format("Call(callee={}, args=(", callee.s);
    if(!args.empty())
    {
        for(std::size_t i = 0; i < args.size() - 1; ++i)
        {
            ret += std::format("{}, ", args[i] ? args[i]->to_string() : std::string("<none>"));
        }
        ret += std::format("{}", args.back() ? args.back()->to_string() : std::string("<none>"));
    }
    ret += "))";
    return ret;
}

/*
 * return_statement.
 */

std::unique_ptr<expression> return_statement::clone() const
{
    return std::make_unique<return_statement>(*this);
}

void return_statement::serialize(archive& ar)
{
    super::serialize(ar);
    ar& expression_serializer{expr};
}

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
    if(!sig.has_value())
    {
        throw ty::type_error(loc, "Cannot have return statement outside a function.");
    }

    if(ty::to_string(sig->ret_type) == "void")
    {
        if(expr)
        {
            throw ty::type_error(
              loc,
              std::format(
                "Function '{}' declared as having no return value cannot have a return expression.",
                sig->name.s));
        }
    }
    else
    {
        auto ret_type = expr->type_check(ctx);
        if(!ret_type.has_value())
        {
            throw ty::type_error(loc, "Return expression has no type.");
        }

        if(!ret_type->is_resolved())
        {
            throw ty::type_error(
              loc,
              std::format(
                "Function '{}': Unresolved return type '{}'.",
                sig->name.s,
                ty::to_string(*ret_type)));
        }

        if(*ret_type != sig->ret_type)
        {
            throw ty::type_error(
              loc,
              std::format(
                "Function '{}': Return expression has type '{}', expected '{}'.",
                sig->name.s,
                ty::to_string(*ret_type),
                ty::to_string(sig->ret_type)));
        }
    }

    expr_type = sig->ret_type;
    ctx.set_expression_type(this, *expr_type);

    return expr_type;
}

std::string return_statement::to_string() const
{
    if(expr)
    {
        return std::format("Return(expr={})", expr->to_string());
    }

    return "Return()";
}

/*
 * if_statement.
 */

std::unique_ptr<expression> if_statement::clone() const
{
    return std::make_unique<if_statement>(*this);
}

void if_statement::serialize(archive& ar)
{
    super::serialize(ar);
    ar& expression_serializer{condition};
    ar& expression_serializer{if_block};
    ar& expression_serializer{else_block};
}

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
        throw cg::codegen_error(
          loc,
          std::format(
            "Expected if condition to be of type 'i32', got '{}",
            v->get_type().to_string()));
    }

    // store where to insert the branch.
    auto* function_insertion_point = ctx.get_insertion_point(true);

    // set up basic blocks.
    auto* if_basic_block = cg::basic_block::create(ctx, ctx.generate_label());
    cg::basic_block* else_basic_block = nullptr;
    auto* merge_basic_block = cg::basic_block::create(ctx, ctx.generate_label());

    bool if_ends_with_return = false;
    bool else_ends_with_return = false;

    // code generation for if block.
    ctx.get_current_function(true)->append_basic_block(if_basic_block);
    ctx.set_insertion_point(if_basic_block);
    if_block->generate_code(ctx, memory_context::none);
    if_ends_with_return = if_basic_block->ends_with_return();
    if(!if_ends_with_return)
    {
        ctx.generate_branch(merge_basic_block);
    }

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
        if(!else_ends_with_return)
        {
            ctx.generate_branch(merge_basic_block);
        }

        ctx.set_insertion_point(function_insertion_point);
        ctx.generate_cond_branch(if_basic_block, else_basic_block);
    }

    // emit merge block.
    if(!if_ends_with_return || !else_ends_with_return)
    {
        ctx.get_current_function(true)->append_basic_block(merge_basic_block);
        ctx.set_insertion_point(merge_basic_block);
    }
    else
    {
        // pick the last of the if/else blocks.
        if(else_block)
        {
            ctx.set_insertion_point(else_basic_block);
        }
        else
        {
            ctx.set_insertion_point(if_basic_block);
        }
    }

    return nullptr;
}

std::optional<ty::type_info> if_statement::type_check(ty::context& ctx)
{
    auto condition_type = condition->type_check(ctx);
    if(!condition_type.has_value())
    {
        throw ty::type_error(condition->get_location(), "Condition has no type.");
    }

    if(ty::to_string(*condition_type) != "i32")
    {
        throw ty::type_error(
          loc,
          std::format(
            "Expected if condition to be of type 'i32', got '{}",
            ty::to_string(*condition_type)));
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
    return std::format(
      "If(condition={}, if_block={}, else_block={})",
      condition ? condition->to_string() : std::string("<none>"),
      if_block ? if_block->to_string() : std::string("<none>"),
      else_block ? else_block->to_string() : std::string("<none>"));
}

/*
 * while_statement.
 */

std::unique_ptr<expression> while_statement::clone() const
{
    return std::make_unique<while_statement>(*this);
}

void while_statement::serialize(archive& ar)
{
    super::serialize(ar);
    ar& expression_serializer{condition};
    ar& expression_serializer{while_block};
}

std::unique_ptr<cg::value> while_statement::generate_code(cg::context& ctx, memory_context mc) const
{
    if(mc != memory_context::none)
    {
        throw cg::codegen_error(loc, "Invalid memory context for while_statement.");
    }

    // set up basic blocks.
    auto* while_loop_header_basic_block = cg::basic_block::create(ctx, ctx.generate_label());
    auto* while_loop_basic_block = cg::basic_block::create(ctx, ctx.generate_label());
    auto* merge_basic_block = cg::basic_block::create(ctx, ctx.generate_label());

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
        throw cg::codegen_error(
          loc,
          std::format(
            "Expected while condition to be of type 'i32', got '{}'.",
            v->get_type().to_string()));
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
    if(!condition_type.has_value())
    {
        throw ty::type_error(condition->get_location(), "Condition has no type.");
    }

    if(ty::to_string(*condition_type) != "i32")
    {
        throw ty::type_error(
          loc,
          std::format(
            "Expected while condition to be of type 'i32', got '{}",
            ty::to_string(*condition_type)));
    }

    ctx.enter_anonymous_scope(while_block->get_location());
    while_block->type_check(ctx);
    ctx.exit_anonymous_scope();

    return std::nullopt;
}

std::string while_statement::to_string() const
{
    return std::format(
      "While(condition={}, while_block={})",
      condition ? condition->to_string() : std::string("<none>"),
      while_block ? while_block->to_string() : std::string("<none>"));
}

/*
 * break_statement.
 */

std::unique_ptr<expression> break_statement::clone() const
{
    return std::make_unique<break_statement>(*this);
}

std::unique_ptr<cg::value> break_statement::generate_code(cg::context& ctx, [[maybe_unused]] memory_context mc) const
{
    auto [break_block, continue_block] = ctx.top_break_continue(loc);
    ctx.generate_branch(break_block);
    return nullptr;
}

/*
 * continue_statement.
 */

std::unique_ptr<expression> continue_statement::clone() const
{
    return std::make_unique<continue_statement>(*this);
}

std::unique_ptr<cg::value> continue_statement::generate_code(cg::context& ctx, [[maybe_unused]] memory_context mc) const
{
    auto [break_block, continue_block] = ctx.top_break_continue(loc);
    ctx.generate_branch(continue_block);
    return nullptr;
}

}    // namespace slang::ast
