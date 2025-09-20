/**
 * slang - a simple scripting language.
 *
 * constant expression info, used e.g. during constant evaluation.
 *
 * \author Felix Lubbe
 * \copyright Copyright (c) 2025
 * \license Distributed under the MIT software license (see accompanying LICENSE.txt).
 */

#pragma once

#include <string>
#include <unordered_map>
#include <variant>

#include "compiler/sema.h"

/*
 * Forward declarations.
 */
namespace slang::ast
{
class expression; /* ast.h */
}    // namespace slang::ast

namespace slang::const_
{

/** Constant type. */
enum class constant_type
{
    i32, /* An i32 constant. */
    f32, /* An f32 constant. */
    str  /* A string constant*/
};

/**
 * Convert a `constant_type` into a readable string.
 *
 * @param c The constant type.
 * @returns Returns a string representation of the constant type.
 */
std::string to_string(constant_type c);

/** Information about a constant. */
struct const_info
{
    /** Result type. */
    constant_type type;

    /** Value. */
    std::variant<
      std::monostate,
      int,
      float,
      std::string>
      value;

    /** Comparison. */
    bool operator==(const const_info& other) const
    {
        return type == other.type
               && value == other.value;
    }
    bool operator!=(const const_info& other) const
    {
        return !(*this == other);
    }
};

/** Constant id (for interned constants). */
using constant_id = std::uint64_t;

/** Constant evaluation environment. */
struct env
{
    /** Const info for symbols. */
    std::unordered_map<
      sema::symbol_id,
      const_info,
      sema::symbol_id::hash>
      const_info_map;

    /** Constant literals. */
    std::unordered_map<
      constant_id,
      const_info>
      const_literal_map;

    /** Const-eval expressions. */
    std::unordered_map<
      const ast::expression*,
      bool>
      const_eval_exprs;

    /** Const-eval expressions values. */
    std::unordered_map<
      const ast::expression*,
      const_info>
      const_eval_expr_values;

    /** Default constructors. */
    env() = default;
    env(const env&) = default;
    env(env&&) = default;

    /** Default destructor. */
    ~env() = default;

    /** Default assignments. */
    env& operator=(const env&) = default;
    env& operator=(env&&) = default;

    /**
     * Set the result of a constant evaluation for a symbol.
     *
     * @note Overwrites a possibly existing value for the symbol.
     *
     * @param id Symbol id.
     * @param info The constant info to set.
     */
    void set_const_info(
      sema::symbol_id id,
      const_info info);

    /**
     * Get the result of a constant evaluation for a symbol.
     *
     * @param id Symbol id.
     * @returns Returns the `const_info` for this symbol, or `std::nullopt`.
     */
    std::optional<const_info> get_const_info(
      sema::symbol_id id) const;

    /**
     * Intern a string constant.
     *
     * @param s The string.
     * @returns Returns an index for the string.
     */
    constant_id intern(std::string s);

    /**
     * Set whether an expression was found to be const-eval.
     *
     * @param expr The ast node / expression that was evaluated.
     * @param is_const_eval Whether the expression was found to be const-eval.
     */
    void set_expression_const_eval(
      const ast::expression& expr,
      bool is_const_eval);

    /**
     * Check if an expression is known to be (not) const-eval.
     *
     * @param expr The ast node / expression to check.
     * @returns Returns `true` if the expression is const-eval, and `false` if it is not.
     *      Returns `std::nullopt` if the expression was not checked.
     */
    std::optional<bool> is_expression_const_eval(
      const ast::expression& expr)
    {
        return const_eval_exprs.contains(&expr);
    }

    /**
     * Check if an expression was already evaluated.
     *
     * @param expr The ast node / expression to check.
     * @returns Returns `true` if the expression was evaluated.
     */
    bool is_expression_evaluated(
      const ast::expression& expr)
    {
        return const_eval_expr_values.contains(&expr);
    }

    /**
     * Set an expression's value.
     *
     * @param expr The ast node / expression.
     * @param info The constant information containing the expression's value.
     */
    void set_expression_value(
      const ast::expression& expr,
      const_info info);

    /**
     * Get an expressions's value.
     *
     * @note The returned reference is only valid until a re-hash occurs.
     *       Copy if you intend to use it later.
     * @param expr The ast node / expression.
     * @returns Returns the expression's value.
     */
    const const_info& get_expression_value(
      const ast::expression& expr) const;
};

}    // namespace slang::const_