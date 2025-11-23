/**
 * slang - a simple scripting language.
 *
 * abstract syntax tree.
 *
 * \author Felix Lubbe
 * \copyright Copyright (c) 2025
 * \license Distributed under the MIT software license (see accompanying LICENSE.txt).
 */

#pragma once

#include <optional>
#include <ranges>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

#include "archives/archive.h"
#include "compiler/collect.h"
#include "compiler/constant.h"
#include "compiler/directive.h"
#include "compiler/typing.h"
#include "node_ids.h"
#include "utils.h"

/*
 * Forward declarations for code generation.
 */
namespace slang::codegen
{
class context;
class function;
class type;
class value;
}    // namespace slang::codegen

/*
 * Forward references for macro collection / expansion.
 */

namespace slang::macro
{
struct env;
}    // namespace slang::macro

/*
 * Forward declarations for name resolution.
 */
namespace slang::resolve
{
class context;
}    // namespace slang::resolve

namespace slang::ast
{

namespace cg = slang::codegen;
namespace co = slang::collect;
namespace rs = slang::resolve;
namespace ty = slang::typing;

/**
 * The memory context for an expression.
 */
enum class memory_context
{
    none,  /** No explicit context. */
    load,  /** loading context. */
    store, /** storing context. */
};

/** Base class for all expression nodes. */
class expression
{
protected:
    /** The expression's location. */
    source_location loc;

    /** Namespace stack for the expression. */
    std::vector<std::string> namespace_stack;

    /** Scope id. */
    std::optional<sema::scope_id> scope_id;

    /** Symbol id. */
    std::optional<sema::symbol_id> symbol_id;

    /** The expression's given or inferred type. Needs to be set by `type_check`. */
    std::optional<ty::type_id> expr_type;

public:
    /** Default constructors. */
    expression() = default;
    expression(const expression&) = default;
    expression(expression&&) = default;

    /**
     * Construct an expression.
     *
     * @param loc The expression location.
     */
    expression(source_location loc)
    : loc{loc}
    {
    }

    /** Assignment operators. */
    expression& operator=(const expression&) = delete;
    expression& operator=(expression&&) = default;

    /** Default destructor. */
    virtual ~expression() = default;

    /** Get the node id. */
    [[nodiscard]]
    virtual node_identifier get_id() const
    {
        return node_identifier::expression;
    }

    /** Clone this expression. */
    [[nodiscard]]
    virtual std::unique_ptr<expression> clone() const;

    /**
     * Serialize the expression.
     *
     * @param ar The archive to use for serialization.
     */
    virtual void serialize(archive& ar);

    /** Whether this expression needs stack cleanup. */
    [[nodiscard]]
    virtual bool needs_pop() const
    {
        return false;
    }

    /** Whether this expression is a variable declaration. */
    [[nodiscard]]
    bool is_variable_declaration() const
    {
        return get_id() == node_identifier::variable_declaration_expression;
    }

    /**
     * Get the expression as a variable declaration.
     *
     * @throws Throws a `std::runtime_error` if the expression is not a variable declaration.
     */
    [[nodiscard]]
    virtual class variable_declaration_expression* as_variable_declaration();

    /** Whether this expression is a constant declaration. */
    [[nodiscard]]
    bool is_constant_declaration() const
    {
        return get_id() == node_identifier::constant_declaration_expression;
    }

    /**
     * Get the expression as a constant declaration.
     *
     * @throws Throws a `std::runtime_error` if the expression is not a constant declaration.
     */
    [[nodiscard]]
    virtual class constant_declaration_expression* as_constant_declaration();

    /**
     * Get the expression as a constant declaration.
     *
     * @throws Throws a `std::runtime_error` if the expression is not a constant declaration.
     */
    [[nodiscard]]
    virtual const class constant_declaration_expression* as_constant_declaration() const;

    /** Whether this expression is a variable reference. */
    [[nodiscard]]
    bool is_variable_reference() const
    {
        return get_id() == node_identifier::variable_reference_expression;
    }

    /**
     * Get the expression as a variable reference.
     *
     * @throws Throws a `std::runtime_error` if the expression is not a variable reference.
     */
    [[nodiscard]]
    virtual class variable_reference_expression* as_variable_reference();

    /** Whether this expression is an access of an array element. */
    [[nodiscard]]
    virtual bool is_array_element_access() const
    {
        return false;
    }

    /** Whether this expression is a struct member access. */
    [[nodiscard]]
    virtual bool is_struct_member_access() const
    {
        return false;
    }

    /** Whether this is a function call expression. */
    [[nodiscard]]
    virtual bool is_call_expression() const
    {
        return false;
    }

    /**
     * Get the expression as a call expression.
     *
     * @throws Throws a `std::runtime_error` if the expression is not a call expression.
     */
    virtual class call_expression* as_call_expression();

    /** Whether this expression is a macro expression. */
    [[nodiscard]]
    virtual bool is_macro_expression() const
    {
        return false;
    }

    /**
     * Get the expression as a macro expression.
     *
     * @throws Throws a `std::runtime_error` if the expression is not a macro expression.
     */
    virtual class macro_expression* as_macro_expression();

    /** Whether this expression is a macro branch. */
    [[nodiscard]]
    virtual bool is_macro_branch() const
    {
        return false;
    }

    /**
     * Get the expression as a macro branch.
     *
     * @throws Throws a `std::runtime_error` if the expression is not a macro branch.
     */
    [[nodiscard]]
    virtual class macro_branch* as_macro_branch();

    /**
     * Get the expression as a macro branch.
     *
     * @throws Throws a `std::runtime_error` if the expression is not a macro branch.
     */
    [[nodiscard]]
    virtual const class macro_branch* as_macro_branch() const;

    /** Whether the expression is a macro expression list. */
    [[nodiscard]]
    virtual bool is_macro_expression_list() const
    {
        return get_id() == node_identifier::macro_expression_list;
    }

    /**
     * Get the expression as a macro expression list.
     *
     * @throws Throws a `std::runtime_error` if the expression is not a macro expression list.
     */
    [[nodiscard]]
    virtual class macro_expression_list* as_macro_expression_list();

    /**
     * Get the expression as a macro expression list.
     *
     * @throws Throws a `std::runtime_error` if the expression is not a macro expression list.
     */
    [[nodiscard]]
    virtual const class macro_expression_list* as_macro_expression_list() const;

    /** Whether this expression is a macro invocation. */
    [[nodiscard]]
    virtual bool is_macro_invocation() const
    {
        return false;
    }

    /**
     * Get the expression as a macro invocation expression.
     *
     * @note Updates the expression's namespace path.
     * @throws Throws a `std::runtime_error` if the expression is not a macro invocation.
     */
    virtual class macro_invocation* as_macro_invocation();

    /** Whether this is a literal. */
    [[nodiscard]]
    bool is_literal() const
    {
        return get_id() == node_identifier::literal_expression;
    }

    /**
     * Get the expression as a literal expression.
     *
     * @throws Throws a `std::runtime_error` if the expression is not a literal expression.
     */
    virtual class literal_expression* as_literal();

    /**
     * Get the expression as a struct member access expression.
     *
     * @throws Throws a `std::runtime_error` if the expression is not a struct member access expression.
     */
    virtual class access_expression* as_access_expression();

    /**
     * Get the expression as a struct member access expression.
     *
     * @throws Throws a `std::runtime_error` if the expression is a struct member access expression.
     */
    [[nodiscard]]
    virtual const class access_expression* as_access_expression() const;

    /** Whether this expression is a `named_expression`. */
    [[nodiscard]]
    virtual bool is_named_expression() const
    {
        return false;
    }

    /**
     * Get the expression as a named expression.
     *
     * @throws Throws a `std::runtime_error` if the expression is not a named expression.
     */
    virtual class named_expression* as_named_expression();

    /**
     * Get the expression as a named expression.
     *
     * @throws Throws a `std::runtime_error` if the expression is not a named expression.
     */
    [[nodiscard]]
    virtual const class named_expression* as_named_expression() const;

    /**
     * Return whether this expression can be evaluated to a compile-time constant.
     *
     * @param env The constant evaluation environment.
     */
    [[nodiscard]]
    virtual bool is_const_eval([[maybe_unused]] const_::env& env) const
    {
        return false;
    }

    /**
     * Return whether this expression is pure, i.e. if it has no side effects.
     *
     * @param ctx The context in which to check.
     */
    [[nodiscard]]
    virtual bool is_pure(cg::context&) const
    {
        return false;
    }

    /**
     * Evaluate the compile-time constant.
     *
     * @param ctx The type context.
     * @param env The semantic environment.
     * @returns Returns the result of the evaluation, or a `nullptr` if evaluation failed.
     */
    virtual std::optional<const_::const_info> evaluate(
      [[maybe_unused]] ty::context& ctx,
      [[maybe_unused]] const_::env& env) const;

    /**
     * Generate IR.
     *
     * @param ctx The context to use for code generation.
     * @returns The value of this expression or nullptr.
     * @note The default implementation does not generate code.
     * @throws The default implementation throws `std::runtime_error`.
     */
    virtual std::unique_ptr<cg::value> generate_code(
      cg::context& ctx,
      memory_context mc = memory_context::none) const;

    /**
     * Name collection.
     *
     * @param ctx Collection context.
     */
    virtual void collect_names(co::context& ctx)
    {
        scope_id = ctx.get_current_scope();
    }

    /** Name resolution. */
    virtual void resolve_names([[maybe_unused]] rs::context& ctx)
    {
    }

    /** Collect attributes. */
    void collect_attributes(sema::env& env) const;

    /** Macro collection. */
    void collect_macros(sema::env& sema_env, macro::env& macro_env) const;

    /** Function declarations. */
    void declare_functions(ty::context& ctx, sema::env& env);

    /** Type declaration. */
    void declare_types(ty::context& ctx, sema::env& env);

    /** Type definition. */
    void define_types(ty::context& ctx) const;

    /** Bind constant declarations (no evaluation). */
    void bind_constant_declarations(sema::env& sema_env, const_::env& const_env) const;

    /** Constant evaluation and folding. */
    void evaluate_constant_expressions(
      ty::context& ctx,
      const_::env& env);

    /** Insert AST nodes for implicit casts after type checking. */
    void insert_implicit_casts(
      ty::context& ctx,
      sema::env& env);

    /**
     * Macro expansion (before type checking). Expands macro invocations by the corresponding
     * macro AST's while taking care of macro hygiene.
     *
     * @param co_ctx Name collection context.
     * @param rs_ctx Name resolution context.
     * @param codegen_ctx Code generation context.
     * @param macro_env Macro collection / expansion environment.
     * @param macro_asts The module's macros as AST's.
     * @returns `true` if macros were expanded and `false` if no macros were expanded.
     */
    bool expand_macros(
      co::context& co_ctx,
      rs::context& rs_ctx,
      cg::context& codegen_ctx,
      macro::env& macro_env,
      const std::vector<expression*>& macro_asts);

    /**
     * Macro expansion (after type checking). Calls the corresponding
     * function on the macro.
     *
     * @note Explicitly checks for the built-in `format!` macro, since this is
     *       the only macro using type information.
     *
     * @param co_ctx Name collection context.
     * @param rs_ctx Name resolution context.
     * @param type_ctx Type system context.
     * @param sema_env The semantic environment.
     */
    void expand_late_macros(
      co::context& co_ctx,
      rs::context& rs_ctx,
      ty::context& type_ctx,
      sema::env& env);

    /**
     * Get a directive. If the directive is not unique, a `codegen_error` is thrown.
     *
     * @param name The directive's name.
     * @returns The directive, or `std::nullopt` if the directive was not found.
     * @throws Throws a `codegen_error` if the directive is not unique.
     */
    std::optional<cg::directive> get_unique_directive(const std::string& s) const;

    /**
     * Set the namespace stack for the expression.
     *
     * @param stack The new namespace stack.
     */
    void set_namespace(std::vector<std::string> stack)
    {
        namespace_stack = std::move(stack);
    }

    /** Get the namespace stack. */
    [[nodiscard]]
    const std::vector<std::string>& get_namespace() const
    {
        return namespace_stack;
    }

    /** Return the namespace path, or `std::nullopt` if empty. */
    [[nodiscard]]
    virtual std::optional<std::string> get_namespace_path() const
    {
        if(namespace_stack.empty())
        {
            return std::nullopt;
        }

        return slang::utils::join(namespace_stack, "::");
    }

    /** Update namespace. */
    virtual void update_namespace()
    {
    }

    /**
     * Type checking.
     *
     * @throws A `type_error` if a type error was found.
     *
     * @param ctx Type system context.
     */
    virtual std::optional<ty::type_id> type_check(
      [[maybe_unused]] ty::context& ctx,
      [[maybe_unused]] sema::env& env)
    {
        return std::nullopt;
    }

    /** Get a readable string representation of the node. */
    [[nodiscard]]
    virtual std::string to_string() const;

    /** Get the expression's location. */
    [[nodiscard]]
    const source_location& get_location() const
    {
        return loc;
    }

    /** Get all child nodes. */
    [[nodiscard]]
    virtual std::vector<expression*> get_children()
    {
        return {};
    }

    /** Get all child nodes. */
    [[nodiscard]]
    virtual std::vector<const expression*> get_children() const
    {
        return {};
    }

    /** Get the scope id, or `std::nullopt`. */
    [[nodiscard]]
    std::optional<sema::symbol_id> get_scope_id() const
    {
        return scope_id;
    }

    /** Get the symbol id, or `std::nullopt`. */
    [[nodiscard]]
    std::optional<sema::symbol_id> get_symbol_id() const
    {
        return symbol_id;
    }

    /**
     * Visit all nodes in this expression tree using pre-order or post-order traversal.
     *
     * @param visitor The visitor function.
     * @param visit_self Whether to visit the current node.
     * @param post_order Whether to visit the nodes in post-order. Default is pre-order.
     * @param filter An optional filter that returns `true` if a node
     *               should be traversed. Defaults to traversing all nodes.
     * @throws Throws a `std::runtime_error` if any child node is `nullptr`.
     */
    void visit_nodes(
      std::function<void(expression&)> visitor,
      bool visit_self,
      bool post_order = false,
      const std::function<bool(const expression&)>& filter = nullptr);

    /**
     * Visit all nodes in this expression tree using pre-order or post-order traversal.
     *
     * @param visitor The visitor function.
     * @param visit_self Whether to visit the current node.
     * @param post_order Whether to visit the nodes in post-order. Default is pre-order.
     * @param filter An optional filter that returns `true` if a node
     *               should be traversed. Defaults to traversing all nodes.
     * @throws Throws a `std::runtime_error` if any child node is `nullptr`.
     */
    void visit_nodes(
      std::function<void(const expression&)> visitor,
      bool visit_self,
      bool post_order = false,
      const std::function<bool(const expression&)>& filter = nullptr) const;
};

/**
 * `expression` serializer.
 *
 * @param ar The archive to use.
 * @param expr The expression to serialize.
 * @return Returns the input archive.
 */
inline archive& operator&(archive& ar, expression& expr)
{
    expr.serialize(ar);
    return ar;
}

/** Any expression with a name. */
class named_expression : public expression
{
    friend class macro_expression;

protected:
    /** The expression name. */
    token name;

public:
    /** Set the super class. */
    using super = expression;

    /** Default constructors. */
    named_expression() = default;
    named_expression(const named_expression&) = default;
    named_expression(named_expression&&) = default;

    /** Assignment operators. */
    named_expression& operator=(const named_expression&) = delete;
    named_expression& operator=(named_expression&&) = default;

    /**
     * Construct a named expression.
     *
     * @param loc The location.
     * @param tok The name token.
     */
    named_expression(source_location loc, token name)
    : expression{loc}
    , name{std::move(name)}
    {
    }

    [[nodiscard]]
    node_identifier get_id() const override
    {
        return node_identifier::named_expression;
    }

    [[nodiscard]] std::unique_ptr<expression> clone() const override;
    void serialize(archive& ar) override;

    [[nodiscard]] bool is_named_expression() const override
    {
        return true;
    }

    [[nodiscard]]
    named_expression* as_named_expression() override
    {
        return this;
    }

    [[nodiscard]]
    const named_expression* as_named_expression() const override
    {
        return this;
    }

    /** Get the name. */
    [[nodiscard]]
    const std::string& get_name() const
    {
        return name.s;
    }

    /** Get the name token. */
    const token& get_name_token() const
    {
        return name;
    }

    /** Get the name including namespaces. */
    [[nodiscard]]
    std::string get_qualified_name() const;
};

/** String, integer or floating-point literals. */
class literal_expression : public expression
{
    /** The literal token. */
    token tok;

public:
    /** Set the super class. */
    using super = expression;

    /** Default constructors. */
    literal_expression() = default;
    literal_expression(const literal_expression&) = default;
    literal_expression(literal_expression&&) = default;

    /** Assignment operators. */
    literal_expression& operator=(const literal_expression&) = delete;
    literal_expression& operator=(literal_expression&&) = default;

    /**
     * Construct a literal expression.
     *
     * @param loc The location.
     * @param tok The token.
     */
    literal_expression(source_location loc, token tok)
    : expression{loc}
    , tok{std::move(tok)}
    {
    }

    [[nodiscard]]
    node_identifier get_id() const override
    {
        return node_identifier::literal_expression;
    }

    [[nodiscard]] std::unique_ptr<expression> clone() const override;
    void serialize(archive& ar) override;

    [[nodiscard]] bool needs_pop() const override
    {
        return true;
    }

    [[nodiscard]]
    bool is_const_eval(const_::env&) const override
    {
        return true;
    }

    [[nodiscard]]
    bool is_pure(cg::context&) const override
    {
        return true;
    }

    [[nodiscard]]
    literal_expression* as_literal() override
    {
        return this;
    }

    virtual std::optional<const_::const_info> evaluate(
      ty::context& ctx,
      const_::env& env) const override;

    std::unique_ptr<cg::value> generate_code(cg::context& ctx, memory_context mc = memory_context::none) const override;
    std::optional<ty::type_id> type_check(ty::context& ctx, sema::env& env) override;
    [[nodiscard]] std::string to_string() const override;

    /** Get the token. */
    const token& get_token() const
    {
        return tok;
    }
};

/** A type expression. */
class type_expression
{
    /** Location. */
    source_location loc;

    /** The type name. */
    token type_name;

    /** Type id. */
    ty::type_id type_id;

    /** Namespaces. */
    std::vector<token> namespace_stack;

    /** Whether the type is an array type. */
    bool array{false};

public:
    /** Default constructors. */
    type_expression() = default;
    type_expression(const type_expression&) = default;
    type_expression(type_expression&&) = default;

    /** Assignment operators. */
    type_expression& operator=(const type_expression&) = delete;
    type_expression& operator=(type_expression&&) = default;

    /**
     * Construct a `type_expression`.
     *
     * @param loc The location.
     * @param type_name The (unqualified) type name.
     * @param namespace_stack Namespaces for type resolution.
     * @param is_array Whether the type is an array type.
     */
    type_expression(source_location loc, token type_name, std::vector<token> namespace_stack, bool is_array)
    : loc{loc}
    , type_name{std::move(type_name)}
    , namespace_stack{std::move(namespace_stack)}
    , array{is_array}
    {
    }

    [[nodiscard]] std::unique_ptr<type_expression> clone() const;
    void serialize(archive& ar);

    void resolve_names(rs::context& ctx);
    void type_check(ty::context& ctx, sema::env& env);

    /** Get the location. */
    [[nodiscard]]
    source_location get_location() const
    {
        return loc;
    }

    /** Returns the type name. */
    [[nodiscard]]
    std::string get_name() const
    {
        return type_name.s;
    }

    /**
     * Return the qualified type name, that is, the type name with its namespace path
     * prepended, if not empty.
     */
    [[nodiscard]] std::string get_qualified_name() const;

    /** Return the namespace path, or `std::nullopt` if empty. */
    std::optional<std::string> get_namespace_path() const;

    /** Return a readable representation of the type. */
    [[nodiscard]] std::string to_string() const;

    /** Get the type id. */
    [[nodiscard]] ty::type_id get_type() const
    {
        return type_id;
    }

    /** Return whether the type is an array. */
    [[nodiscard]]
    bool is_array() const
    {
        return array;
    }
};

/**
 * `type_expression` serializer.
 *
 * @param ar The archive to use.
 * @param expr The type expression to serialize.
 * @return Returns the input archive.
 */
inline archive& operator&(archive& ar, type_expression& expr)
{
    expr.serialize(ar);
    return ar;
}

/** A type cast expression. */
class type_cast_expression : public expression
{
    /** The expression. */
    std::unique_ptr<expression> expr;

    /** The target type. */
    std::unique_ptr<type_expression> target_type;

    /** Whether to always perform the cast, e.g. when making the implicit casts explicit. */
    bool always_cast{false};

public:
    /** Set the super class. */
    using super = expression;

    /** Default constructor. */
    type_cast_expression() = default;

    /** Copy and move constructors. */
    type_cast_expression(const type_cast_expression& other)
    : super{other}
    , expr{other.expr->clone()}
    , target_type{other.target_type->clone()}
    , always_cast{other.always_cast}
    {
    }
    type_cast_expression(type_cast_expression&&) = default;

    /** Assignment operators. */
    type_cast_expression& operator=(const type_cast_expression&) = delete;
    type_cast_expression& operator=(type_cast_expression&&) = default;

    /**
     * Construct a type cast expression.
     *
     * @param loc The location
     * @param expr The expression.
     * @param target_type The target type.
     * @param always_cast Whether to always insert the cast.
     */
    type_cast_expression(
      source_location loc,
      std::unique_ptr<expression> expr,
      std::unique_ptr<type_expression> target_type,
      bool always_cast = false)
    : expression{loc}
    , expr{std::move(expr)}
    , target_type{std::move(target_type)}
    , always_cast{always_cast}
    {
    }

    [[nodiscard]]
    node_identifier get_id() const override
    {
        return node_identifier::type_cast_expression;
    }

    [[nodiscard]] std::unique_ptr<expression> clone() const override;
    void serialize(archive& ar) override;

    [[nodiscard]]
    bool is_struct_member_access() const override
    {
        return expr->is_struct_member_access();
    }

    [[nodiscard]]
    access_expression* as_access_expression() override
    {
        return expr->as_access_expression();
    }

    [[nodiscard]]
    const access_expression* as_access_expression() const override
    {
        return expr->as_access_expression();
    }

    [[nodiscard]]
    bool is_named_expression() const override
    {
        return expr->is_named_expression();
    }

    [[nodiscard]]
    named_expression* as_named_expression() override
    {
        return expr->as_named_expression();
    }

    [[nodiscard]]
    const named_expression* as_named_expression() const override
    {
        return expr->as_named_expression();
    }

    [[nodiscard]]
    bool is_const_eval(const_::env& env) const override
    {
        return expr->is_const_eval(env);
    }

    [[nodiscard]]
    bool is_pure(cg::context& ctx) const override
    {
        return expr->is_pure(ctx);
    }

    virtual std::optional<const_::const_info> evaluate(
      ty::context& ctx,
      const_::env& env) const override;

    std::unique_ptr<cg::value> generate_code(cg::context& ctx, memory_context mc = memory_context::none) const override;
    void collect_names(co::context& ctx) override;
    void resolve_names(rs::context& ctx) override;
    std::optional<ty::type_id> type_check(ty::context& ctx, sema::env& env) override;
    [[nodiscard]] std::string to_string() const override;

    [[nodiscard]]
    std::vector<expression*> get_children() override
    {
        return {expr.get()};
    }
    [[nodiscard]]
    std::vector<const expression*> get_children() const override
    {
        return {expr.get()};
    }
};

/** Namespace access expression. */
class namespace_access_expression : public named_expression
{
    /** The remaining expression. */
    std::unique_ptr<expression> expr;

public:
    /** Set the super class. */
    using super = named_expression;

    /** Defaulted and deleted constructors. */
    namespace_access_expression() = default;
    namespace_access_expression(const namespace_access_expression& other)
    : super{other}
    , expr{other.expr->clone()}
    {
    }
    namespace_access_expression(namespace_access_expression&&) = default;

    /** Assignment operators. */
    namespace_access_expression& operator=(const namespace_access_expression&) = delete;
    namespace_access_expression& operator=(namespace_access_expression&&) = default;

    /**
     * Construct a scope expression.
     *
     * @param name The scope's name.
     * @param expr An expression.
     */
    namespace_access_expression(
      token name,
      std::unique_ptr<expression> expr)
    : named_expression{name.location, std::move(name)}
    , expr{std::move(expr)}
    {
    }

    [[nodiscard]]
    node_identifier get_id() const override
    {
        return node_identifier::namespace_access_expression;
    }

    [[nodiscard]] std::unique_ptr<expression> clone() const override;
    void serialize(archive& ar) override;

    [[nodiscard]]
    bool needs_pop() const override
    {
        return expr->needs_pop();
    }

    [[nodiscard]]
    bool is_call_expression() const override
    {
        return expr->is_call_expression();
    }

    [[nodiscard]]
    call_expression* as_call_expression() override
    {
        update_namespace();
        return expr->as_call_expression();
    }

    [[nodiscard]]
    bool is_macro_invocation() const override
    {
        return expr->is_macro_invocation();
    }

    [[nodiscard]]
    macro_invocation* as_macro_invocation() override
    {
        update_namespace();
        return expr->as_macro_invocation();
    }

    [[nodiscard]]
    bool is_const_eval(const_::env& env) const override
    {
        return expr->is_const_eval(env);
    }

    [[nodiscard]]
    bool is_pure(cg::context& ctx) const override
    {
        return expr->is_pure(ctx);
    }

    virtual std::optional<const_::const_info> evaluate(
      ty::context& ctx,
      const_::env& env) const override;

    std::unique_ptr<cg::value> generate_code(cg::context& ctx, memory_context mc = memory_context::none) const override;
    void resolve_names(rs::context& ctx) override;

    std::optional<std::string> get_namespace_path() const override
    {
        return expr->get_namespace_path();
    }

    void update_namespace() override
    {
        auto expr_namespace_stack = namespace_stack;
        expr_namespace_stack.push_back(name.s);
        expr->set_namespace(std::move(expr_namespace_stack));
    }

    void collect_names(co::context& ctx) override;
    std::optional<ty::type_id> type_check(ty::context& ctx, sema::env& env) override;
    [[nodiscard]] std::string to_string() const override;

    [[nodiscard]]
    std::vector<expression*> get_children() override
    {
        return expr->get_children();
    }
    [[nodiscard]]
    std::vector<const expression*> get_children() const override
    {
        std::vector<const expression*> exprs;
        exprs.reserve(expr->get_children().size());
        for(auto& c: expr->get_children())
        {
            exprs.emplace_back(c);
        }
        return exprs;
    }

    /** Return the contained expression. */
    std::unique_ptr<expression>& get_expr()
    {
        return expr;
    }
};

/** Access expression. */
class access_expression : public expression
{
    friend class macro_expression;

    /** Left and right hand sides. */
    std::unique_ptr<expression> lhs, rhs;

    /** Struct type of the l.h.s. Set during type checking. */
    std::optional<ty::type_id> struct_type;

    /** Struct info. Set during type checking. */
    std::optional<ty::struct_info> struct_info;

    /** Field index. */
    std::size_t field_index{0};

    /** Whether the l.h.s. is an array type. */
    bool lhs_is_array{false};

public:
    /** Set the super class. */
    using super = expression;

    /** Defaulted and deleted constructors. */
    access_expression() = default;
    access_expression(const access_expression& other)
    : super{other}
    , lhs{other.lhs->clone()}
    , rhs{other.rhs->clone()}
    , struct_type{other.struct_type}
    {
    }
    access_expression(access_expression&&) = default;

    /** Assignment operators. */
    access_expression& operator=(const access_expression&) = delete;
    access_expression& operator=(access_expression&&) = default;

    /**
     * Construct an access expression.
     *
     * @param lhs Left-hand side of the access expression.
     * @param rhs Right-hand side of the access expression.
     */
    access_expression(std::unique_ptr<expression> lhs, std::unique_ptr<expression> rhs);

    [[nodiscard]]
    node_identifier get_id() const override
    {
        return node_identifier::access_expression;
    }

    [[nodiscard]] std::unique_ptr<expression> clone() const override;
    void serialize(archive& ar) override;

    [[nodiscard]]
    bool is_struct_member_access() const override
    {
        return true;
    }

    [[nodiscard]]
    virtual access_expression* as_access_expression() override
    {
        return this;
    }

    [[nodiscard]]
    virtual const access_expression* as_access_expression() const override
    {
        return this;
    }

    [[nodiscard]]
    bool is_pure(cg::context& ctx) const override
    {
        return lhs->is_pure(ctx) && rhs->is_pure(ctx);
    }

    /**
     * Generate IR.
     *
     * @param ctx The context to use for code generation.
     * @returns The value of this expression or nullptr.
     * @note When called with `memory_context::store`, the object must be loaded onto the stack by
     *       calling `generate_object_load` beforehand.
     */
    std::unique_ptr<cg::value> generate_code(cg::context& ctx, memory_context mc = memory_context::none) const override;
    void collect_names(co::context& ctx) override;
    void resolve_names(rs::context& ctx) override;
    std::optional<ty::type_id> type_check(ty::context& ctx, sema::env& env) override;
    [[nodiscard]] std::string to_string() const override;

    /** Get the left-hand side expression. */
    [[nodiscard]]
    expression* get_left_expression()
    {
        return lhs.get();
    }

    /** Get the left-hand side expression. */
    [[nodiscard]]
    const expression* get_left_expression() const
    {
        return lhs.get();
    }

    /**
     * Return the accessed struct's type info.
     *
     * @throws Throws a `type_error` if the struct type was not set.
     * @returns The struct type.
     */
    [[nodiscard]]
    ty::type_id get_struct_type() const;

    /** Get struct info. */
    [[nodiscard]]
    const std::optional<ty::struct_info>& get_struct_info() const
    {
        return struct_info;
    }

    /** Get the field index. */
    [[nodiscard]]
    std::size_t get_field_index() const
    {
        return field_index;
    }
};

/** Import statements. */
class import_expression : public expression
{
    /** The import path. */
    std::vector<token> path;

public:
    /** Set the super class. */
    using super = expression;

    /** Default constructor. */
    import_expression() = default;

    /** Copy and move constructors. */
    import_expression(const import_expression& other)
    : super{other}
    , path{other.path}
    {
    }
    import_expression(import_expression&&) = default;

    /** Assignment operators. */
    import_expression& operator=(const import_expression&) = delete;
    import_expression& operator=(import_expression&&) = default;

    /**
     * Construct an import expression.
     *
     * @param path The import path.
     */
    explicit import_expression(std::vector<token> path)
    : expression{path[0].location}
    , path{std::move(path)}
    {
    }

    [[nodiscard]]
    node_identifier get_id() const override
    {
        return node_identifier::import_expression;
    }

    [[nodiscard]] std::unique_ptr<expression> clone() const override;
    void serialize(archive& ar) override;

    std::unique_ptr<cg::value> generate_code(cg::context& ctx, memory_context mc = memory_context::none) const override;
    void collect_names(co::context& ctx) override;
    [[nodiscard]] std::string to_string() const override;
};

/** Directives. Directives have names and contain a list of key-value pairs as arguments. */
class directive_expression : public named_expression
{
    /** Key-value pairs. */
    std::vector<std::pair<token, token>> args;

    /** An expression the directive applies to. */
    std::unique_ptr<expression> expr;

public:
    /** Set the super class. */
    using super = named_expression;

    /** Default constructor. */
    directive_expression() = default;

    /** Copy and move constructors. */
    directive_expression(const directive_expression& other)
    : super{other}
    , args{other.args}
    , expr{other.expr->clone()}
    {
    }
    directive_expression(directive_expression&&) = default;

    /** Assignment operators. */
    directive_expression& operator=(const directive_expression&) = delete;
    directive_expression& operator=(directive_expression&&) = default;

    /**
     * Construct a directive expression.
     *
     * @param name The name.
     * @param args The directive's arguments as key-value pairs.
     * @param expr The expression the directive applies to.
     */
    directive_expression(token name, std::vector<std::pair<token, token>> args, std::unique_ptr<expression> expr)
    : named_expression{name.location, std::move(name)}
    , args{std::move(args)}
    , expr{std::move(expr)}
    {
    }

    [[nodiscard]]
    node_identifier get_id() const override
    {
        return node_identifier::directive_expression;
    }

    [[nodiscard]] std::unique_ptr<expression> clone() const override;
    void serialize(archive& ar) override;

    [[nodiscard]]
    bool needs_pop() const override
    {
        return expr->needs_pop();
    }

    [[nodiscard]]
    bool is_macro_expression() const override
    {
        return expr->is_macro_expression();
    }

    std::unique_ptr<cg::value> generate_code(cg::context& ctx, memory_context mc = memory_context::none) const override;
    void collect_names(co::context& ctx) override;
    void resolve_names(rs::context& ctx) override;
    std::optional<ty::type_id> type_check(ty::context& ctx, sema::env& env) override;
    [[nodiscard]] std::string to_string() const override;

    [[nodiscard]]
    std::vector<expression*> get_children() override
    {
        return {expr.get()};
    }
    [[nodiscard]]
    std::vector<const expression*> get_children() const override
    {
        return {expr.get()};
    }

    /** Return the target expression. */
    expression* get_target();

    /** Return the target expression. */
    const expression* get_target() const;

    /** Get the arguments. */
    const std::vector<std::pair<token, token>>& get_args() const
    {
        return args;
    }
};

/** Variable references. */
class variable_reference_expression : public named_expression
{
    friend class macro_expression;

    /** An optional expression for element access. */
    std::unique_ptr<expression> element_expr;

    /** The array type. Only valid for arrays. */
    std::optional<ty::type_id> array_type;

    /** Expansions for macro names. */
    std::unique_ptr<expression> expansion;

public:
    /** Set the super class. */
    using super = named_expression;

    /** Defaulted and deleted constructors. */
    variable_reference_expression() = default;
    variable_reference_expression(const variable_reference_expression& other)
    : super{other}
    , element_expr{other.element_expr ? other.element_expr->clone() : nullptr}
    , expansion{other.expansion ? other.expansion->clone() : nullptr}
    {
    }
    variable_reference_expression(variable_reference_expression&&) = default;

    /** Assignment operators. */
    variable_reference_expression& operator=(const variable_reference_expression&) = delete;
    variable_reference_expression& operator=(variable_reference_expression&&) = default;

    /**
     * Construct a variable reference expression.
     *
     * @param name The variable's name.
     * @param element_expr An optional expression for array element access.
     * @param expansion A macro expansion.
     */
    variable_reference_expression(
      token name,
      std::unique_ptr<expression> element_expr = nullptr,
      std::unique_ptr<expression> expansion = nullptr)
    : named_expression{name.location, std::move(name)}
    , element_expr{std::move(element_expr)}
    , expansion{std::move(expansion)}
    {
    }

    [[nodiscard]]
    node_identifier get_id() const override
    {
        return node_identifier::variable_reference_expression;
    }

    [[nodiscard]] std::unique_ptr<expression> clone() const override;
    void serialize(archive& ar) override;

    [[nodiscard]] bool needs_pop() const override
    {
        return true;
    }

    [[nodiscard]]
    variable_reference_expression* as_variable_reference() override
    {
        return this;
    }

    [[nodiscard]]
    bool is_array_element_access() const override
    {
        return element_expr != nullptr;
    }

    [[nodiscard]]
    expression* get_element_expression()
    {
        return element_expr.get();
    }

    [[nodiscard]] bool is_const_eval(const_::env& env) const override;

    [[nodiscard]]
    bool is_pure(cg::context&) const override
    {
        return true;
    }

    virtual std::optional<const_::const_info> evaluate(
      ty::context& ctx,
      const_::env& env) const override;

    std::unique_ptr<cg::value> generate_code(cg::context& ctx, memory_context mc = memory_context::none) const override;
    void collect_names(co::context& ctx) override;
    void resolve_names(rs::context& ctx) override;
    std::optional<ty::type_id> type_check(ty::context& ctx, sema::env& env) override;
    [[nodiscard]] std::string to_string() const override;

    [[nodiscard]]
    std::vector<expression*> get_children() override
    {
        if(element_expr && expansion)
        {
            return {element_expr.get(), expansion.get()};
        }
        else if(element_expr)
        {
            return {element_expr.get()};
        }
        else if(expansion)
        {
            return {expansion.get()};
        }
        return {};
    }
    [[nodiscard]]
    std::vector<const expression*> get_children() const override
    {
        if(element_expr && expansion)
        {
            return {element_expr.get(), expansion.get()};
        }
        else if(element_expr)
        {
            return {element_expr.get()};
        }
        else if(expansion)
        {
            return {expansion.get()};
        }
        return {};
    }

    /** Whether this variable was expanded by an expression. */
    [[nodiscard]]
    bool has_expansion() const
    {
        return expansion != nullptr;
    }

    /** Get the expansion. The expansion can be `nullptr`. */
    const std::unique_ptr<expression>& get_expansion() const
    {
        return expansion;
    }

    /**
     * Set the expansion.
     *
     * @param exp The expansion.
     */
    void set_expansion(std::unique_ptr<expression> exp)
    {
        expansion = std::move(exp);
    }

    /** Get the array type, or `std::nullopt` for non-arrays. */
    [[nodiscard]]
    std::optional<ty::type_id> get_array_type() const
    {
        return array_type;
    }
};

/** Variable declaration. */
class variable_declaration_expression : public named_expression
{
    friend class macro_expression;

    /** The variable's type. */
    std::unique_ptr<type_expression> type;

    /** An optional initializer expression. */
    std::unique_ptr<expression> expr;

public:
    /** Set the super class. */
    using super = named_expression;

    /** Defaulted and deleted constructors. */
    variable_declaration_expression() = default;
    variable_declaration_expression(const variable_declaration_expression& other)
    : super{other}
    , type{other.type->clone()}
    , expr{other.expr ? other.expr->clone() : nullptr}
    {
    }
    variable_declaration_expression(variable_declaration_expression&&) = default;

    /** Assignment operators. */
    variable_declaration_expression& operator=(const variable_declaration_expression&) = delete;
    variable_declaration_expression& operator=(variable_declaration_expression&&) = default;

    /**
     * Construct a variable expression.
     *
     * @param loc The location.
     * @param name The variable's name.
     * @param type The variable's type.
     * @param expr An optional initializer expression.
     */
    variable_declaration_expression(
      source_location loc,
      token name,
      std::unique_ptr<type_expression> type,
      std::unique_ptr<expression> expr)
    : named_expression{loc, std::move(name)}
    , type{std::move(type)}
    , expr{std::move(expr)}
    {
    }

    [[nodiscard]]
    node_identifier get_id() const override
    {
        return node_identifier::variable_declaration_expression;
    }

    [[nodiscard]] std::unique_ptr<expression> clone() const override;
    void serialize(archive& ar) override;

    [[nodiscard]]
    variable_declaration_expression* as_variable_declaration() override
    {
        return this;
    }

    std::unique_ptr<cg::value> generate_code(cg::context& ctx, memory_context mc = memory_context::none) const override;
    void collect_names(co::context& ctx) override;
    void resolve_names(rs::context& ctx) override;
    std::optional<ty::type_id> type_check(ty::context& ctx, sema::env& env) override;
    [[nodiscard]] std::string to_string() const override;

    [[nodiscard]]
    std::vector<expression*> get_children() override
    {
        if(expr)
        {
            return {expr.get()};
        }
        return {};
    }
    [[nodiscard]]
    std::vector<const expression*> get_children() const override
    {
        if(expr)
        {
            return {expr.get()};
        }
        return {};
    }

    /** Get the variable's type. */
    [[nodiscard]]
    const std::unique_ptr<type_expression>& get_type() const
    {
        return type;
    }

    /** Whether this variable is an array. */
    [[nodiscard]]
    bool is_array() const
    {
        return type->is_array();
    }

    /**
     * Insert AST nodes for implicit casts after type checking.
     *
     * @param ctx The type context.
     * @param env Semantic environment.
     */
    void insert_implicit_casts(
      ty::context& ctx,
      sema::env& env);
};

/** Constant declaration. */
class constant_declaration_expression : public named_expression
{
    /** The constant's type. */
    std::unique_ptr<type_expression> type;

    /** The initializer expression. */
    std::unique_ptr<expression> expr;

public:
    /** Set the super class. */
    using super = named_expression;

    /** Defaulted and deleted constructors. */
    constant_declaration_expression() = default;
    constant_declaration_expression(const constant_declaration_expression& other)
    : super{other}
    , type{other.type->clone()}
    , expr{other.expr->clone()}
    {
    }
    constant_declaration_expression(constant_declaration_expression&&) = default;

    /** Assignment operators. */
    constant_declaration_expression& operator=(const constant_declaration_expression&) = delete;
    constant_declaration_expression& operator=(constant_declaration_expression&&) = default;

    /**
     * Construct a constant expression.
     *
     * @param loc The location.
     * @param name The constant's name.
     * @param type The constant's type.
     * @param expr The initializer expression.
     */
    constant_declaration_expression(
      source_location loc,
      token name,
      std::unique_ptr<type_expression> type,
      std::unique_ptr<expression> expr)
    : named_expression{loc, std::move(name)}
    , type{std::move(type)}
    , expr{std::move(expr)}
    {
    }

    [[nodiscard]]
    node_identifier get_id() const override
    {
        return node_identifier::constant_declaration_expression;
    }

    [[nodiscard]] std::unique_ptr<expression> clone() const override;
    void serialize(archive& ar) override;

    [[nodiscard]]
    constant_declaration_expression* as_constant_declaration() override
    {
        return this;
    }

    [[nodiscard]]
    const constant_declaration_expression* as_constant_declaration() const override
    {
        return this;
    }

    std::unique_ptr<cg::value> generate_code(cg::context& ctx, memory_context mc = memory_context::none) const override;
    void collect_names(co::context& ctx) override;
    void resolve_names(rs::context& ctx) override;
    std::optional<ty::type_id> type_check(ty::context& ctx, sema::env& env) override;
    [[nodiscard]] std::string to_string() const override;

    [[nodiscard]]
    std::vector<expression*> get_children() override
    {
        return {expr.get()};
    }
    [[nodiscard]]
    std::vector<const expression*> get_children() const override
    {
        return {expr.get()};
    }

    /** Get the constant's type. */
    [[nodiscard]]
    const std::unique_ptr<type_expression>& get_type() const
    {
        return type;
    }

    /** Get the r.h.s. expression. */
    [[nodiscard]]
    const expression* get_expr() const
    {
        return expr.get();
    }
};

/** An array initializer expression. */
class array_initializer_expression : public expression
{
    /** Initializer expressions for each element. */
    std::vector<std::unique_ptr<expression>> exprs;

public:
    /** Set the super class. */
    using super = expression;

    /** Default constructor. */
    array_initializer_expression() = default;

    /** Copy and move constructors. */
    array_initializer_expression(const array_initializer_expression& other)
    : super{other}
    , exprs{other.exprs
            | std::views::transform([](const auto& ptr)
                                    { return ptr->clone(); })
            | std::ranges::to<std::vector>()}
    {
    }
    array_initializer_expression(array_initializer_expression&&) = default;

    /** Assignment operators. */
    array_initializer_expression& operator=(const array_initializer_expression&) = delete;
    array_initializer_expression& operator=(array_initializer_expression&&) = default;

    /**
     * Construct an array initializer expression.
     */
    array_initializer_expression(
      source_location loc,
      std::vector<std::unique_ptr<expression>> exprs)
    : expression{loc}
    , exprs{std::move(exprs)}
    {
    }

    [[nodiscard]]
    node_identifier get_id() const override
    {
        return node_identifier::array_initializer_expression;
    }

    [[nodiscard]] std::unique_ptr<expression> clone() const override;
    void serialize(archive& ar) override;

    [[nodiscard]]
    bool is_pure(cg::context&) const override
    {
        return true;
    }

    std::unique_ptr<cg::value> generate_code(cg::context& ctx, memory_context mc = memory_context::none) const override;
    void collect_names(co::context& ctx) override;
    void resolve_names(rs::context& ctx) override;
    std::optional<ty::type_id> type_check(ty::context& ctx, sema::env& env) override;
    [[nodiscard]] std::string to_string() const override;

    [[nodiscard]]
    std::vector<expression*> get_children() override
    {
        std::vector<expression*> children;
        children.reserve(exprs.size());

        for(auto& e: exprs)
        {
            children.emplace_back(e.get());
        }
        return children;
    }
    [[nodiscard]]
    std::vector<const expression*> get_children() const override
    {
        std::vector<const expression*> children;
        children.reserve(exprs.size());

        for(auto& e: exprs)
        {
            children.emplace_back(e.get());
        }
        return children;
    }
};

/** Struct definition. */
class struct_definition_expression : public named_expression
{
    /** The struct's members. */
    std::vector<std::unique_ptr<variable_declaration_expression>> members;

    /** Type id filled in after type declaration. */
    ty::type_id struct_type_id;

public:
    /** Set the super class. */
    using super = named_expression;

    /** Default constructor. */
    struct_definition_expression() = default;

    /** Copy and move constructors. */
    struct_definition_expression(const struct_definition_expression& other)
    : super{other}
    , members{other.members
              | std::views::transform(
                [](const auto& ptr)
                { return std::unique_ptr<variable_declaration_expression>(
                    static_cast<variable_declaration_expression*>(    // NOLINT(cppcoreguidelines-pro-type-static-cast-downcast)
                      ptr->clone().release())); })
              | std::ranges::to<std::vector>()}
    {
    }
    struct_definition_expression(struct_definition_expression&&) = default;

    /** Assignment operators. */
    struct_definition_expression& operator=(const struct_definition_expression&) = delete;
    struct_definition_expression& operator=(struct_definition_expression&&) = default;

    /**
     * Construct a struct definition.
     *
     * @param loc The location.
     * @param name The struct's name.
     * @param members The struct's members.
     */
    struct_definition_expression(
      source_location loc,
      token name,
      std::vector<std::unique_ptr<variable_declaration_expression>> members)
    : named_expression{loc, std::move(name)}
    , members{std::move(members)}
    {
    }

    [[nodiscard]]
    node_identifier get_id() const override
    {
        return node_identifier::struct_definition_expression;
    }

    [[nodiscard]] std::unique_ptr<expression> clone() const override;
    void serialize(archive& ar) override;

    void collect_names(co::context& ctx) override;
    void resolve_names(rs::context& ctx) override;
    std::optional<ty::type_id> type_check(ty::context& ctx, sema::env& env) override;
    [[nodiscard]] std::string to_string() const override;

    [[nodiscard]]
    std::vector<expression*> get_children() override
    {
        std::vector<expression*> children;
        children.reserve(members.size());

        for(auto& e: members)
        {
            children.emplace_back(e.get());
        }
        return children;
    }
    [[nodiscard]]
    std::vector<const expression*> get_children() const override
    {
        std::vector<const expression*> children;
        children.reserve(members.size());

        for(auto& e: members)
        {
            children.emplace_back(e.get());
        }
        return children;
    }

    /** Declare a struct. */
    void declare_type(ty::context& ctx, sema::env& env);

    /** Define a struct. */
    void define_type(ty::context& ctx) const;
};

/** Field info. */
struct field_info
{
    /** Field index. */
    std::size_t field_index;

    /** Type id. */
    ty::type_id field_type_id;

    /** Struct type id. */
    ty::type_id struct_type_id;
};

/** Anonymous struct initialization. */
class struct_anonymous_initializer_expression : public named_expression
{
    /** Anonymous initializers. */
    std::vector<std::unique_ptr<expression>> initializers;

    /** Field info, in order of declaration. Set during type check. */
    std::vector<field_info> fields;

public:
    /** Set the super class. */
    using super = named_expression;

    /** Defaulted and deleted constructor. */
    struct_anonymous_initializer_expression() = default;
    struct_anonymous_initializer_expression(const struct_anonymous_initializer_expression& other)
    : super{other}
    , initializers{other.initializers
                   | std::views::transform(
                     [](const auto& ptr)
                     {
                         return ptr->clone();
                     })
                   | std::ranges::to<std::vector>()}
    {
    }
    struct_anonymous_initializer_expression(struct_anonymous_initializer_expression&&) = default;

    /** Assignment operators. */
    struct_anonymous_initializer_expression& operator=(const struct_anonymous_initializer_expression&) = delete;
    struct_anonymous_initializer_expression& operator=(struct_anonymous_initializer_expression&&) = default;

    /**
     * Construct a anonymous struct initialization.
     *
     * @param loc The location.
     * @param name The struct's name.
     * @param members The struct's members.
     */
    struct_anonymous_initializer_expression(
      token name,
      std::vector<std::unique_ptr<expression>> initializers)
    : named_expression{name.location, std::move(name)}
    , initializers{std::move(initializers)}
    {
    }

    [[nodiscard]]
    node_identifier get_id() const override
    {
        return node_identifier::struct_anonymous_initializer_expression;
    }

    [[nodiscard]] std::unique_ptr<expression> clone() const override;
    void serialize(archive& ar) override;

    [[nodiscard]]
    bool is_pure(cg::context&) const override
    {
        return true;
    }

    std::unique_ptr<cg::value> generate_code(cg::context& ctx, memory_context mc = memory_context::none) const override;
    void collect_names(co::context& ctx) override;
    void resolve_names(rs::context& ctx) override;
    std::optional<ty::type_id> type_check(ty::context& ctx, sema::env& env) override;
    [[nodiscard]] std::string to_string() const override;

    [[nodiscard]]
    std::vector<expression*> get_children() override
    {
        return initializers
               | std::views::transform(
                 [](const std::unique_ptr<expression>& initializer) -> expression*
                 {
                     return initializer.get();
                 })
               | std::ranges::to<std::vector>();
    }
    [[nodiscard]]
    std::vector<const expression*> get_children() const override
    {
        return initializers
               | std::views::transform(
                 [](const std::unique_ptr<expression>& initializer) -> const expression*
                 {
                     return initializer.get();
                 })
               | std::ranges::to<std::vector>();
    }
};

/** An initializer for named struct initialization. */
class named_initializer : public named_expression
{
    /** Initializer expression. */
    std::unique_ptr<expression> expr;

public:
    /** Set the super class. */
    using super = named_expression;

    /** Default constructor. */
    named_initializer() = default;

    /** Copy and move constructors. */
    named_initializer(const named_initializer& other)
    : super{other}
    , expr{other.expr->clone()}
    {
    }
    named_initializer(named_initializer&&) = default;

    /** Assignment operators. */
    named_initializer& operator=(const named_initializer&) = delete;
    named_initializer& operator=(named_initializer&&) = default;

    /**
     * Construct a new named initializer.
     *
     * @param name The initialized member's name.
     * @param expr The initializer expression.
     */
    named_initializer(token name, std::unique_ptr<expression> expr)
    : named_expression{name.location, std::move(name)}
    , expr{std::move(expr)}
    {
    }

    [[nodiscard]]
    node_identifier get_id() const override
    {
        return node_identifier::named_initializer;
    }

    [[nodiscard]] std::unique_ptr<expression> clone() const override;
    void serialize(archive& ar) override;

    [[nodiscard]]
    bool is_pure(cg::context&) const override
    {
        return true;
    }

    std::unique_ptr<cg::value> generate_code(cg::context& ctx, memory_context mc = memory_context::none) const override;
    void collect_names(co::context& ctx) override;
    void resolve_names(rs::context& ctx) override;
    std::optional<ty::type_id> type_check(ty::context& ctx, sema::env& env) override;

    [[nodiscard]] std::string to_string() const override;

    [[nodiscard]]
    std::vector<expression*> get_children() override
    {
        return {expr.get()};
    }

    [[nodiscard]]
    std::vector<const expression*> get_children() const override
    {
        return {expr.get()};
    }

    /** Return the initializer expression. */
    [[nodiscard]]
    expression* get_expression()
    {
        return expr.get();
    }

    /** Return the initializer expression. */
    [[nodiscard]]
    const expression* get_expression() const
    {
        return expr.get();
    }
};

/** Named struct initialization. */
class struct_named_initializer_expression : public named_expression
{
    /** Initialized members. */
    std::vector<std::unique_ptr<named_initializer>> initializers;

    /** Field info, in order of declaration. Set during type check. */
    std::vector<field_info> fields;

public:
    /** Set the super class. */
    using super = named_expression;

    /** Default constructor. */
    struct_named_initializer_expression() = default;

    /** Copy and move constructors. */
    struct_named_initializer_expression(const struct_named_initializer_expression& other)
    : super{other}
    , initializers{other.initializers
                   | std::views::transform(
                     [](const auto& ptr)
                     { return std::unique_ptr<named_initializer>(
                         static_cast<named_initializer*>(    // NOLINT(cppcoreguidelines-pro-type-static-cast-downcast)
                           ptr->clone().release())); })
                   | std::ranges::to<std::vector>()}
    {
    }
    struct_named_initializer_expression(struct_named_initializer_expression&&) = default;

    /** Assignment operators. */
    struct_named_initializer_expression& operator=(const struct_named_initializer_expression&) = delete;
    struct_named_initializer_expression& operator=(struct_named_initializer_expression&&) = default;

    /**
     * Construct a named struct initialization.
     *
     * @param name The struct's name.
     * @param initializers The named initializer expressions.
     */
    struct_named_initializer_expression(
      token name,
      std::vector<std::unique_ptr<named_initializer>> initializers)
    : named_expression{name.location, std::move(name)}
    , initializers{std::move(initializers)}
    {
    }

    [[nodiscard]]
    node_identifier get_id() const override
    {
        return node_identifier::struct_named_initializer_expression;
    }

    [[nodiscard]] std::unique_ptr<expression> clone() const override;
    void serialize(archive& ar) override;

    [[nodiscard]]
    bool is_pure(cg::context&) const override
    {
        return true;
    }

    std::unique_ptr<cg::value> generate_code(cg::context& ctx, memory_context mc = memory_context::none) const override;
    void collect_names(co::context& ctx) override;
    void resolve_names(rs::context& ctx) override;
    std::optional<ty::type_id> type_check(ty::context& ctx, sema::env& env) override;
    [[nodiscard]] std::string to_string() const override;

    [[nodiscard]]
    std::vector<expression*> get_children() override
    {
        return initializers
               | std::views::transform(
                 [](const std::unique_ptr<named_initializer>& initializer) -> expression*
                 {
                     return initializer.get();
                 })
               | std::ranges::to<std::vector>();
    }
    [[nodiscard]]
    std::vector<const expression*> get_children() const override
    {
        return initializers
               | std::views::transform(
                 [](const std::unique_ptr<named_initializer>& initializer) -> const expression*
                 {
                     return initializer.get();
                 })
               | std::ranges::to<std::vector>();
    }
};

/** Binary operators. */
class binary_expression : public expression
{
    /** The binary operator. */
    token op;

    /** Left and right hand sides. */
    std::unique_ptr<expression> lhs, rhs;

public:
    /** Set the super class. */
    using super = expression;

    /** Defaulted and deleted constructors. */
    binary_expression() = default;
    binary_expression(const binary_expression& other)
    : super{other}
    , op{other.op}
    , lhs{other.lhs->clone()}
    , rhs{other.rhs->clone()}
    {
    }
    binary_expression(binary_expression&&) = default;

    /** Assignment operators. */
    binary_expression& operator=(const binary_expression&) = delete;
    binary_expression& operator=(binary_expression&&) = default;

    /**
     * Construct a binary expression.
     *
     * @param loc The location.
     * @param op The binary operator.
     * @param lhs The left-hand side.
     * @param rhs The right-hand side.
     */
    binary_expression(
      source_location loc,
      token op,
      std::unique_ptr<expression> lhs,
      std::unique_ptr<expression> rhs)
    : expression{loc}
    , op{std::move(op)}
    , lhs{std::move(lhs)}
    , rhs{std::move(rhs)}
    {
    }

    [[nodiscard]]
    node_identifier get_id() const override
    {
        return node_identifier::binary_expression;
    }

    [[nodiscard]] std::unique_ptr<expression> clone() const override;
    void serialize(archive& ar) override;

    [[nodiscard]] bool needs_pop() const override;

    [[nodiscard]] bool is_const_eval(const_::env& env) const override;
    [[nodiscard]] bool is_pure(cg::context& ctx) const override;
    virtual std::optional<const_::const_info> evaluate(
      ty::context& ctx,
      const_::env& env) const override;

    std::unique_ptr<cg::value> generate_code(cg::context& ctx, memory_context mc = memory_context::none) const override;
    void collect_names(co::context& ctx) override;
    void resolve_names(rs::context& ctx) override;
    std::optional<ty::type_id> type_check(ty::context& ctx, sema::env& env) override;
    [[nodiscard]] std::string to_string() const override;

    [[nodiscard]]
    std::vector<expression*> get_children() override
    {
        return {lhs.get(), rhs.get()};
    }
    [[nodiscard]]
    std::vector<const expression*> get_children() const override
    {
        return {lhs.get(), rhs.get()};
    }

    /**
     * Insert AST nodes for implicit casts after type checking.
     *
     * @param ctx The type context.
     * @param env Semantic environment.
     */
    void insert_implicit_casts(
      ty::context& ctx,
      sema::env& env);
};

/** Unary operators. */
class unary_expression : public expression
{
    /** The operator. */
    token op;

    /** The operand. */
    std::unique_ptr<expression> operand;

public:
    /** Set the super class. */
    using super = expression;

    /** Default constructor. */
    unary_expression() = default;

    /** Copy and move constructors. */
    unary_expression(const unary_expression& other)
    : super{other}
    , op{other.op}
    , operand{other.operand->clone()}
    {
    }
    unary_expression(unary_expression&&) = default;

    /** Assignment operators. */
    unary_expression& operator=(const unary_expression&) = delete;
    unary_expression& operator=(unary_expression&&) = default;

    /**
     * Construct a unary expression.
     *
     * @param loc The location.
     * @param op The operator.
     * @param operand The operand.
     */
    unary_expression(
      source_location loc,
      token op,
      std::unique_ptr<expression> operand)
    : expression{loc}
    , op{std::move(op)}
    , operand{std::move(operand)}
    {
    }

    [[nodiscard]]
    node_identifier get_id() const override
    {
        return node_identifier::unary_expression;
    }

    [[nodiscard]] std::unique_ptr<expression> clone() const override;
    void serialize(archive& ar) override;

    [[nodiscard]] bool needs_pop() const override
    {
        return true;
    }

    [[nodiscard]] bool is_const_eval(const_::env& env) const override;
    [[nodiscard]] bool is_pure(cg::context& ctx) const override;
    virtual std::optional<const_::const_info> evaluate(
      ty::context& ctx,
      const_::env& env) const override;

    std::unique_ptr<cg::value> generate_code(cg::context& ctx, memory_context mc = memory_context::none) const override;
    void collect_names(co::context& ctx) override;
    void resolve_names(rs::context& ctx) override;
    std::optional<ty::type_id> type_check(ty::context& ctx, sema::env& env) override;
    [[nodiscard]] std::string to_string() const override;

    [[nodiscard]]
    std::vector<expression*> get_children() override
    {
        return {operand.get()};
    }
    [[nodiscard]]
    std::vector<const expression*> get_children() const override
    {
        return {operand.get()};
    }
};

class new_expression : public expression
{
    /** The type expression. */
    std::unique_ptr<type_expression> type_expr;

    /** Type id of the type expression. */
    std::optional<ty::type_id> type_expr_id;

    /** Array length expression. */
    std::unique_ptr<expression> array_length_expr;

public:
    /** Set the super class. */
    using super = expression;

    /** Defaulted and deleted constructors. */
    new_expression() = default;
    new_expression(const new_expression& other)
    : super{other}
    , type_expr{other.type_expr->clone()}
    , array_length_expr{other.array_length_expr->clone()}
    {
    }
    new_expression(new_expression&&) = default;

    /** Assignment operators. */
    new_expression& operator=(const new_expression&) = delete;
    new_expression& operator=(new_expression&&) = default;

    /**
     * Construct a 'new' expression.
     *
     * @param loc The location.
     * @param type The type to be allocated.
     * @param array_length_expr The array length expression.
     */
    new_expression(
      source_location loc,
      std::unique_ptr<type_expression> type_expr,
      std::unique_ptr<expression> array_length_expr)
    : expression{loc}
    , type_expr{std::move(type_expr)}
    , array_length_expr{std::move(array_length_expr)}
    {
    }

    [[nodiscard]]
    node_identifier get_id() const override
    {
        return node_identifier::new_expression;
    }

    [[nodiscard]] std::unique_ptr<expression> clone() const override;
    void serialize(archive& ar) override;

    [[nodiscard]] bool needs_pop() const override
    {
        return true;
    }

    [[nodiscard]] bool is_pure(cg::context& ctx) const override;

    std::unique_ptr<cg::value> generate_code(cg::context& ctx, memory_context mc = memory_context::none) const override;
    void collect_names(co::context& ctx) override;
    void resolve_names(rs::context& ctx) override;
    std::optional<ty::type_id> type_check(ty::context& ctx, sema::env& env) override;
    [[nodiscard]] std::string to_string() const override;

    [[nodiscard]]
    std::vector<expression*> get_children() override
    {
        return {array_length_expr.get()};
    }
    [[nodiscard]]
    std::vector<const expression*> get_children() const override
    {
        return {array_length_expr.get()};
    }
};

/** 'null' expression. */
class null_expression : public expression
{
public:
    /** Set the super class. */
    using super = expression;

    /** Defaulted constructors. */
    null_expression() = default;
    null_expression(const null_expression& other)
    : super{other}
    {
    }
    null_expression(null_expression&&) = default;

    /** Assignment operators. */
    null_expression& operator=(const null_expression&) = delete;
    null_expression& operator=(null_expression&&) = default;

    /**
     * Construct a 'null' expression.
     *
     * @param loc The location.
     */
    explicit null_expression(source_location loc)
    : expression{loc}
    {
    }

    [[nodiscard]]
    node_identifier get_id() const override
    {
        return node_identifier::null_expression;
    }

    [[nodiscard]] std::unique_ptr<expression> clone() const override;

    [[nodiscard]] bool needs_pop() const override
    {
        return true;
    }

    [[nodiscard]] bool is_pure(cg::context&) const override
    {
        return true;
    }

    std::unique_ptr<cg::value> generate_code(cg::context& ctx, memory_context mc = memory_context::none) const override;
    std::optional<ty::type_id> type_check(ty::context& ctx, sema::env& env) override;
    [[nodiscard]] std::string to_string() const override;
};

/** Postfix operator expression. */
class postfix_expression : public expression
{
    /** The operand. */
    std::unique_ptr<expression> identifier;

    /** The operator. */
    token op;

public:
    /** Set the super class. */
    using super = expression;

    /** Defaulted and deleted constructor. */
    postfix_expression() = default;
    postfix_expression(const postfix_expression& other)
    : super{other}
    , identifier{other.identifier->clone()}
    , op{other.op}
    {
    }
    postfix_expression(postfix_expression&&) = default;

    /** Assignment operators. */
    postfix_expression& operator=(const postfix_expression&) = delete;
    postfix_expression& operator=(postfix_expression&&) = default;

    /**
     * Construct a unary expression.
     *
     * @param identifier The operand.
     * @param op The operator.
     */
    postfix_expression(std::unique_ptr<expression> identifier, token op)
    : expression{identifier->get_location()}
    , identifier{std::move(identifier)}
    , op{std::move(op)}
    {
    }

    [[nodiscard]]
    node_identifier get_id() const override
    {
        return node_identifier::postfix_expression;
    }

    [[nodiscard]] std::unique_ptr<expression> clone() const override;
    void serialize(archive& ar) override;

    [[nodiscard]]
    bool needs_pop() const override
    {
        return true;
    }

    std::unique_ptr<cg::value> generate_code(cg::context& ctx, memory_context mc = memory_context::none) const override;
    void collect_names(co::context& ctx) override;
    void resolve_names(rs::context& ctx) override;
    std::optional<ty::type_id> type_check(ty::context& ctx, sema::env& env) override;
    [[nodiscard]] std::string to_string() const override;

    [[nodiscard]]
    std::vector<expression*> get_children() override
    {
        return {identifier.get()};
    }
    [[nodiscard]]
    std::vector<const expression*> get_children() const override
    {
        return {identifier.get()};
    }
};

/**
 * Function prototype.
 *
 * @note Not a subclass of `expression` because there is no code generation.
 */
class prototype_ast
{
    /** Token location. */
    source_location loc;

    /** The function name. */
    token name;

    /** Symbol id. */
    sema::symbol_id symbol_id{sema::symbol_id::invalid};

    /** The function's arguments as pairs `(name, (symbol_id, type))`. */
    std::vector<
      std::tuple<
        token,
        sema::symbol_id,
        std::unique_ptr<type_expression>>>
      args;

    /** The function's return type as a pair. */
    std::unique_ptr<type_expression> return_type;

    /** Argument type info. Set during type checking. */
    std::vector<ty::type_id> arg_type_ids;

    /** Return type info. Set during type checking. */
    ty::type_id return_type_id;

public:
    /** Default constructor. */
    prototype_ast() = default;

    /** Copy and move constructors. */
    prototype_ast(const prototype_ast& other)
    : loc{other.loc}
    , name{other.name}
    , args{other.args
           | std::views::transform(
             [](const auto& arg)
             { return std::make_tuple(
                 std::get<0>(arg),
                 std::get<1>(arg),
                 std::get<2>(arg)->clone()); })
           | std::ranges::to<std::vector>()}
    , return_type{other.return_type->clone()}
    , arg_type_ids{other.arg_type_ids}
    , return_type_id{other.return_type_id}
    {
    }
    prototype_ast(prototype_ast&&) = default;

    /** Assignment operators. */
    prototype_ast& operator=(const prototype_ast&) = delete;
    prototype_ast& operator=(prototype_ast&&) = default;

    /**
     * Construct a function prototype.
     *
     * @param loc The location.
     * @param name The function's name.
     * @param args The function's arguments as a vector of pairs `(name, type)`.
     * @param return_type The function's return type.
     */
    prototype_ast(
      source_location loc,
      token name,
      std::vector<
        std::tuple<
          token,
          sema::symbol_id,
          std::unique_ptr<type_expression>>>
        args,
      std::unique_ptr<type_expression> return_type)
    : loc{loc}
    , name{std::move(name)}
    , args{std::move(args)}
    , return_type{std::move(return_type)}
    {
    }

    [[nodiscard]] std::unique_ptr<prototype_ast> clone() const;
    void serialize(archive& ar);

    void collect_names(co::context& ctx);
    [[nodiscard]] std::string to_string() const;

    /**
     * Declare the function. Sets up the type information.
     *
     * @param ctx The type context.
     * @param env The semantic environment.
     */
    void declare(ty::context& ctx, sema::env& env);

    /** Return the function name. */
    [[nodiscard]]
    const std::string& get_name() const
    {
        return name.s;
    }

    /** Return the name token. */
    [[nodiscard]]
    const token& get_name_token() const
    {
        return name;
    }

    /** Return the argument type id vector. */
    [[nodiscard]]
    std::vector<ty::type_id> get_arg_type_ids() const
    {
        return arg_type_ids;
    }

    /** Return the arguments symbol ids and type ids. */
    [[nodiscard]]
    std::vector<std::pair<sema::symbol_id, ty::type_id>> get_arg_infos() const;

    /** Return the return type id. */
    [[nodiscard]]
    ty::type_id get_return_type_id() const
    {
        return return_type_id;
    }
};

/** AST of a code block. This can refer to any block, e.g. the whole program, or a function body. */
class block : public expression
{
    /** The program expressions. */
    std::vector<std::unique_ptr<expression>> exprs;

public:
    /** Set the super class. */
    using super = expression;

    /** Defaulted and deleted constructor. */
    block() = default;
    block(const block& other)
    : super{other}
    , exprs{other.exprs
            | std::views::transform(
              [](const auto& ptr)
              {
                  return ptr->clone();
              })
            | std::ranges::to<std::vector>()}
    {
    }
    block(block&&) = default;

    /** Assignment operators. */
    block& operator=(const block&) = delete;
    block& operator=(block&&) = default;

    /**
     * Construct a program.
     *
     * @param loc The location.
     * @param exprs The program expressions.
     */
    block(
      source_location loc,
      std::vector<std::unique_ptr<expression>> exprs)
    : expression{loc}
    , exprs{std::move(exprs)}
    {
    }

    [[nodiscard]]
    node_identifier get_id() const override
    {
        return node_identifier::block;
    }

    [[nodiscard]] std::unique_ptr<expression> clone() const override;
    void serialize(archive& ar) override;

    [[nodiscard]] bool is_pure(cg::context&) const override;

    std::unique_ptr<cg::value> generate_code(cg::context& ctx, memory_context mc = memory_context::none) const override;
    void collect_names(co::context& ctx) override;
    void resolve_names(rs::context& ctx) override;
    std::optional<ty::type_id> type_check(ty::context& ctx, sema::env& env) override;
    [[nodiscard]] std::string to_string() const override;

    [[nodiscard]]
    std::vector<expression*> get_children() override
    {
        std::vector<expression*> children;
        children.reserve(exprs.size());

        for(auto& e: exprs)
        {
            children.emplace_back(e.get());
        }
        return children;
    }
    [[nodiscard]]
    std::vector<const expression*> get_children() const override
    {
        std::vector<const expression*> children;
        children.reserve(exprs.size());

        for(auto& e: exprs)
        {
            children.emplace_back(e.get());
        }
        return children;
    }

    /**
     * Name collection.
     *
     * @param ctx Collection context.
     * @param push_anonymous_scope Whether to push an anonymous name for this scope.
     */
    void collect_names(co::context& ctx, bool push_anonymous_scope);
};

/** A function definition. */
class function_expression : public expression
{
    /** The function prototype. */
    std::unique_ptr<prototype_ast> prototype;

    /** The function's body. */
    std::unique_ptr<block> body;

public:
    /** Set the super class. */
    using super = expression;

    /** Default constructor. */
    function_expression() = default;

    /** Copy and move constructors. */
    function_expression(const function_expression& other)
    : super{other}
    , prototype{other.prototype->clone()}
    , body{static_cast<block*>(other.body->clone().release())}
    {
    }
    function_expression(function_expression&&) = default;

    /** Assignment operators. */
    function_expression& operator=(const function_expression&) = delete;
    function_expression& operator=(function_expression&&) = default;

    /**
     * Construct a function.
     *
     * @param loc The location.
     * @param prototype The function's prototype.
     * @param body The function's body.
     */
    function_expression(
      source_location loc,
      std::unique_ptr<prototype_ast> prototype,
      std::unique_ptr<block> body)
    : expression{loc}
    , prototype{std::move(prototype)}
    , body{std::move(body)}
    {
    }

    [[nodiscard]]
    node_identifier get_id() const override
    {
        return node_identifier::function_expression;
    }

    [[nodiscard]] std::unique_ptr<expression> clone() const override;
    void serialize(archive& ar) override;

    std::unique_ptr<cg::value> generate_code(cg::context& ctx, memory_context mc = memory_context::none) const override;
    void collect_names(co::context& ctx) override;
    void resolve_names(rs::context& ctx) override;
    std::optional<ty::type_id> type_check(ty::context& ctx, sema::env& env) override;
    [[nodiscard]] std::string to_string() const override;

    [[nodiscard]]
    std::vector<expression*> get_children() override
    {
        // FIXME The prototype should be included.
        if(body)
        {
            return {body.get()};
        }
        return {};
    }
    [[nodiscard]]
    std::vector<const expression*> get_children() const override
    {
        // FIXME The prototype should be included.
        if(body)
        {
            return {body.get()};
        }
        return {};
    }

    /**
     * Declare the function.
     *
     * @param ctx The type context.
     * @param env The semantic environment.
     */
    void declare_function(ty::context& ctx, sema::env& env);

    /** Return the argument type id vector. */
    [[nodiscard]]
    std::vector<ty::type_id> get_arg_type_ids() const
    {
        return prototype->get_arg_type_ids();
    }

    /** Return the return type id. */
    [[nodiscard]]
    ty::type_id get_return_type_id() const
    {
        return prototype->get_return_type_id();
    }
};

/** Function calls. */
class call_expression : public expression
{
    /** The callee's name. */
    token callee;

    /** The function's arguments. */
    std::vector<std::unique_ptr<expression>> args;

    /** An optional index expression for return value array access. */
    std::unique_ptr<expression> index_expr;

    /** The return type. Set during type checking. */
    ty::type_id return_type;

    /** Whether the return type is void. */
    bool is_void_return_type{false};

public:
    /** Set the super class. */
    using super = expression;

    /** Defaulted and deleted constructors. */
    call_expression() = default;
    call_expression(const call_expression& other)
    : super{other}
    , callee{other.callee}
    , args{other.args
           | std::views::transform(
             [](const auto& ptr)
             {
                 return ptr->clone();
             })
           | std::ranges::to<std::vector>()}
    , index_expr{other.index_expr ? other.index_expr->clone() : nullptr}
    , return_type{other.return_type}
    {
    }
    call_expression(call_expression&&) = default;

    /** Assignment operators. */
    call_expression& operator=(const call_expression&) = delete;
    call_expression& operator=(call_expression&&) = default;

    /**
     * Construct a function call.
     *
     * @param callee The callee's name.
     * @param args The argument expressions.
     * @param index_expr Index expression for array access.
     */
    call_expression(
      token callee,
      std::vector<std::unique_ptr<expression>> args,
      std::unique_ptr<expression> index_expr = nullptr)
    : expression{callee.location}
    , callee{std::move(callee)}
    , args{std::move(args)}
    , index_expr{std::move(index_expr)}
    {
    }

    [[nodiscard]]
    node_identifier get_id() const override
    {
        return node_identifier::call_expression;
    }

    [[nodiscard]]
    bool is_call_expression() const override
    {
        return true;
    }

    [[nodiscard]]
    call_expression* as_call_expression() override
    {
        return this;
    }

    [[nodiscard]] std::unique_ptr<expression> clone() const override;
    void serialize(archive& ar) override;

    [[nodiscard]]
    bool needs_pop() const override
    {
        return !is_void_return_type;
    }

    [[nodiscard]] bool is_pure(cg::context& ctx) const override;

    std::unique_ptr<cg::value> generate_code(cg::context& ctx, memory_context mc = memory_context::none) const override;
    void collect_names(co::context& ctx) override;
    void resolve_names(rs::context& ctx) override;
    std::optional<ty::type_id> type_check(ty::context& ctx, sema::env& env) override;
    [[nodiscard]] std::string to_string() const override;

    [[nodiscard]]
    std::vector<expression*> get_children() override
    {
        std::vector<expression*> children;
        children.reserve(args.size());

        for(auto& e: args)
        {
            children.emplace_back(e.get());
        }
        if(index_expr)
        {
            children.emplace_back(index_expr.get());
        }
        return children;
    }
    [[nodiscard]]
    std::vector<const expression*> get_children() const override
    {
        std::vector<const expression*> children;
        children.reserve(args.size());

        for(auto& e: args)
        {
            children.emplace_back(e.get());
        }
        if(index_expr)
        {
            children.emplace_back(index_expr.get());
        }
        return children;
    }

    /** Return the callee's token/name. */
    [[nodiscard]]
    token get_callee() const
    {
        return callee;
    }

    /** Return the callee's name including namespaces. */
    [[nodiscard]]
    std::string get_qualified_callee_name() const;

    /** Return the argument expressions. */
    [[nodiscard]]
    std::vector<expression*> get_args() const
    {
        std::vector<expression*> children;
        children.reserve(args.size());
        for(auto& e: args)
        {
            children.emplace_back(e.get());
        }
        return children;
    }
};

/** Macro invocation. */
class macro_invocation : public named_expression
{
    friend class expression;

    /** Expressions the macro operates on. */
    std::vector<std::unique_ptr<expression>> exprs;

    /** An optional index expression for return value array access. */
    std::unique_ptr<expression> index_expr;

    /** Macro expansion. */
    std::unique_ptr<expression> expansion;

public:
    /** Set the super class. */
    using super = named_expression;

    /** Defaulted and deleted constructors. */
    macro_invocation() = default;
    macro_invocation(const macro_invocation& other)
    : super{other}
    , exprs{other.exprs
            | std::views::transform(
              [](const auto& ptr)
              {
                  return ptr->clone();
              })
            | std::ranges::to<std::vector>()}
    , index_expr{other.index_expr ? other.index_expr->clone() : nullptr}
    , expansion{other.expansion ? other.expansion->clone() : nullptr}
    {
    }

    macro_invocation(macro_invocation&&) = default;

    /** Assignment operators. */
    macro_invocation& operator=(const macro_invocation&) = delete;
    macro_invocation& operator=(macro_invocation&&) = default;

    /**
     * Construct a macro invocation.
     *
     * @param name The macro's name.
     * @param exprs Expressions the macro operates on.
     * @param index_expr Index expression for array access.
     * @param expansion A macro expansion.
     */
    macro_invocation(
      token name,
      std::vector<std::unique_ptr<expression>> exprs,
      std::unique_ptr<expression> index_expr = nullptr,
      std::unique_ptr<expression> expansion = nullptr)
    : named_expression{name.location, std::move(name)}
    , exprs{std::move(exprs)}
    , index_expr{std::move(index_expr)}
    , expansion{std::move(expansion)}
    {
    }

    [[nodiscard]]
    node_identifier get_id() const override
    {
        return node_identifier::macro_invocation;
    }

    [[nodiscard]] std::unique_ptr<expression> clone() const override;
    void serialize(archive& ar) override;

    [[nodiscard]]
    bool is_macro_invocation() const override
    {
        return true;
    }

    [[nodiscard]]
    macro_invocation* as_macro_invocation() override
    {
        return this;
    }

    [[nodiscard]]
    bool needs_pop() const override
    {
        return expansion->needs_pop();
    }

    [[nodiscard]] bool is_pure(cg::context& ctx) const override
    {
        return expansion->is_pure(ctx);
    }

    std::unique_ptr<cg::value> generate_code(cg::context& ctx, memory_context mc = memory_context::none) const override;
    void collect_names(co::context& ctx) override;
    void resolve_names(rs::context& ctx) override;
    std::optional<ty::type_id> type_check(ty::context& ctx, sema::env& env) override;
    [[nodiscard]] std::string to_string() const override;

    [[nodiscard]]
    std::vector<expression*> get_children() override
    {
        std::vector<expression*> children;
        if(expansion != nullptr)
        {
            children.reserve(1 + static_cast<std::size_t>(index_expr != nullptr));
            children.emplace_back(expansion.get());
        }
        else
        {
            children.reserve(
              exprs.size()
              + static_cast<std::size_t>(index_expr != nullptr));

            for(auto& e: exprs)
            {
                children.emplace_back(e.get());
            }
        }

        if(index_expr)
        {
            children.emplace_back(index_expr.get());
        }

        return children;
    }
    [[nodiscard]]
    std::vector<const expression*> get_children() const override
    {
        std::vector<const expression*> children;
        if(expansion != nullptr)
        {
            children.reserve(1 + static_cast<std::size_t>(index_expr != nullptr));
            children.emplace_back(expansion.get());
        }
        else
        {
            children.reserve(
              exprs.size()
              + static_cast<std::size_t>(index_expr != nullptr));

            for(auto& e: exprs)
            {
                children.emplace_back(e.get());
            }
        }

        if(index_expr)
        {
            children.emplace_back(index_expr.get());
        }

        return children;
    }

    /**
     * Get the tokens the macro operates on.
     *
     * @return The expressions.
     */
    [[nodiscard]]
    const std::vector<std::unique_ptr<expression>>& get_exprs() const
    {
        return exprs;
    }

    /** Whether this macro is expanded. */
    [[nodiscard]]
    bool has_expansion() const
    {
        return expansion != nullptr;
    }

    /**
     * Set the macro expansion.
     *
     * @param exp The expansion.
     */
    void set_expansion(std::unique_ptr<expression> exp)
    {
        expansion = std::move(exp);
    }

    /** Return the callee's name including namespaces. */
    [[nodiscard]]
    std::string get_qualified_callee_name() const;
};

/*
 * Statements.
 */

/** Return statement. */
class return_statement : public expression
{
    /** (Optional) returned expression. */
    std::unique_ptr<expression> expr;

public:
    /** Set the super class. */
    using super = expression;

    /** Defaulted and deleted constructors. */
    return_statement() = default;
    return_statement(const return_statement& other)
    : super{other}
    , expr{other.expr ? other.expr->clone() : nullptr}
    {
    }
    return_statement(return_statement&&) = default;

    /** Assignment operators. */
    return_statement& operator=(const return_statement&) = delete;
    return_statement& operator=(return_statement&&) = default;

    /**
     * Construct a return statement.
     *
     * @param loc The location.
     * @param expr The returned expression (if any).
     */
    return_statement(
      source_location loc,
      std::unique_ptr<expression> expr)
    : expression{loc}
    , expr{std::move(expr)}
    {
    }

    [[nodiscard]]
    node_identifier get_id() const override
    {
        return node_identifier::return_statement;
    }

    [[nodiscard]] std::unique_ptr<expression> clone() const override;
    void serialize(archive& ar) override;

    std::unique_ptr<cg::value> generate_code(cg::context& ctx, memory_context mc = memory_context::none) const override;
    void collect_names(co::context& ctx) override;
    void resolve_names(rs::context& ctx) override;
    std::optional<ty::type_id> type_check(ty::context& ctx, sema::env& env) override;
    [[nodiscard]] std::string to_string() const override;

    [[nodiscard]]
    std::vector<expression*> get_children() override
    {
        if(expr)
        {
            return {expr.get()};
        }
        return {};
    }
    [[nodiscard]]
    std::vector<const expression*> get_children() const override
    {
        if(expr)
        {
            return {expr.get()};
        }
        return {};
    }
};

/** If statement. */
class if_statement : public expression
{
    /** The condition. */
    std::unique_ptr<expression> condition;

    /** If block. */
    std::unique_ptr<expression> if_block;

    /** (Optional) else block. */
    std::unique_ptr<expression> else_block;

public:
    /** Set the super class. */
    using super = expression;

    /** Defaulted and deleted constructors. */
    if_statement() = default;
    if_statement(const if_statement& other)
    : super{other}
    , condition{other.condition->clone()}
    , if_block{other.if_block->clone()}
    , else_block{other.else_block ? other.else_block->clone() : nullptr}
    {
    }
    if_statement(if_statement&&) = default;

    /** Assignment operators. */
    if_statement& operator=(const if_statement&) = delete;
    if_statement& operator=(if_statement&&) = default;

    /**
     * Construct an if statement.
     *
     * @param loc The location.
     * @param condition The condition.
     * @param if_block The block to be executed if the condition was true.
     * @param else_block The block to be executed if the condition was false.
     */
    if_statement(
      source_location loc,
      std::unique_ptr<expression> condition,
      std::unique_ptr<expression> if_block,
      std::unique_ptr<expression> else_block)
    : expression{loc}
    , condition{std::move(condition)}
    , if_block{std::move(if_block)}
    , else_block{std::move(else_block)}
    {
    }

    [[nodiscard]]
    node_identifier get_id() const override
    {
        return node_identifier::if_statement;
    }

    [[nodiscard]] std::unique_ptr<expression> clone() const override;
    void serialize(archive& ar) override;

    [[nodiscard]] bool is_pure(cg::context& ctx) const override
    {
        return condition->is_pure(ctx)
               && if_block->is_pure(ctx)
               && (!else_block || else_block->is_pure(ctx));
    }

    std::unique_ptr<cg::value> generate_code(cg::context& ctx, memory_context mc = memory_context::none) const override;
    void collect_names(co::context& ctx) override;
    void resolve_names(rs::context& ctx) override;
    std::optional<ty::type_id> type_check(ty::context& ctx, sema::env& env) override;
    [[nodiscard]] std::string to_string() const override;

    [[nodiscard]]
    std::vector<expression*> get_children() override
    {
        std::vector<expression*> children;
        children.reserve(else_block ? 3 : 2);

        children.emplace_back(condition.get());
        children.emplace_back(if_block.get());
        if(else_block)
        {
            children.emplace_back(else_block.get());
        }
        return children;
    }
    [[nodiscard]]
    std::vector<const expression*> get_children() const override
    {
        std::vector<const expression*> children;
        children.reserve(else_block ? 3 : 2);

        children.emplace_back(condition.get());
        children.emplace_back(if_block.get());
        if(else_block)
        {
            children.emplace_back(else_block.get());
        }
        return children;
    }
};

/** While statement. */
class while_statement : public expression
{
    /** The condition. */
    std::unique_ptr<expression> condition;

    /** While block. */
    std::unique_ptr<expression> while_block;

public:
    /** Set the super class. */
    using super = expression;

    /** Defaulted and deleted constructors. */
    while_statement() = default;
    while_statement(const while_statement& other)
    : super{other}
    , condition{other.condition->clone()}
    , while_block{other.while_block->clone()}
    {
    }
    while_statement(while_statement&&) = default;

    /** Assignment operators. */
    while_statement& operator=(const while_statement&) = delete;
    while_statement& operator=(while_statement&&) = default;

    /**
     * Construct an if statement.
     *
     * @param loc The location.
     * @param condition The condition.
     * @param while_block The block to be executed while the condition is true.
     */
    while_statement(
      source_location loc,
      std::unique_ptr<expression> condition,
      std::unique_ptr<expression> while_block)
    : expression{loc}
    , condition{std::move(condition)}
    , while_block{std::move(while_block)}
    {
    }

    [[nodiscard]]
    node_identifier get_id() const override
    {
        return node_identifier::while_statement;
    }

    [[nodiscard]] std::unique_ptr<expression> clone() const override;
    void serialize(archive& ar) override;

    [[nodiscard]] bool is_pure(cg::context& ctx) const override
    {
        return condition->is_pure(ctx)
               && while_block->is_pure(ctx);
    }

    std::unique_ptr<cg::value> generate_code(cg::context& ctx, memory_context mc = memory_context::none) const override;
    void collect_names(co::context& ctx) override;
    void resolve_names(rs::context& ctx) override;
    std::optional<ty::type_id> type_check(ty::context& ctx, sema::env& env) override;
    [[nodiscard]] std::string to_string() const override;

    [[nodiscard]]
    std::vector<expression*> get_children() override
    {
        return {condition.get(), while_block.get()};
    }
    [[nodiscard]]
    std::vector<const expression*> get_children() const override
    {
        return {condition.get(), while_block.get()};
    }
};

/** Break statement. */
class break_statement : public expression
{
public:
    /** Set the super class. */
    using super = expression;

    /** Default constructor. */
    break_statement() = default;

    /** Copy and move constructors. */
    break_statement(const break_statement& other)
    : super{other}
    {
    }
    break_statement(break_statement&&) = default;

    /** Assignment operators. */
    break_statement& operator=(const break_statement&) = delete;
    break_statement& operator=(break_statement&&) = default;

    /**
     * Construct a break statement.
     *
     * @param loc The location.
     */
    explicit break_statement(source_location loc)
    : expression{loc}
    {
    }

    [[nodiscard]]
    node_identifier get_id() const override
    {
        return node_identifier::break_statement;
    }

    [[nodiscard]] std::unique_ptr<expression> clone() const override;

    std::unique_ptr<cg::value> generate_code(cg::context& ctx, memory_context mc = memory_context::none) const override;

    [[nodiscard]]
    std::string to_string() const override
    {
        return "Break()";
    }
};

/** Continue statement. */
class continue_statement : public expression
{
public:
    /** Set the super class. */
    using super = expression;

    /** Default constructor. */
    continue_statement() = default;

    /** Copy and move constructors. */
    continue_statement(const continue_statement& other)
    : super{other}
    {
    }
    continue_statement(continue_statement&&) = default;

    /** Assignment operators. */
    continue_statement& operator=(const continue_statement&) = delete;
    continue_statement& operator=(continue_statement&&) = default;

    /**
     * Construct a continue statement.
     *
     * @param loc The location.
     */
    explicit continue_statement(source_location loc)
    : expression{loc}
    {
    }

    [[nodiscard]]
    node_identifier get_id() const override
    {
        return node_identifier::continue_statement;
    }

    [[nodiscard]] std::unique_ptr<expression> clone() const override;

    std::unique_ptr<cg::value> generate_code(cg::context& ctx, memory_context mc = memory_context::none) const override;

    [[nodiscard]]
    std::string to_string() const override
    {
        return "Continue()";
    }
};

/** Macro branch. */
class macro_branch : public expression
{
    friend class macro_expression;

    /** Arguments. */
    std::vector<std::pair<token, token>> args;

    /** Whether the arguments end with a list. */
    bool args_end_with_list{false};

    /** Body. */
    std::unique_ptr<block> body;

public:
    using super = expression;

    /** Defaulted and deleted constructors. */
    macro_branch() = default;
    macro_branch(const macro_branch& other)
    : super{other}
    , args{other.args}
    , args_end_with_list{other.args_end_with_list}
    , body{static_cast<block*>(other.body->clone().release())}
    {
    }
    macro_branch(macro_branch&&) = default;

    /** Assignment operators. */
    macro_branch& operator=(const macro_branch&) = delete;
    macro_branch& operator=(macro_branch&&) = default;

    /**
     * Construct a macro branch.
     *
     * @param loc The branch location.
     * @param args Macro arguments.
     * @param args_end_with_list Whether the arguments end with a list.
     * @param body Branch body.
     */
    macro_branch(
      source_location loc,
      std::vector<std::pair<token, token>> args,
      bool args_end_with_list,
      std::unique_ptr<block> body)
    : expression{loc}
    , args{std::move(args)}
    , args_end_with_list{args_end_with_list}
    , body{std::move(body)}
    {
    }

    [[nodiscard]]
    node_identifier get_id() const override
    {
        return node_identifier::macro_branch;
    }

    [[nodiscard]]
    std::unique_ptr<expression> clone() const override;
    void serialize(archive& ar) override;

    [[nodiscard]]
    bool is_macro_branch() const override
    {
        return true;
    }

    [[nodiscard]]
    macro_branch* as_macro_branch() override
    {
        return this;
    }

    [[nodiscard]]
    virtual const class macro_branch* as_macro_branch() const override
    {
        return this;
    }

    std::unique_ptr<cg::value> generate_code(cg::context& ctx, memory_context mc = memory_context::none) const override;
    void collect_names(co::context& ctx) override;
    void resolve_names(rs::context& ctx) override;
    std::optional<ty::type_id> type_check(ty::context& ctx, sema::env& env) override;
    [[nodiscard]] std::string to_string() const override;

    [[nodiscard]]
    std::vector<expression*> get_children() override
    {
        return {body.get()};
    }
    [[nodiscard]]
    std::vector<const expression*> get_children() const override
    {
        return {body.get()};
    }

    /** Get the macro branch arguments. */
    [[nodiscard]]
    const std::vector<std::pair<token, token>>& get_args() const
    {
        return args;
    }

    /** Whether this macro branch ens with an expression list capture.  */
    [[nodiscard]]
    bool ends_with_list_capture() const
    {
        return args_end_with_list;
    }

    /** Get the macro branch body. */
    [[nodiscard]]
    const std::unique_ptr<block>& get_body() const
    {
        return body;
    }
};

/**
 * An expression list for macro expansion.
 *
 * NOTE Temporary expression during macro expansion.
 */
class macro_expression_list : public expression
{
    /** The expression list. */
    std::vector<std::unique_ptr<expression>> expr_list;

public:
    /** Set the super class. */
    using super = expression;

    /** Defaulted and deleted constructors. */
    macro_expression_list() = default;
    macro_expression_list(const macro_expression_list& other)
    : super{other}
    , expr_list{other.expr_list
                | std::views::transform(
                  [](const auto& ptr)
                  {
                      return ptr->clone();
                  })
                | std::ranges::to<std::vector>()}
    {
    }
    macro_expression_list(macro_expression_list&&) = default;

    /** Assignment operators. */
    macro_expression_list& operator=(const macro_expression_list&) = delete;
    macro_expression_list& operator=(macro_expression_list&&) = default;

    /**
     * Construct a macro expression list.
     *
     * @param loc The expression location.
     * @param expr_list The expression list.
     */
    macro_expression_list(
      source_location loc,
      std::vector<std::unique_ptr<expression>> expr_list)
    : expression{loc}
    , expr_list{std::move(expr_list)}
    {
    }

    [[nodiscard]]
    node_identifier get_id() const override
    {
        return node_identifier::macro_expression_list;
    }

    [[nodiscard]]
    std::unique_ptr<expression> clone() const override;
    void serialize(archive& ar) override;

    [[nodiscard]]
    macro_expression_list* as_macro_expression_list() override
    {
        return this;
    }

    [[nodiscard]]
    const macro_expression_list* as_macro_expression_list() const override
    {
        return this;
    }

    std::unique_ptr<cg::value> generate_code(cg::context& ctx, memory_context mc = memory_context::none) const override;
    void collect_names(co::context& ctx) override;
    void resolve_names(rs::context& ctx) override;
    std::optional<ty::type_id> type_check(ty::context& ctx, sema::env& env) override;
    std::string to_string() const override;

    /** Get the expression list. */
    std::vector<std::unique_ptr<expression>>& get_expr_list()
    {
        return expr_list;
    }

    /** Get the expression list. */
    const std::vector<std::unique_ptr<expression>>& get_expr_list() const
    {
        return expr_list;
    }
};

/** Macros. */
class macro_expression : public named_expression
{
    /** The macro's branches. */
    std::vector<std::unique_ptr<macro_branch>> branches;

public:
    /** Set the super class. */
    using super = named_expression;

    /** Defaulted and deleted constructors. */
    macro_expression() = default;
    macro_expression(const macro_expression& other)
    : super{other}
    , branches{other.branches
               | std::views::transform(
                 [](const auto& ptr)
                 {
                     return std::unique_ptr<macro_branch>(
                       static_cast<macro_branch*>(ptr->clone().release()));
                 })
               | std::ranges::to<std::vector>()}
    {
    }
    macro_expression(macro_expression&&) = default;

    /** Assignment operators. */
    macro_expression& operator=(const macro_expression&) = delete;
    macro_expression& operator=(macro_expression&&) = default;

    /**
     * Construct a macro expression.
     *
     * @param loc The location.
     * @param name The macro's name.
     * @param branches The macro's branches.
     */
    macro_expression(
      source_location loc,
      token name,
      std::vector<std::unique_ptr<macro_branch>> branches)
    : named_expression{loc, std::move(name)}
    , branches{std::move(branches)}
    {
    }

    [[nodiscard]]
    node_identifier get_id() const override
    {
        return node_identifier::macro_expression;
    }

    [[nodiscard]]
    std::unique_ptr<expression> clone() const override;
    void serialize(archive& ar) override;

    [[nodiscard]]
    bool is_macro_expression() const override
    {
        return true;
    }

    [[nodiscard]]
    macro_expression* as_macro_expression() override
    {
        return this;
    }

    std::unique_ptr<cg::value> generate_code(cg::context& ctx, memory_context mc = memory_context::none) const override;
    void collect_names(co::context& ctx) override;
    void resolve_names(rs::context& ctx) override;
    [[nodiscard]] std::optional<ty::type_id> type_check(
      [[maybe_unused]] ty::context& ctx,
      [[maybe_unused]] sema::env& env) override;
    [[nodiscard]] std::string to_string() const override;

    [[nodiscard]]
    std::vector<expression*> get_children() override
    {
        std::vector<expression*> exprs;
        exprs.reserve(branches.size());
        for(auto& b: branches)
        {
            exprs.emplace_back(b.get());
        }
        return exprs;
    }
    [[nodiscard]]
    std::vector<const expression*> get_children() const override
    {
        std::vector<const expression*> exprs;
        exprs.reserve(branches.size());
        for(auto& b: branches)
        {
            exprs.emplace_back(b.get());
        }
        return exprs;
    }

    /**
     * Collect this macro into the macro environment.
     *
     * @param sema_env Semantic environment.
     * @param macro_env Macro collection / expansion environment.
     */
    void collect_macro(
      sema::env& sema_env,
      macro::env& macro_env) const;

    /**
     * Expand the macro given a macro invocation.
     *
     * @param ctx The code generation context.
     * @param invocation The macro invocation expression/context.
     * @return The macro expansion AST for the invocation.
     */
    std::unique_ptr<expression> expand(
      cg::context& ctx,
      const macro_invocation& invocation) const;
};

}    // namespace slang::ast
