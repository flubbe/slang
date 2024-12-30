/**
 * slang - a simple scripting language.
 *
 * abstract syntax tree.
 *
 * \author Felix Lubbe
 * \copyright Copyright (c) 2024
 * \license Distributed under the MIT software license (see accompanying LICENSE.txt).
 */

#pragma once

#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "directive.h"
#include "type.h"
#include "utils.h"

/*
 * Forward declarations for code generation.
 */
namespace slang::codegen
{
class context;
class function;
class value;
}    // namespace slang::codegen

namespace slang::typing
{
class context;
}    // namespace slang::typing

namespace slang::ast
{

namespace cg = slang::codegen;
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
    token_location loc;

    /** Namespace stack for the expression. */
    std::vector<std::string> namespace_stack;

    /** The expression's given or inferred type. Needs to be set by `type_check`. */
    std::optional<ty::type_info> expr_type;

public:
    /** No default constructor. */
    expression() = delete;

    /** Default copy/move constructors. */
    expression(const expression&) = default;
    expression(expression&&) = default;

    /**
     * Construct an expression.
     *
     * @param loc The expression location.
     */
    explicit expression(token_location loc)
    : loc{std::move(loc)}
    {
    }

    /** Default assignments. */
    expression& operator=(const expression&) = default;
    expression& operator=(expression&&) = default;

    /** Default destructor. */
    virtual ~expression() = default;

    /** Whether this expression needs stack cleanup. */
    virtual bool needs_pop() const
    {
        return false;
    }

    /** Whether this expression is an access of an array element. */
    virtual bool is_array_element_access() const
    {
        return false;
    }

    /** Whether this expression is a struct member access. */
    virtual bool is_struct_member_access() const
    {
        return false;
    }

    /**
     * Get the expression as a struct member access expression.
     *
     * @throws Throws a `std::runtime_error` if the expression is a struct member access expression.
     */
    virtual class access_expression* as_access_expression();

    /**
     * Get the expression as a struct member access expression.
     *
     * @throws Throws a `std::runtime_error` if the expression is a struct member access expression.
     */
    virtual const class access_expression* as_access_expression() const;

    /** Whether this expression is a `named_expression`. */
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
    virtual const class named_expression* as_named_expression() const;

    /**
     * Return whether this expression can be evaluated to a compile-time constant.
     *
     * @param ctx The context in which to check evaluation.
     */
    virtual bool is_const_eval(cg::context&) const
    {
        return false;
    }

    /**
     * Evaluate the compile-time constant.
     *
     * @returns Returns the result of the evaluation, or a `nullptr` if evaluation failed.
     */
    virtual std::unique_ptr<cg::value> evaluate(cg::context&) const;

    /**
     * Generate IR.
     *
     * @param ctx The context to use for code generation.
     * @returns The value of this expression or nullptr.
     */
    virtual std::unique_ptr<cg::value> generate_code(cg::context& ctx, memory_context mc = memory_context::none) const = 0;

    /**
     * Name collection.
     *
     * @param ctx Code generation context.
     * @param type_ctx Type system context.
     */
    virtual void collect_names([[maybe_unused]] cg::context& ctx, [[maybe_unused]] ty::context& type_ctx) const
    {
    }

    /**
     * Push a directive to the expression's directive stack (during code generation).
     *
     * @throws A `type_error` if the directive is not supported by the expression.
     *
     * @param ctx The code generation context.
     * @param name The directive's name.
     * @param args The directive's arguments.
     */
    virtual void push_directive(
      cg::context& ctx,
      const token& name,
      const std::vector<std::pair<token, token>>& args);

    /**
     * Pop the last directive from the expression's directive stack.
     *
     * @param ctx The code generation context.
     * @throws A `type_error` if the stack was empty.
     */
    virtual void pop_directive(cg::context& ctx);

    /**
     * Check whether a directive is supported by the expression.
     *
     * @param name Name of the directive.
     * @returns True if the directive is supported, and false otherwise.
     */
    virtual bool supports_directive([[maybe_unused]] const std::string& name) const;

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
    const std::vector<std::string>& get_namespace() const
    {
        return namespace_stack;
    }

    /** Return the namespace path, or `std::nullopt` if empty. */
    std::optional<std::string> get_namespace_path() const
    {
        if(namespace_stack.size() == 0)
        {
            return std::nullopt;
        }

        return slang::utils::join(namespace_stack, "::");
    }

    /**
     * Type checking.
     *
     * @throws A `type_error` if a type error was found.
     *
     * @param ctx Type system context.
     * @returns The expression's type or std::nullopt.
     */
    virtual std::optional<ty::type_info> type_check(ty::context& ctx) = 0;

    /** Get a readable string representation of the node. */
    virtual std::string to_string() const = 0;

    /** Get the expression's location. */
    const token_location& get_location() const
    {
        return loc;
    }
};

/** Any expression with a name. */
class named_expression : public expression
{
protected:
    /** The expression name. */
    token name;

public:
    /** Set the super class. */
    using super = expression;

    /** No default constructor. */
    named_expression() = delete;

    /** Default destructor. */
    virtual ~named_expression() = default;

    /** Default copy and move constructors. */
    named_expression(const named_expression&) = default;
    named_expression(named_expression&&) = default;

    /** Default assignment operators. */
    named_expression& operator=(const named_expression&) = default;
    named_expression& operator=(named_expression&&) = default;

    /**
     * Construct a named expression.
     *
     * @param loc The location.
     * @param tok The name token.
     */
    named_expression(token_location loc, token name)
    : expression{std::move(loc)}
    , name{std::move(name)}
    {
    }

    bool is_named_expression() const override
    {
        return true;
    }

    named_expression* as_named_expression() override
    {
        return this;
    }

    const named_expression* as_named_expression() const override
    {
        return this;
    }

    /** Get the name. */
    token get_name() const
    {
        return name;
    }
};

/** String, integer or floating-point literals. */
class literal_expression : public expression
{
    /** The literal token. */
    token tok;

public:
    /** Set the super class. */
    using super = expression;

    /** No default constructor. */
    literal_expression() = delete;

    /** Default destructor. */
    virtual ~literal_expression() = default;

    /** Default copy and move constructors. */
    literal_expression(const literal_expression&) = default;
    literal_expression(literal_expression&&) = default;

    /** Default assignment operators. */
    literal_expression& operator=(const literal_expression&) = default;
    literal_expression& operator=(literal_expression&&) = default;

    /**
     * Construct a literal expression.
     *
     * @param loc The location.
     * @param tok The token.
     */
    literal_expression(token_location loc, token tok)
    : expression{std::move(loc)}
    , tok{std::move(tok)}
    {
    }

    bool is_const_eval(cg::context&) const override
    {
        return true;
    }

    std::unique_ptr<cg::value> evaluate(cg::context& ctx) const override;

    std::unique_ptr<cg::value> generate_code(cg::context& ctx, memory_context mc = memory_context::none) const override;
    std::optional<ty::type_info> type_check(ty::context& ctx) override;
    std::string to_string() const override;

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
    token_location loc;

    /** The type name. */
    token type_name;

    /** Namespaces. */
    std::vector<token> namespace_stack;

    /** Whether the type is an array type. */
    bool array{false};

public:
    /** Default and deleted constructors. */
    type_expression() = delete;
    type_expression(const type_expression&) = default;
    type_expression(type_expression&&) = default;

    /** Default assignments. */
    type_expression& operator=(const type_expression&) = default;
    type_expression& operator=(type_expression&&) = default;

    /**
     * Construct a `type_expression`.
     *
     * @param loc The location.
     * @param type_name The (unqualified) type name.
     * @param namespace_stack Namespaces for type resolution.
     * @param is_array Whether the type is an array type.
     */
    type_expression(token_location loc, token type_name, std::vector<token> namespace_stack, bool is_array)
    : loc{std::move(loc)}
    , type_name{std::move(type_name)}
    , namespace_stack{std::move(namespace_stack)}
    , array{is_array}
    {
    }

    /** Get the location. */
    token_location get_location() const
    {
        return loc;
    }

    /** Returns the type name. */
    token get_name() const
    {
        return type_name;
    }

    /** Return the namespace path, or `std::nullopt` if empty. */
    std::optional<std::string> get_namespace_path() const;

    /** Return a readable representation of the type. */
    std::string to_string() const;

    /** Return whether the type is an array. */
    bool is_array() const
    {
        return array;
    }
};

/** A type cast expression. */
class type_cast_expression : public named_expression
{
    /** The expression. */
    std::unique_ptr<expression> expr;

    /** The target type. */
    std::unique_ptr<type_expression> target_type;

public:
    /** Set the super class. */
    using super = named_expression;

    /** No default constructor. */
    type_cast_expression() = delete;

    /** Default destructor. */
    virtual ~type_cast_expression() = default;

    /** Copy and move constructors. */
    type_cast_expression(const type_cast_expression&) = delete;
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
     */
    type_cast_expression(token_location loc, std::unique_ptr<expression> expr, std::unique_ptr<type_expression> target_type)
    : named_expression{std::move(loc),
                       expr->is_named_expression()
                         ? static_cast<named_expression*>(expr.get())->get_name()
                         : token{"<none>", {0, 0}}}    // FIXME we might not have a name.
    , expr{std::move(expr)}
    , target_type{std::move(target_type)}
    {
    }

    bool is_struct_member_access() const override
    {
        return expr->is_struct_member_access();
    }

    access_expression* as_access_expression() override
    {
        return expr->as_access_expression();
    }

    const access_expression* as_access_expression() const override
    {
        return expr->as_access_expression();
    }

    bool is_named_expression() const override
    {
        return expr->is_named_expression();
    }

    named_expression* as_named_expression() override
    {
        return expr->as_named_expression();
    }

    const named_expression* as_named_expression() const override
    {
        return expr->as_named_expression();
    }

    std::unique_ptr<cg::value> generate_code(cg::context& ctx, memory_context mc = memory_context::none) const override;
    std::optional<ty::type_info> type_check(ty::context& ctx) override;
    std::string to_string() const override;
};

/** Scope expression. */
class namespace_access_expression : public expression
{
    /** The scope name. */
    token name;

    /** The remaining expression. */
    std::unique_ptr<expression> expr;

public:
    /** Set the super class. */
    using super = expression;

    /** No default constructor. */
    namespace_access_expression() = delete;

    /** Default destructor. */
    virtual ~namespace_access_expression() = default;

    /** Copy and move constructors. */
    namespace_access_expression(const namespace_access_expression&) = delete;
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
    namespace_access_expression(token name, std::unique_ptr<expression> expr)
    : expression{name.location}
    , name{std::move(name)}
    , expr{std::move(expr)}
    {
    }

    virtual bool needs_pop() const override
    {
        return expr->needs_pop();
    }

    bool is_const_eval(cg::context& ctx) const override
    {
        return expr->is_const_eval(ctx);
    }

    std::unique_ptr<cg::value> evaluate(cg::context& ctx) const override;

    std::unique_ptr<cg::value> generate_code(cg::context& ctx, memory_context mc = memory_context::none) const override;
    std::optional<ty::type_info> type_check(ty::context& ctx) override;
    std::string to_string() const override;
};

/** Access expression. */
class access_expression : public expression
{
    /** Left-hand side of the access axpression. */
    std::unique_ptr<expression> lhs;

    /** Right-hand side of the access expression. */
    std::unique_ptr<expression> rhs;

    /** Struct type of the l.h.s. Set during type checking. */
    ty::type_info lhs_type;

public:
    /** Set the super class. */
    using super = expression;

    /** No default constructor. */
    access_expression() = delete;

    /** Default destructor. */
    virtual ~access_expression() = default;

    /** Copy and move constructors. */
    access_expression(const access_expression&) = delete;
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

    bool is_struct_member_access() const override
    {
        return true;
    }

    virtual access_expression* as_access_expression() override
    {
        return this;
    }

    virtual const access_expression* as_access_expression() const override
    {
        return this;
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
    std::optional<ty::type_info> type_check(ty::context& ctx) override;
    std::string to_string() const override;

    /** Return the accessed struct's type info. */
    ty::type_info get_struct_type() const
    {
        return lhs_type;
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

    /** No default constructor. */
    import_expression() = delete;

    /** Default destructor. */
    virtual ~import_expression() = default;

    /** Default copy and move constructors. */
    import_expression(const import_expression&) = default;
    import_expression(import_expression&&) = default;

    /** Default assignment operators. */
    import_expression& operator=(const import_expression&) = default;
    import_expression& operator=(import_expression&&) = default;

    /**
     * Construct an import expression.
     *
     * @param in_path The import path.
     */
    explicit import_expression(std::vector<token> in_path)
    : expression{in_path[0].location}
    , path{std::move(in_path)}
    {
    }

    std::unique_ptr<cg::value> generate_code(cg::context& ctx, memory_context mc = memory_context::none) const override;
    void collect_names(cg::context& ctx, ty::context& type_ctx) const override;
    std::optional<ty::type_info> type_check(ty::context& ctx) override;
    std::string to_string() const override;
};

/** Directives. Directives have names and contain a list of key-value pairs as arguments. */
class directive_expression : public named_expression
{
    /** Key-value pairs. */
    std::vector<std::pair<token, token>> args;

    /** An optional expression the directive applies to. */
    std::unique_ptr<expression> expr;

public:
    /** Set the super class. */
    using super = named_expression;

    /** No default constructor. */
    directive_expression() = delete;

    /** Default destructor. */
    virtual ~directive_expression() = default;

    /** Default copy and move constructors. */
    directive_expression(const directive_expression&) = delete;
    directive_expression(directive_expression&&) = default;

    /** Default assignment operators. */
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

    bool needs_pop() const override
    {
        return expr->needs_pop();
    }

    std::unique_ptr<cg::value> generate_code(cg::context& ctx, memory_context mc = memory_context::none) const override;
    void collect_names(cg::context& ctx, ty::context& type_ctx) const override;
    std::optional<ty::type_info> type_check(ty::context& ctx) override;
    std::string to_string() const override;
};

/** Variable references. */
class variable_reference_expression : public named_expression
{
    /** An optional expression for element access. */
    std::unique_ptr<expression> element_expr;

public:
    /** Set the super class. */
    using super = named_expression;

    /** No default constructor. */
    variable_reference_expression() = delete;

    /** Default destructor. */
    virtual ~variable_reference_expression() = default;

    /** Default copy and move constructors. */
    variable_reference_expression(const variable_reference_expression&) = delete;
    variable_reference_expression(variable_reference_expression&&) = default;

    /** Default assignment operators. */
    variable_reference_expression& operator=(const variable_reference_expression&) = delete;
    variable_reference_expression& operator=(variable_reference_expression&&) = default;

    /**
     * Construct a variable reference expression.
     *
     * @param name The variable's name.
     * @param element_expr An optional expression for array element access.
     */
    variable_reference_expression(token name, std::unique_ptr<expression> element_expr = nullptr)
    : named_expression{name.location, std::move(name)}
    , element_expr{std::move(element_expr)}
    {
    }

    bool is_array_element_access() const override
    {
        return static_cast<bool>(element_expr);
    }

    bool is_const_eval(cg::context& ctx) const override;
    std::unique_ptr<cg::value> evaluate(cg::context& ctx) const override;

    std::unique_ptr<cg::value> generate_code(cg::context& ctx, memory_context mc = memory_context::none) const override;
    std::optional<ty::type_info> type_check(ty::context& ctx) override;
    std::string to_string() const override;

    /** Get the value of the object. */
    cg::value get_value(cg::context& ctx) const;
};

/** Variable declaration. */
class variable_declaration_expression : public named_expression
{
    /** The variable's type. */
    std::unique_ptr<ast::type_expression> type;

    /** An optional initializer expression. */
    std::unique_ptr<ast::expression> expr;

public:
    /** Set the super class. */
    using super = named_expression;

    /** No default constructor. */
    variable_declaration_expression() = delete;

    /** Default destructor. */
    virtual ~variable_declaration_expression() = default;

    /** Copy and move constructors. */
    variable_declaration_expression(const variable_declaration_expression&) = delete;
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
    variable_declaration_expression(token_location loc, token name, std::unique_ptr<ast::type_expression> type, std::unique_ptr<ast::expression> expr)
    : named_expression{std::move(loc), std::move(name)}
    , type{std::move(type)}
    , expr{std::move(expr)}
    {
    }

    std::unique_ptr<cg::value> generate_code(cg::context& ctx, memory_context mc = memory_context::none) const override;
    std::optional<ty::type_info> type_check(ty::context& ctx) override;
    std::string to_string() const override;

    /** Get the variable's type. */
    const std::unique_ptr<ast::type_expression>& get_type() const
    {
        return type;
    }

    /** Whether this variable is an array. */
    bool is_array() const
    {
        return type->is_array();
    }
};

/** Constant declaration. */
class constant_declaration_expression : public named_expression
{
    /** The constant's type. */
    std::unique_ptr<ast::type_expression> type;

    /** The initializer expression. */
    std::unique_ptr<ast::expression> expr;

public:
    /** Set the super class. */
    using super = named_expression;

    /** No default constructor. */
    constant_declaration_expression() = delete;

    /** Default destructor. */
    virtual ~constant_declaration_expression() = default;

    /** Copy and move constructors. */
    constant_declaration_expression(const constant_declaration_expression&) = delete;
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
    constant_declaration_expression(token_location loc,
                                    token name,
                                    std::unique_ptr<ast::type_expression> type,
                                    std::unique_ptr<ast::expression> expr)
    : named_expression{std::move(loc), std::move(name)}
    , type{std::move(type)}
    , expr{std::move(expr)}
    {
    }

    void push_directive(
      cg::context& ctx,
      const token& name,
      const std::vector<std::pair<token, token>>& args) override;

    std::unique_ptr<cg::value> generate_code(cg::context& ctx, memory_context mc = memory_context::none) const override;
    std::optional<ty::type_info> type_check(ty::context& ctx) override;
    std::string to_string() const override;

    /** Get the constant's type. */
    const std::unique_ptr<ast::type_expression>& get_type() const
    {
        return type;
    }
};

/** An array initializer expression. */
class array_initializer_expression : public expression
{
    /** Initializer expressions for each element. */
    std::vector<std::unique_ptr<ast::expression>> exprs;

public:
    /** Set the super class. */
    using super = expression;

    /** No default constructor. */
    array_initializer_expression() = delete;

    /** Default destructor. */
    virtual ~array_initializer_expression() = default;

    /** Copy and move constructors. */
    array_initializer_expression(const array_initializer_expression&) = delete;
    array_initializer_expression(array_initializer_expression&&) = default;

    /** Assignment operators. */
    array_initializer_expression& operator=(const array_initializer_expression&) = delete;
    array_initializer_expression& operator=(array_initializer_expression&&) = default;

    /**
     * Construct an array initializer expression.
     */
    array_initializer_expression(token_location loc, std::vector<std::unique_ptr<ast::expression>> exprs)
    : expression{std::move(loc)}
    , exprs{std::move(exprs)}
    {
    }

    std::unique_ptr<cg::value> generate_code(cg::context& ctx, memory_context mc = memory_context::none) const override;
    std::optional<ty::type_info> type_check(ty::context& ctx) override;
    std::string to_string() const override;
};

/** Struct definition. */
class struct_definition_expression : public named_expression
{
    /** The struct's members. */
    std::vector<std::unique_ptr<variable_declaration_expression>> members;

public:
    /** Set the super class. */
    using super = named_expression;

    /** No default constructor. */
    struct_definition_expression() = delete;

    /** Default destructor. */
    virtual ~struct_definition_expression() = default;

    /** Copy and move constructors. */
    struct_definition_expression(const struct_definition_expression&) = delete;
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
    struct_definition_expression(token_location loc, token name, std::vector<std::unique_ptr<variable_declaration_expression>> members)
    : named_expression{std::move(loc), std::move(name)}
    , members{std::move(members)}
    {
    }

    std::unique_ptr<cg::value> generate_code(cg::context& ctx, memory_context mc = memory_context::none) const override;
    void collect_names(cg::context& ctx, ty::context& type_ctx) const override;
    bool supports_directive(const std::string& name) const override;
    std::optional<ty::type_info> type_check(ty::context& ctx) override;
    std::string to_string() const override;
};

/** Anonymous struct initialization. */
class struct_anonymous_initializer_expression : public named_expression
{
    /** Anonymous initializers. */
    std::vector<std::unique_ptr<expression>> initializers;

public:
    /** Set the super class. */
    using super = named_expression;

    /** No default constructor. */
    struct_anonymous_initializer_expression() = delete;

    /** Default destructor. */
    virtual ~struct_anonymous_initializer_expression() = default;

    /** Copy and move constructors. */
    struct_anonymous_initializer_expression(const struct_anonymous_initializer_expression&) = delete;
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
    struct_anonymous_initializer_expression(token name, std::vector<std::unique_ptr<expression>> initializers)
    : named_expression{name.location, std::move(name)}
    , initializers{std::move(initializers)}
    {
    }

    std::unique_ptr<cg::value> generate_code(cg::context& ctx, memory_context mc = memory_context::none) const override;
    std::optional<ty::type_info> type_check(ty::context& ctx) override;
    std::string to_string() const override;
};

/** Named struct initialization. */
class struct_named_initializer_expression : public named_expression
{
    /** Initialized member names. */
    std::vector<std::unique_ptr<expression>> member_names;

    /** Initializers. */
    std::vector<std::unique_ptr<expression>> initializers;

public:
    /** Set the super class. */
    using super = named_expression;

    /** No default constructor. */
    struct_named_initializer_expression() = delete;

    /** Default destructor. */
    virtual ~struct_named_initializer_expression() = default;

    /** Copy and move constructors. */
    struct_named_initializer_expression(const struct_named_initializer_expression&) = delete;
    struct_named_initializer_expression(struct_named_initializer_expression&&) = default;

    /** Assignment operators. */
    struct_named_initializer_expression& operator=(const struct_named_initializer_expression&) = delete;
    struct_named_initializer_expression& operator=(struct_named_initializer_expression&&) = default;

    /**
     * Construct a named struct initialization.
     *
     * @param name The struct's name.
     * @param member_names The initialized member names.
     * @param members The struct's members.
     */
    struct_named_initializer_expression(token name, std::vector<std::unique_ptr<expression>> member_names, std::vector<std::unique_ptr<expression>> initializers)
    : named_expression{name.location, std::move(name)}
    , member_names{std::move(member_names)}
    , initializers{std::move(initializers)}
    {
    }

    std::unique_ptr<cg::value> generate_code(cg::context& ctx, memory_context mc = memory_context::none) const override;
    std::optional<ty::type_info> type_check(ty::context& ctx) override;
    std::string to_string() const override;
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

    /** No default constructor. */
    binary_expression() = delete;

    /** Default destructor. */
    virtual ~binary_expression() = default;

    /** Copy and move constructors. */
    binary_expression(const binary_expression&) = delete;
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
    binary_expression(token_location loc, token op, std::unique_ptr<expression> lhs, std::unique_ptr<expression> rhs)
    : expression{std::move(loc)}
    , op{std::move(op)}
    , lhs{std::move(lhs)}
    , rhs{std::move(rhs)}
    {
    }

    bool needs_pop() const override;

    bool is_const_eval(cg::context& ctx) const override;
    std::unique_ptr<cg::value> evaluate(cg::context& ctx) const override;

    std::unique_ptr<cg::value> generate_code(cg::context& ctx, memory_context mc = memory_context::none) const override;
    std::optional<ty::type_info> type_check(ty::context& ctx) override;
    std::string to_string() const override;
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

    /** No default constructor. */
    unary_expression() = delete;

    /** Default destructor. */
    virtual ~unary_expression() = default;

    /** Copy and move constructors. */
    unary_expression(const unary_expression&) = delete;
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
    unary_expression(token_location loc, token op, std::unique_ptr<expression> operand)
    : expression{std::move(loc)}
    , op{std::move(op)}
    , operand{std::move(operand)}
    {
    }

    bool is_const_eval(cg::context& ctx) const override;
    std::unique_ptr<cg::value> evaluate(cg::context& ctx) const override;

    std::unique_ptr<cg::value> generate_code(cg::context& ctx, memory_context mc = memory_context::none) const override;
    std::optional<ty::type_info> type_check(ty::context& ctx) override;
    std::string to_string() const override;
};

class new_expression : public expression
{
    /** The type. */
    token type;

    /** Array length expression. */
    std::unique_ptr<expression> expr;

public:
    /** Set the super class. */
    using super = expression;

    /** No default constructor. */
    new_expression() = delete;

    /** Default destructor. */
    virtual ~new_expression() = default;

    /** Copy and move constructors. */
    new_expression(const new_expression&) = delete;
    new_expression(new_expression&&) = default;

    /** Assignment operators. */
    new_expression& operator=(const new_expression&) = delete;
    new_expression& operator=(new_expression&&) = default;

    /**
     * Construct a 'new' expression.
     *
     * @param loc The location.
     * @param type The type to be allocated.
     * @param expr The array length expression.
     */
    new_expression(token_location loc, token type, std::unique_ptr<expression> expr)
    : expression{std::move(loc)}
    , type{std::move(type)}
    , expr{std::move(expr)}
    {
    }

    std::unique_ptr<cg::value> generate_code(cg::context& ctx, memory_context mc = memory_context::none) const override;
    std::optional<ty::type_info> type_check(ty::context& ctx) override;
    std::string to_string() const override;
};

/** 'null' expression. */
class null_expression : public expression
{
public:
    /** Set the super class. */
    using super = expression;

    /** No default constructor. */
    null_expression() = delete;

    /** Default destructor. */
    virtual ~null_expression() = default;

    /** Copy and move constructors. */
    null_expression(const null_expression&) = delete;
    null_expression(null_expression&&) = default;

    /** Assignment operators. */
    null_expression& operator=(const null_expression&) = delete;
    null_expression& operator=(null_expression&&) = default;

    /**
     * Construct a 'null' expression.
     *
     * @param loc The location.
     */
    explicit null_expression(token_location loc)
    : expression{std::move(loc)}
    {
    }

    std::unique_ptr<cg::value> generate_code(cg::context& ctx, memory_context mc = memory_context::none) const override;
    std::optional<ty::type_info> type_check(ty::context& ctx) override;
    std::string to_string() const override;
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

    /** No default constructor. */
    postfix_expression() = delete;

    /** Default destructor. */
    virtual ~postfix_expression() = default;

    /** Copy and move constructors. */
    postfix_expression(const postfix_expression&) = delete;
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

    bool needs_pop() const override
    {
        return true;
    }

    std::unique_ptr<cg::value> generate_code(cg::context& ctx, memory_context mc = memory_context::none) const override;
    std::optional<ty::type_info> type_check(ty::context& ctx) override;
    std::string to_string() const override;
};

/** Function prototype. */
class prototype_ast
{
    /** Token location. */
    token_location loc;

    /** The function name. */
    token name;

    /** The function's arguments as pairs `(name, type)`. */
    std::vector<std::pair<token, std::unique_ptr<type_expression>>> args;

    /** The function's return type as a pair. */
    std::unique_ptr<type_expression> return_type;

public:
    /** No default constructor. */
    prototype_ast() = delete;

    /** Default destructor. */
    virtual ~prototype_ast() = default;

    /** Default copy and move constructors. */
    prototype_ast(const prototype_ast&) = delete;
    prototype_ast(prototype_ast&&) = default;

    /** Default assignment operators. */
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
    prototype_ast(token_location loc,
                  token name,
                  std::vector<std::pair<token, std::unique_ptr<type_expression>>> args,
                  std::unique_ptr<type_expression> return_type)
    : loc{std::move(loc)}
    , name{std::move(name)}
    , args{std::move(args)}
    , return_type{std::move(return_type)}
    {
    }

    cg::function* generate_code(cg::context& ctx, memory_context mc = memory_context::none) const;
    void generate_native_binding(const std::string& lib_name, cg::context& ctx) const;
    void collect_names(cg::context& ctx, ty::context& type_ctx) const;
    void type_check(ty::context& ctx);
    void finish_type_check(ty::context& ctx);
    std::string to_string() const;

    token get_name() const
    {
        return name;
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

    /** No default constructor. */
    block() = delete;

    /** Default destructor. */
    virtual ~block() = default;

    /** Copy and move constructors. */
    block(const block&) = delete;
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
    block(token_location loc, std::vector<std::unique_ptr<expression>> exprs)
    : expression{std::move(loc)}
    , exprs{std::move(exprs)}
    {
    }

    std::unique_ptr<cg::value> generate_code(cg::context& ctx, memory_context mc = memory_context::none) const override;
    void collect_names(cg::context& ctx, ty::context& type_ctx) const override;
    std::optional<ty::type_info> type_check(ty::context& ctx) override;
    std::string to_string() const override;
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

    /** No default constructor. */
    function_expression() = delete;

    /** Default destructor. */
    virtual ~function_expression() = default;

    /** Copy and move constructors. */
    function_expression(const function_expression&) = delete;
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
    function_expression(token_location loc, std::unique_ptr<prototype_ast> prototype, std::unique_ptr<block> body)
    : expression{std::move(loc)}
    , prototype{std::move(prototype)}
    , body{std::move(body)}
    {
    }

    std::unique_ptr<cg::value> generate_code(cg::context& ctx, memory_context mc = memory_context::none) const override;
    void collect_names(cg::context& ctx, ty::context& type_ctx) const override;
    bool supports_directive(const std::string& name) const override;
    std::optional<ty::type_info> type_check(ty::context& ctx) override;
    std::string to_string() const override;
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
    ty::type_info return_type;

public:
    /** Set the super class. */
    using super = expression;

    /** No default constructor. */
    call_expression() = delete;

    /** Default destructor. */
    virtual ~call_expression() = default;

    /** Default copy and move constructors. */
    call_expression(const call_expression&) = delete;
    call_expression(call_expression&&) = default;

    /** Default assignment operators. */
    call_expression& operator=(const call_expression&) = delete;
    call_expression& operator=(call_expression&&) = default;

    /**
     * Construct a function call.
     *
     * @param callee The callee's name.
     * @param args The argument expressions.
     */
    call_expression(token callee, std::vector<std::unique_ptr<expression>> args, std::unique_ptr<expression> index_expr = nullptr)
    : expression{callee.location}
    , callee{std::move(callee)}
    , args{std::move(args)}
    , index_expr{std::move(index_expr)}
    {
    }

    bool needs_pop() const override
    {
        return return_type.to_string() != "void";
    }

    std::unique_ptr<cg::value> generate_code(cg::context& ctx, memory_context mc = memory_context::none) const override;
    std::optional<ty::type_info> type_check(ty::context& ctx) override;
    std::string to_string() const override;
};

/*
 * Statements.
 */

/** Return statement. */
class return_statement : public expression
{
    /** (Optional) returned expression. */
    std::unique_ptr<ast::expression> expr;

public:
    /** Set the super class. */
    using super = expression;

    /** No default constructor. */
    return_statement() = delete;

    /** Default destructor. */
    virtual ~return_statement() = default;

    /** Copy and move constructors. */
    return_statement(const return_statement&) = delete;
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
    return_statement(token_location loc, std::unique_ptr<ast::expression> expr)
    : expression{std::move(loc)}
    , expr{std::move(expr)}
    {
    }

    std::unique_ptr<cg::value> generate_code(cg::context& ctx, memory_context mc = memory_context::none) const override;
    std::optional<ty::type_info> type_check(ty::context& ctx) override;
    std::string to_string() const override;
};

/** If statement. */
class if_statement : public expression
{
    /** The condition. */
    std::unique_ptr<ast::expression> condition;

    /** If block. */
    std::unique_ptr<ast::expression> if_block;

    /** (Optinal) else block. */
    std::unique_ptr<ast::expression> else_block;

public:
    /** Set the super class. */
    using super = expression;

    /** No default constructor. */
    if_statement() = delete;

    /** Default destructor. */
    virtual ~if_statement() = default;

    /** Copy and move constructors. */
    if_statement(const if_statement&) = delete;
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
    if_statement(token_location loc, std::unique_ptr<ast::expression> condition, std::unique_ptr<ast::expression> if_block, std::unique_ptr<ast::expression> else_block)
    : expression{std::move(loc)}
    , condition{std::move(condition)}
    , if_block{std::move(if_block)}
    , else_block{std::move(else_block)}
    {
    }

    std::unique_ptr<cg::value> generate_code(cg::context& ctx, memory_context mc = memory_context::none) const override;
    std::optional<ty::type_info> type_check(ty::context& ctx) override;
    std::string to_string() const override;
};

/** While statement. */
class while_statement : public expression
{
    /** The condition. */
    std::unique_ptr<ast::expression> condition;

    /** While block. */
    std::unique_ptr<ast::expression> while_block;

public:
    /** Set the super class. */
    using super = expression;

    /** No default constructor. */
    while_statement() = delete;

    /** Default destructor. */
    virtual ~while_statement() = default;

    /** Copy and move constructors. */
    while_statement(const while_statement&) = delete;
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
    while_statement(token_location loc, std::unique_ptr<ast::expression> condition, std::unique_ptr<ast::expression> while_block)
    : expression{std::move(loc)}
    , condition{std::move(condition)}
    , while_block{std::move(while_block)}
    {
    }

    std::unique_ptr<cg::value> generate_code(cg::context& ctx, memory_context mc = memory_context::none) const override;
    std::optional<ty::type_info> type_check(ty::context& ctx) override;
    std::string to_string() const override;
};

/** Break statement. */
class break_statement : public expression
{
public:
    /** Set the super class. */
    using super = expression;

    /** No default constructor. */
    break_statement() = delete;

    /** Default destructor. */
    virtual ~break_statement() = default;

    /** Default copy and move constructors. */
    break_statement(const break_statement&) = default;
    break_statement(break_statement&&) = default;

    /** Default assignment operators. */
    break_statement& operator=(const break_statement&) = default;
    break_statement& operator=(break_statement&&) = default;

    /**
     * Construct a break statement.
     *
     * @param loc The location.
     */
    explicit break_statement(token_location loc)
    : expression{std::move(loc)}
    {
    }

    std::unique_ptr<cg::value> generate_code(cg::context& ctx, memory_context mc = memory_context::none) const override;

    std::optional<ty::type_info> type_check([[maybe_unused]] ty::context& ctx) override
    {
        return std::nullopt;
    }

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

    /** No default constructor. */
    continue_statement() = delete;

    /** Default destructor. */
    virtual ~continue_statement() = default;

    /** Default copy and move constructors. */
    continue_statement(const continue_statement&) = default;
    continue_statement(continue_statement&&) = default;

    /** Default assignment operators. */
    continue_statement& operator=(const continue_statement&) = default;
    continue_statement& operator=(continue_statement&&) = default;

    /**
     * Construct a continue statement.
     *
     * @param loc The location.
     */
    explicit continue_statement(token_location loc)
    : expression{std::move(loc)}
    {
    }

    std::unique_ptr<cg::value> generate_code(cg::context& ctx, memory_context mc = memory_context::none) const override;

    std::optional<ty::type_info> type_check([[maybe_unused]] ty::context& ctx) override
    {
        return std::nullopt;
    }

    std::string to_string() const override
    {
        return "Continue()";
    }
};

}    // namespace slang::ast