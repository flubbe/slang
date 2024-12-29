/**
 * slang - a simple scripting language.
 *
 * runtime.
 *
 * \author Felix Lubbe
 * \copyright Copyright (c) 2024
 * \license Distributed under the MIT software license (see accompanying LICENSE.txt).
 */

#pragma once

#include "utils.h"

/* Forward declarations. */
namespace slang::interpreter
{
class operand_stack;
class context;
}    // namespace slang::interpreter

namespace slang::gc
{
class garbage_collector;
}    // namespace slang::gc

namespace slang::runtime
{

namespace si = slang::interpreter;
namespace gc = slang::gc;

/*
 * Built-in types.
 */

/** Wrapper around i32. */
struct i32s
{
    /** Value held by the wrapper. */
    std::int32_t value{0};
};

/** Wrapper around f32. */
struct f32s
{
    /** Value held by the wrapper. */
    float value{0};
};

/** Result type. */
struct result
{
    /** Indicates whether the evaluation succeeded. Defaults to `0`, indicating a failure. */
    std::uint32_t ok{0};

    /** Holds the evaluation's result on success, and an object describing the error on failure. */
    void* value{nullptr};
};

/**
 * Register layouts for built-in types.
 *
 * @param gc The garbage collector to register the types with.
 */
void register_builtin_type_layouts(gc::garbage_collector& gc);

/*
 * Arrays.
 */

/**
 * Copy an array into another array.
 *
 * @param ctx The interpreter context.
 * @param stack The operand stack.
 */
void array_copy(si::context& ctx, si::operand_stack& stack);

/*
 * Strings.
 */

/**
 * Check strings for equality. Pushes `1` onto the stack if the strings are equal, and `0` otherwise.
 *
 * @param ctx The interpreter context.
 * @param stack The operand stack.
 */
void string_equals(si::context& ctx, si::operand_stack& stack);

/**
 * Concatenate two strings.
 *
 * @param ctx The interpreter context.
 * @param stack The operand stack.
 */
void string_concat(si::context& ctx, si::operand_stack& stack);

/**
 * Convert an i32 integer to a string.
 *
 * @param ctx The interpreter context.
 * @param stack The operand stack.
 */
void i32_to_string(si::context& ctx, si::operand_stack& stack);

/**
 * Convert an f32 float to a string.
 *
 * @param ctx The interpreter context.
 * @param stack The operand stack.
 */
void f32_to_string(si::context& ctx, si::operand_stack& stack);

/**
 * Parse a string to obtain an i32 integer.
 *
 * @param ctx The interpreter context.
 * @param stack The operand stack.
 */
void parse_i32(si::context& ctx, si::operand_stack& stack);

/**
 * Parse a string to obtain a f32 float.
 *
 * @param ctx The interpreter context.
 * @param stack The operand stack.
 */
void parse_f32(si::context& ctx, si::operand_stack& stack);

/*
 * Debug.
 */

/**
 * Assert that an expression does not evaluate to 0.
 *
 * @param ctx The interpreter context.
 * @param stack The operand stack.
 */
void assert_(si::context& ctx, si::operand_stack& stack);

/*
 * Math functions.
 */

/**
 * Compute the absolute value.
 *
 * @param x A floating point value.
 * @return Returns `abs(x)`.
 */
void abs(si::context& ctx, si::operand_stack& stack);

/**
 * Compute the square root.
 *
 * @param x A floating point value.
 * @return Returns `sqrt(x)`.
 */
void sqrt(si::context& ctx, si::operand_stack& stack);

/**
 * Computes the least integer value not less than `x`.
 *
 * @param x A floating point value.
 * @return Returns `ceil(x)`.
 */
void ceil(si::context& ctx, si::operand_stack& stack);

/**
 * Computes the largest integer value not greater than `x`.
 *
 * @param x A floating point value.
 * @return Returns `floor(x)`.
 */
void floor(si::context& ctx, si::operand_stack& stack);

/**
 * Computes the nearest integer not greater in magnitude than `x`.
 *
 * @param x A floating point value.
 * @return Returns `trunc(x)`.
 */
void trunc(si::context& ctx, si::operand_stack& stack);

/**
 * Computes the nearest integer value to `x` (in floating-point format),
 * rounding halfway cases away from zero, regardless of the current rounding mode
 *
 * @param x A floating point value.
 * @return Returns `round(x)`.
 */
void round(si::context& ctx, si::operand_stack& stack);

/**
 * Compute the sine.
 *
 * @param x Argument to the sine.
 * @return Returns `sin(x)`.
 */
void sin(si::context& ctx, si::operand_stack& stack);

/**
 * Compute the cosine.
 *
 * @param x Argument to the cosine.
 * @return Returns `cos(x)`.
 */
void cos(si::context& ctx, si::operand_stack& stack);

/**
 * Compute the tangent.
 *
 * @param x Argument to the tangent.
 * @return Returns `tan(x)`.
 */
void tan(si::context& ctx, si::operand_stack& stack);

/**
 * Compute the arc sine.
 *
 * @param x Argument to the arc sine.
 * @return Returns `asin(x)`.
 */
void asin(si::context& ctx, si::operand_stack& stack);

/**
 * Compute the arc cosine.
 *
 * @param x Argument to the arc cosine.
 * @return Returns `acos(x)`.
 */
void acos(si::context& ctx, si::operand_stack& stack);

/**
 * Compute the principal value of the arc tangent.
 *
 * @param x Argument to the arc tangent.
 * @return Returns `atan(x)`.
 */
void atan(si::context& ctx, si::operand_stack& stack);

/**
 * Computes the arc tangent of `y/x` using the signs of arguments to determine the correct quadrant.
 *
 * @param x `f32` value.
 * @param y `f32` value.
 * @return Returns `atan(x)`.
 */
void atan2(si::context& ctx, si::operand_stack& stack);

}    // namespace slang::runtime
