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
#include <tuple>
#include <utility>
#include <vector>

#include "token.h"

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

/** A directive that modifies an expression. */
struct directive
{
    /** The directive's name. */
    token name;

    /** The directive's arguments. */
    std::vector<std::pair<token, token>> args;

    /** Default constructors. */
    directive() = default;
    directive(const directive&) = default;
    directive(directive&&) = default;

    /** Default assignments. */
    directive& operator=(const directive&) = default;
    directive& operator=(directive&&) = default;

    /** Default constructor. */
    directive(token name, std::vector<std::pair<token, token>> args)
    : name{std::move(name)}
    , args{std::move(args)}
    {
    }
};

/** Base class for all expression nodes. */
class expression
{
protected:
    /** The expression's location. */
    token_location loc;

    /** The expression's directive stack. */
    std::vector<directive> directive_stack;

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
    expression(token_location loc)
    : loc{std::move(loc)}
    {
    }

    /** Default assignments. */
    expression& operator=(const expression&) = default;
    expression& operator=(expression&&) = default;

    /** Default destructor. */
    virtual ~expression() = default;

    /**
     * Generate IR.
     *
     * @param ctx The context to use for code generation.
     * @returns The value of this expression or nullptr.
     */
    virtual std::unique_ptr<slang::codegen::value> generate_code(slang::codegen::context& ctx, memory_context mc = memory_context::none) const = 0;

    /**
     * Name collection.
     *
     * @param ctx Code generation context.
     * @param type_ctx Type system context.
     */
    virtual void collect_names(cg::context& ctx, ty::context& type_ctx) const
    {
    }

    /**
     * Push a directive to the expression's directive stack (during code generation).
     *
     * @throws A type_error if the directive is not supported by the expression.
     *
     * @param name The directive's name.
     * @param args The directive's arguments.
     */
    void push_directive(const token& name, const std::vector<std::pair<token, token>>& args);

    /**
     * Pop the last directive from the expression's directive stack.
     *
     * @throws A type_error if the stack was empty.
     */
    void pop_directive();

    /**
     * Check whether a directive is supported by the expression.
     *
     * @returns True if the directive is supported, and false otherwise.
     */
    virtual bool supports_directive(const std::string& s) const
    {
        return false;
    }

    /**
     * Get a list of matching directives.
     *
     * @param name The directive name.
     * @returns A vector of directives matching the name, or an empty vector if none are found.
     */
    std::vector<directive> get_directives(const std::string& s) const;

    /**
     * Type checking.
     *
     * @throws A type_error if a type error was found.
     *
     * @param ctx Type system context.
     * @returns A string representation of the expression's type or std::nullopt.
     */
    virtual std::optional<std::string> type_check(slang::typing::context& ctx) const = 0;

    /** Get a readable string representation of the node. */
    virtual std::string to_string() const = 0;

    /** Get the expression's location. */
    const token_location& get_location() const
    {
        return loc;
    }
};

/** String, integer or floating-point literals. */
class literal_expression : public expression
{
    /** The literal token. */
    token tok;

public:
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

    std::unique_ptr<slang::codegen::value> generate_code(slang::codegen::context& ctx, memory_context mc = memory_context::none) const override;
    std::optional<std::string> type_check(slang::typing::context& ctx) const override;
    std::string to_string() const override;
};

/** A type cast expression. */
class type_cast_expression : public expression
{
    /** The expression. */
    std::unique_ptr<expression> expr;

    /** The target type. */
    token target_type;

public:
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
    type_cast_expression(token_location loc, std::unique_ptr<expression> expr, token target_type)
    : expression{std::move(loc)}
    , expr{std::move(expr)}
    , target_type{std::move(target_type)}
    {
    }

    std::unique_ptr<slang::codegen::value> generate_code(slang::codegen::context& ctx, memory_context mc = memory_context::none) const override;
    std::optional<std::string> type_check(slang::typing::context& ctx) const override;
    std::string to_string() const override;
};

/** Scope expression. */
class scope_expression : public expression
{
    /** The scope name. */
    token name;

    /** The remaining expression. */
    std::unique_ptr<expression> expr;

public:
    /** No default constructor. */
    scope_expression() = delete;

    /** Default destructor. */
    virtual ~scope_expression() = default;

    /** Copy and move constructors. */
    scope_expression(const scope_expression&) = delete;
    scope_expression(scope_expression&&) = default;

    /** Assignment operators. */
    scope_expression& operator=(const scope_expression&) = delete;
    scope_expression& operator=(scope_expression&&) = default;

    /**
     * Construct a scope expression.
     *
     * @param name The scope's name.
     * @param expr An expression.
     */
    scope_expression(token name, std::unique_ptr<expression> expr)
    : expression{name.location}
    , name{std::move(name)}
    , expr{std::move(expr)}
    {
    }

    std::unique_ptr<slang::codegen::value> generate_code(slang::codegen::context& ctx, memory_context mc = memory_context::none) const override;
    std::optional<std::string> type_check(slang::typing::context& ctx) const override;
    std::string to_string() const override;
};

/** Access expression. */
class access_expression : public expression
{
    /** The namespace name. */
    token name;

    /** The resolved expression. */
    std::unique_ptr<expression> expr;

public:
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
     * @param identifier The identifier.
     * @param expr An identifier expression.
     */
    access_expression(token identifier, std::unique_ptr<expression> expr)
    : expression{identifier.location}
    , name{std::move(identifier)}
    , expr{std::move(expr)}
    {
    }

    std::unique_ptr<slang::codegen::value> generate_code(slang::codegen::context& ctx, memory_context mc = memory_context::none) const override;
    std::optional<std::string> type_check(slang::typing::context& ctx) const override;
    std::string to_string() const override;
};

/** Import statements. */
class import_expression : public expression
{
    /** The import path. */
    std::vector<token> path;

public:
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
    import_expression(std::vector<token> in_path)
    : expression{in_path[0].location}
    , path{std::move(in_path)}
    {
    }

    std::unique_ptr<slang::codegen::value> generate_code(slang::codegen::context& ctx, memory_context mc = memory_context::none) const override;
    void collect_names(cg::context& ctx, ty::context& type_ctx) const override;
    std::optional<std::string> type_check(slang::typing::context& ctx) const override;
    std::string to_string() const override;
};

/** Directives. Directives have names and contain a list of key-value pairs as arguments. */
class directive_expression : public expression
{
    /** The name. */
    token name;

    /** Key-value pairs. */
    std::vector<std::pair<token, token>> args;

    /** An optional expression the directive applies to. */
    std::unique_ptr<expression> expr;

public:
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
    : expression{name.location}
    , name{std::move(name)}
    , args{std::move(args)}
    , expr{std::move(expr)}
    {
    }

    std::unique_ptr<slang::codegen::value> generate_code(slang::codegen::context& ctx, memory_context mc = memory_context::none) const override;
    std::optional<std::string> type_check(slang::typing::context& ctx) const override;
    std::string to_string() const override;
};

/** Variable references. */
class variable_reference_expression : public expression
{
    /** The variable name. */
    token name;

public:
    /** No default constructor. */
    variable_reference_expression() = delete;

    /** Default destructor. */
    virtual ~variable_reference_expression() = default;

    /** Default copy and move constructors. */
    variable_reference_expression(const variable_reference_expression&) = default;
    variable_reference_expression(variable_reference_expression&&) = default;

    /** Default assignment operators. */
    variable_reference_expression& operator=(const variable_reference_expression&) = default;
    variable_reference_expression& operator=(variable_reference_expression&&) = default;

    /**
     * Construct a variable reference expression.
     *
     * @param name The variable's name.
     */
    variable_reference_expression(token name)
    : expression{name.location}
    , name{std::move(name)}
    {
    }

    std::unique_ptr<slang::codegen::value> generate_code(slang::codegen::context& ctx, memory_context mc = memory_context::none) const override;
    std::optional<std::string> type_check(slang::typing::context& ctx) const override;
    std::string to_string() const override;
};

/** Variable declaration. */
class variable_declaration_expression : public expression
{
    /** The variable name. */
    token name;

    /** The variable's type. */
    token type;

    /** An optional initializer expression. */
    std::unique_ptr<ast::expression> expr;

public:
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
    variable_declaration_expression(token_location loc, token name, token type, std::unique_ptr<ast::expression> expr)
    : expression{std::move(loc)}
    , name{std::move(name)}
    , type{std::move(type)}
    , expr{std::move(expr)}
    {
    }

    std::unique_ptr<slang::codegen::value> generate_code(slang::codegen::context& ctx, memory_context mc = memory_context::none) const override;
    std::optional<std::string> type_check(slang::typing::context& ctx) const override;
    std::string to_string() const override;

    /** Get the variable's name. */
    const token& get_name() const
    {
        return name;
    }

    /** Get the variable's type. */
    const token& get_type() const
    {
        return type;
    }
};

/** Struct definition. */
class struct_definition_expression : public expression
{
    /** The struct's name. */
    token name;

    /** The struct's members. */
    std::vector<std::unique_ptr<variable_declaration_expression>> members;

public:
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
    : expression{std::move(loc)}
    , name{std::move(name)}
    , members{std::move(members)}
    {
    }

    std::unique_ptr<slang::codegen::value> generate_code(slang::codegen::context& ctx, memory_context mc = memory_context::none) const override;
    void collect_names(cg::context& ctx, ty::context& type_ctx) const override;
    std::optional<std::string> type_check(slang::typing::context& ctx) const override;
    std::string to_string() const override;
};

/** Anonymous struct initialization. */
class struct_anonymous_initializer_expression : public expression
{
    /** The struct's name. */
    token name;

    /** Anonymous initializers. */
    std::vector<std::unique_ptr<expression>> initializers;

public:
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
    : expression{name.location}
    , name{std::move(name)}
    , initializers{std::move(initializers)}
    {
    }

    std::unique_ptr<slang::codegen::value> generate_code(slang::codegen::context& ctx, memory_context mc = memory_context::none) const override;
    std::optional<std::string> type_check(slang::typing::context& ctx) const override;
    std::string to_string() const override;
};

/** Named struct initialization. */
class struct_named_initializer_expression : public expression
{
    /** The struct's name. */
    token name;

    /** Initialized member names. */
    std::vector<std::unique_ptr<expression>> member_names;

    /** Initializers. */
    std::vector<std::unique_ptr<expression>> initializers;

public:
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
    : expression{name.location}
    , name{std::move(name)}
    , member_names{std::move(member_names)}
    , initializers{std::move(initializers)}
    {
    }

    std::unique_ptr<slang::codegen::value> generate_code(slang::codegen::context& ctx, memory_context mc = memory_context::none) const override;
    std::optional<std::string> type_check(slang::typing::context& ctx) const override;
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

    std::unique_ptr<slang::codegen::value> generate_code(slang::codegen::context& ctx, memory_context mc = memory_context::none) const override;
    std::optional<std::string> type_check(slang::typing::context& ctx) const override;
    std::string to_string() const override;
};

/** Unary operators. */
class unary_ast : public expression
{
    /** The operator. */
    token op;

    /** The operand. */
    std::unique_ptr<expression> operand;

public:
    /** No default constructor. */
    unary_ast() = delete;

    /** Default destructor. */
    virtual ~unary_ast() = default;

    /** Copy and move constructors. */
    unary_ast(const unary_ast&) = delete;
    unary_ast(unary_ast&&) = default;

    /** Assignment operators. */
    unary_ast& operator=(const unary_ast&) = delete;
    unary_ast& operator=(unary_ast&&) = default;

    /**
     * Construct a unary expression.
     *
     * @param loc The location.
     * @param op The operator.
     * @param operand The operand.
     */
    unary_ast(token_location loc, token op, std::unique_ptr<expression> operand)
    : expression{std::move(loc)}
    , op{std::move(op)}
    , operand{std::move(operand)}
    {
    }

    std::unique_ptr<slang::codegen::value> generate_code(slang::codegen::context& ctx, memory_context mc = memory_context::none) const override;
    std::optional<std::string> type_check(slang::typing::context& ctx) const override;
    std::string to_string() const override;
};

/** Function prototype. */
class prototype_ast
{
    /** Token location. */
    token_location loc;

    /** The function name. */
    token name;

    /** The function's arguments as tuples (name, type). */
    std::vector<std::pair<token, token>> args;

    /** The function's return type. */
    token return_type;

public:
    /** No default constructor. */
    prototype_ast() = delete;

    /** Default destructor. */
    virtual ~prototype_ast() = default;

    /** Default copy and move constructors. */
    prototype_ast(const prototype_ast&) = default;
    prototype_ast(prototype_ast&&) = default;

    /** Default assignment operators. */
    prototype_ast& operator=(const prototype_ast&) = default;
    prototype_ast& operator=(prototype_ast&&) = default;

    /**
     * Construct a function prototype.
     *
     * @param loc The location.
     * @param name The function's name.
     * @param args The function's arguments as a vector of tuples (location, type_identifier, name).
     * @param return_type The function's return type.
     */
    prototype_ast(token_location loc, token name, std::vector<std::pair<token, token>> args, token return_type)
    : loc{std::move(loc)}
    , name{std::move(name)}
    , args{std::move(args)}
    , return_type{std::move(return_type)}
    {
    }

    slang::codegen::function* generate_code(slang::codegen::context& ctx, memory_context mc = memory_context::none) const;
    void generate_native_binding(const std::string& lib_name, slang::codegen::context& ctx) const;
    void collect_names(cg::context& ctx, ty::context& type_ctx) const;
    void type_check(slang::typing::context& ctx) const;
    void finish_type_check(slang::typing::context& ctx) const;
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

    std::unique_ptr<slang::codegen::value> generate_code(slang::codegen::context& ctx, memory_context mc = memory_context::none) const override;
    void collect_names(cg::context& ctx, ty::context& type_ctx) const override;
    std::optional<std::string> type_check(slang::typing::context& ctx) const override;
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

    std::unique_ptr<slang::codegen::value> generate_code(slang::codegen::context& ctx, memory_context mc = memory_context::none) const override;
    void collect_names(cg::context& ctx, ty::context& type_ctx) const override;
    bool supports_directive(const std::string& s) const override;
    std::optional<std::string> type_check(slang::typing::context& ctx) const override;
    std::string to_string() const override;
};

/** Function calls. */
class call_expression : public expression
{
    /** The callee's name. */
    token callee;

    /** The function's arguments. */
    std::vector<std::unique_ptr<expression>> args;

public:
    /** No default constructor. */
    call_expression() = delete;

    /** Default destructor. */
    virtual ~call_expression() = default;

    /** Default copy and move constructors. */
    call_expression(const call_expression&) = default;
    call_expression(call_expression&&) = default;

    /** Default assignment operators. */
    call_expression& operator=(const call_expression&) = default;
    call_expression& operator=(call_expression&&) = default;

    /**
     * Construct a function call.
     *
     * @param callee The callee's name.
     * @param args The argument expressions.
     */
    call_expression(token callee, std::vector<std::unique_ptr<expression>> args)
    : expression{callee.location}
    , callee{std::move(callee)}
    , args{std::move(args)}
    {
    }

    std::unique_ptr<slang::codegen::value> generate_code(slang::codegen::context& ctx, memory_context mc = memory_context::none) const override;
    std::optional<std::string> type_check(slang::typing::context& ctx) const override;
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

    std::unique_ptr<slang::codegen::value> generate_code(slang::codegen::context& ctx, memory_context mc = memory_context::none) const override;
    std::optional<std::string> type_check(slang::typing::context& ctx) const override;
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

    std::unique_ptr<slang::codegen::value> generate_code(slang::codegen::context& ctx, memory_context mc = memory_context::none) const override;
    std::optional<std::string> type_check(slang::typing::context& ctx) const override;
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

    std::unique_ptr<slang::codegen::value> generate_code(slang::codegen::context& ctx, memory_context mc = memory_context::none) const override;
    std::optional<std::string> type_check(slang::typing::context& ctx) const override;
    std::string to_string() const override;
};

/** Break statement. */
class break_statement : public expression
{
public:
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
    break_statement(token_location loc)
    : expression{std::move(loc)}
    {
    }

    std::unique_ptr<slang::codegen::value> generate_code(slang::codegen::context& ctx, memory_context mc = memory_context::none) const override;

    std::optional<std::string> type_check(slang::typing::context& ctx) const override
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
    continue_statement(token_location loc)
    : expression{std::move(loc)}
    {
    }

    std::unique_ptr<slang::codegen::value> generate_code(slang::codegen::context& ctx, memory_context mc = memory_context::none) const override;

    std::optional<std::string> type_check(slang::typing::context& ctx) const override
    {
        return std::nullopt;
    }

    std::string to_string() const override
    {
        return "Continue()";
    }
};

}    // namespace slang::ast