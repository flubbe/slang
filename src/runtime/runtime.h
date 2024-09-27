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

namespace slang::runtime
{

namespace si = slang::interpreter;

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

}    // namespace slang::runtime
