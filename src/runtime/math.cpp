/**
 * slang - a simple scripting language.
 *
 * runtime math support.
 *
 * \author Felix Lubbe
 * \copyright Copyright (c) 2024
 * \license Distributed under the MIT software license (see accompanying LICENSE.txt).
 */

#include <cmath>

#include "interpreter/interpreter.h"
#include "runtime/runtime.h"

namespace slang::runtime
{

// Work around missing inclusions of some math functions into std on GCC.
// See https://gcc.gnu.org/bugzilla/show_bug.cgi?id=79700.
#if !defined(__clang__) && defined(__GNUC__) && (__GNUC__ < 14)
const auto builtin_fabsf = ::fabsf;
const auto builtin_sqrtf = ::sqrtf;
const auto builtin_ceilf = ::ceilf;
const auto builtin_floorf = ::floorf;
#else
const auto builtin_fabsf = std::fabsf;
const auto builtin_sqrtf = std::sqrtf;
const auto builtin_ceilf = std::ceilf;
const auto builtin_floorf = std::floorf;
#endif

void abs(si::context&, si::operand_stack& stack)
{
    stack.push_f32(builtin_fabsf(stack.pop_f32()));
}

void sqrt(si::context&, si::operand_stack& stack)
{
    stack.push_f32(builtin_sqrtf(stack.pop_f32()));
}

void ceil(si::context&, si::operand_stack& stack)
{
    stack.push_f32(builtin_ceilf(stack.pop_f32()));
}

void floor(si::context&, si::operand_stack& stack)
{
    stack.push_f32(builtin_floorf(stack.pop_f32()));
}

void trunc(si::context&, si::operand_stack& stack)
{
    stack.push_f32(std::truncf(stack.pop_f32()));
}

void round(si::context&, si::operand_stack& stack)
{
    stack.push_f32(std::roundf(stack.pop_f32()));
}

void sin(si::context&, si::operand_stack& stack)
{
    stack.push_f32(::sinf(stack.pop_f32()));
}

void cos(si::context&, si::operand_stack& stack)
{
    stack.push_f32(::cosf(stack.pop_f32()));
}

void tan(si::context&, si::operand_stack& stack)
{
    stack.push_f32(::tanf(stack.pop_f32()));
}

void asin(si::context&, si::operand_stack& stack)
{
    stack.push_f32(::asinf(stack.pop_f32()));
}

void acos(si::context&, si::operand_stack& stack)
{
    stack.push_f32(::acosf(stack.pop_f32()));
}

void atan(si::context&, si::operand_stack& stack)
{
    stack.push_f32(::atanf(stack.pop_f32()));
}

void atan2(si::context&, si::operand_stack& stack)
{
    float y = stack.pop_f32();
    float x = stack.pop_f32();
    stack.push_f32(::atan2f(x, y));
}

}    // namespace slang::runtime
