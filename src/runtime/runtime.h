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

}    // namespace slang::runtime
