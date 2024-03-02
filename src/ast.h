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

/*
 * Forward declarations for code generation.
 */
namespace slang::codegen
{
class context;
class function;
class value;
}    // namespace slang::codegen

namespace slang::ast
{

/**
 * The memory context for an expression.
 */
enum class memory_context
{
    none,  /** Neither load nor store. */
    load,  /** loading context. */
    store, /** storing context. */
};

/** Base class for all expression nodes. */
class expression
{
public:
    /** Default destructor. */
    virtual ~expression() = default;

    /**
     * Generate IR.
     *
     * @param ctx The context to use for code generation.
     * @returns The value of this expression or nullptr.
     */
    virtual std::unique_ptr<slang::codegen::value> generate_code(slang::codegen::context* ctx, memory_context mc = memory_context::none) const = 0;
};

/** Literal type. */
enum class literal_type
{
    int_literal, /** integer literal */
    fp_literal,  /** floating-point literal */
    str_literal  /** string literal */
};

/** String, integer or floating-point literals. */
class literal_expression : public expression
{
    /** The literal type. */
    literal_type type;

    /** Evaluated token, for token_type::int_literal, token_type::fp_literal and token_type::string_literal. */
    std::optional<std::variant<int, float, std::string>> value;

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
     * Construct an integer literal.
     *
     * @param i An integer.
     */
    literal_expression(int i)
    : type{literal_type::int_literal}
    , value{i}
    {
    }

    /**
     * Construct a floating-point literal.
     *
     * @param f A floating-point number.
     */
    literal_expression(float f)
    : type{literal_type::fp_literal}
    , value{f}
    {
    }

    /**
     * Construct an string literal.
     *
     * @param s A string.
     */
    literal_expression(std::string s)
    : type{literal_type::str_literal}
    , value{std::move(s)}
    {
    }

    std::unique_ptr<slang::codegen::value> generate_code(slang::codegen::context* ctx, memory_context mc = memory_context::none) const override;
};

/** A signed expression. */
class signed_expression : public expression
{
    /** The sign. */
    std::string sign;

    /** The expression. */
    std::unique_ptr<expression> expr;

public:
    /** No default constructor. */
    signed_expression() = delete;

    /** Default destructor. */
    virtual ~signed_expression() = default;

    /** Copy and move constructors. */
    signed_expression(const signed_expression&) = delete;
    signed_expression(signed_expression&&) = default;

    /** Assignment operators. */
    signed_expression& operator=(const signed_expression&) = delete;
    signed_expression& operator=(signed_expression&&) = default;

    /**
     * Construct a signed expression.
     *
     * @param sign The sign.
     * @param expr The expression.
     */
    signed_expression(std::string sign, std::unique_ptr<expression> expr)
    : sign{std::move(sign)}
    , expr{std::move(expr)}
    {
    }

    std::unique_ptr<slang::codegen::value> generate_code(slang::codegen::context* ctx, memory_context mc = memory_context::none) const override;
};

/** Scope expression. */
class scope_expression : public expression
{
    /** The scope name. */
    std::string name;

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
     * @param identifier The identifier.
     * @param expr An expression.
     */
    scope_expression(std::string name, std::unique_ptr<expression> expr)
    : name{std::move(name)}
    , expr{std::move(expr)}
    {
    }

    std::unique_ptr<slang::codegen::value> generate_code(slang::codegen::context* ctx, memory_context mc = memory_context::none) const override;
};

/** Access expression. */
class access_expression : public expression
{
    /** The namespace name. */
    std::string name;

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
    access_expression(std::string name, std::unique_ptr<expression> expr)
    : name{std::move(name)}
    , expr{std::move(expr)}
    {
    }

    std::unique_ptr<slang::codegen::value> generate_code(slang::codegen::context* ctx, memory_context mc = memory_context::none) const override;
};

/** Import statements. */
class import_expression : public expression
{
    /** The import path. */
    std::vector<std::string> path;

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
    import_expression(std::vector<std::string> in_path)
    : path{std::move(in_path)}
    {
    }

    std::unique_ptr<slang::codegen::value> generate_code(slang::codegen::context* ctx, memory_context mc = memory_context::none) const override;
};

/** Variable references. */
class variable_reference_expression : public expression
{
    /** The variable name. */
    std::string name;

public:
    /** No default constructor. */
    variable_reference_expression() = default;

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
    variable_reference_expression(std::string name)
    : name{std::move(name)}
    {
    }

    std::unique_ptr<slang::codegen::value> generate_code(slang::codegen::context* ctx, memory_context mc = memory_context::none) const override;
};

/** Variable declaration. */
class variable_declaration_expression : public expression
{
    /** The variable name. */
    std::string name;

    /** The variable's type. */
    std::string type;

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
     * @param name The variable's name.
     * @param type The variable's type.
     * @param expr An optional initializer expression.
     */
    variable_declaration_expression(std::string name, std::string type, std::unique_ptr<ast::expression> expr)
    : name{std::move(name)}
    , type{std::move(type)}
    , expr{std::move(expr)}
    {
    }

    std::unique_ptr<slang::codegen::value> generate_code(slang::codegen::context* ctx, memory_context mc = memory_context::none) const override;
};

/** Struct definition. */
class struct_definition_expression : public expression
{
    /** The struct's name. */
    std::string name;

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
     * @param name The struct's name.
     * @param members The struct's members.
     */
    struct_definition_expression(std::string name, std::vector<std::unique_ptr<variable_declaration_expression>> members)
    : name{std::move(name)}
    , members{std::move(members)}
    {
    }

    std::unique_ptr<slang::codegen::value> generate_code(slang::codegen::context* ctx, memory_context mc = memory_context::none) const override;
};

/** Anonymous struct initialization. */
class struct_anonymous_initializer_expression : public expression
{
    /** The struct's name. */
    std::string name;

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
     * @param name The struct's name.
     * @param members The struct's members.
     */
    struct_anonymous_initializer_expression(std::string name, std::vector<std::unique_ptr<expression>> initializers)
    : name{std::move(name)}
    , initializers{std::move(initializers)}
    {
    }

    std::unique_ptr<slang::codegen::value> generate_code(slang::codegen::context* ctx, memory_context mc = memory_context::none) const override;
};

/** Named struct initialization. */
class struct_named_initializer_expression : public expression
{
    /** The struct's name. */
    std::string name;

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
    struct_named_initializer_expression(std::string name, std::vector<std::unique_ptr<expression>> member_names, std::vector<std::unique_ptr<expression>> initializers)
    : name{std::move(name)}
    , member_names{std::move(member_names)}
    , initializers{std::move(initializers)}
    {
    }

    std::unique_ptr<slang::codegen::value> generate_code(slang::codegen::context* ctx, memory_context mc = memory_context::none) const override;
};

/** Binary operators. */
class binary_expression : public expression
{
    /** The binary operator. */
    std::string op;

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
     * @param op The binary operator.
     * @param lhs The left-hand side.
     * @param rhs The right-hand side.
     */
    binary_expression(std::string op, std::unique_ptr<expression> lhs, std::unique_ptr<expression> rhs)
    : op{op}
    , lhs{std::move(lhs)}
    , rhs{std::move(rhs)}
    {
    }

    std::unique_ptr<slang::codegen::value> generate_code(slang::codegen::context* ctx, memory_context mc = memory_context::none) const override;
};

/** Function prototype. */
class prototype_ast
{
    /** The function name. */
    std::string name;

    /** The function's arguments as (name, type). */
    std::vector<std::pair<std::string, std::string>> args;

    /** The function's return type. */
    std::string return_type;

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
     * @param name The function's name.
     * @param args The function's arguments as a vector of pairs (type_identifier, name).
     * @param return_type The function's return type.
     */
    prototype_ast(std::string name, std::vector<std::pair<std::string, std::string>> args, std::string return_type)
    : name{std::move(name)}
    , args{std::move(args)}
    , return_type{std::move(return_type)}
    {
    }

    slang::codegen::function* generate_code(slang::codegen::context* ctx, memory_context mc = memory_context::none) const;
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
     * @param exprs The program expressions.
     */
    block(std::vector<std::unique_ptr<expression>> exprs)
    : exprs{std::move(exprs)}
    {
    }

    std::unique_ptr<slang::codegen::value> generate_code(slang::codegen::context* ctx, memory_context mc = memory_context::none) const override;
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
     * @param prototype The function's prototype.
     * @param body The function's body.
     */
    function_expression(std::unique_ptr<prototype_ast> prototype, std::unique_ptr<block> body)
    : prototype{std::move(prototype)}
    , body{std::move(body)}
    {
    }

    std::unique_ptr<slang::codegen::value> generate_code(slang::codegen::context* ctx, memory_context mc = memory_context::none) const override;
};

/** Function calls. */
class call_expression : public expression
{
    /** The callee's name. */
    std::string callee;

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
    call_expression(const std::string& callee, std::vector<std::unique_ptr<expression>> args)
    : callee{callee}
    , args{std::move(args)}
    {
    }

    std::unique_ptr<slang::codegen::value> generate_code(slang::codegen::context* ctx, memory_context mc = memory_context::none) const override;
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
     * @param expr The returned expression (if any).
     */
    return_statement(std::unique_ptr<ast::expression> expr)
    : expr{std::move(expr)}
    {
    }

    std::unique_ptr<slang::codegen::value> generate_code(slang::codegen::context* ctx, memory_context mc = memory_context::none) const override;
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
     * @param condition The condition.
     * @param if_block The block to be executed if the condition was true.
     * @param else_block The block to be executed if the condition was false.
     */
    if_statement(std::unique_ptr<ast::expression> condition, std::unique_ptr<ast::expression> if_block, std::unique_ptr<ast::expression> else_block)
    : condition{std::move(condition)}
    , if_block{std::move(if_block)}
    , else_block{std::move(else_block)}
    {
    }

    std::unique_ptr<slang::codegen::value> generate_code(slang::codegen::context* ctx, memory_context mc = memory_context::none) const override;
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
     * @param condition The condition.
     * @param while_block The block to be executed while the condition is true.
     */
    while_statement(std::unique_ptr<ast::expression> condition, std::unique_ptr<ast::expression> while_block)
    : condition{std::move(condition)}
    , while_block{std::move(while_block)}
    {
    }

    std::unique_ptr<slang::codegen::value> generate_code(slang::codegen::context* ctx, memory_context mc = memory_context::none) const override;
};

/** Break statement. */
class break_statement : public expression
{
public:
    /** Default constructor. */
    break_statement() = default;

    /** Default destructor. */
    virtual ~break_statement() = default;

    /** Default copy and move constructors. */
    break_statement(const break_statement&) = default;
    break_statement(break_statement&&) = default;

    /** Default assignment operators. */
    break_statement& operator=(const break_statement&) = default;
    break_statement& operator=(break_statement&&) = default;

    std::unique_ptr<slang::codegen::value> generate_code(slang::codegen::context* ctx, memory_context mc = memory_context::none) const override;
};

/** Continue statement. */
class continue_statement : public expression
{
public:
    /** Default constructor. */
    continue_statement() = default;

    /** Default destructor. */
    virtual ~continue_statement() = default;

    /** Default copy and move constructors. */
    continue_statement(const continue_statement&) = default;
    continue_statement(continue_statement&&) = default;

    /** Default assignment operators. */
    continue_statement& operator=(const continue_statement&) = default;
    continue_statement& operator=(continue_statement&&) = default;

    std::unique_ptr<slang::codegen::value> generate_code(slang::codegen::context* ctx, memory_context mc = memory_context::none) const override;
};

}    // namespace slang::ast