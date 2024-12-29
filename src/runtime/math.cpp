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

void abs(si::context&, si::operand_stack& stack)
{
    stack.push_f32(std::fabsf(stack.pop_f32()));
}

void sqrt(si::context&, si::operand_stack& stack)
{
    stack.push_f32(std::sqrtf(stack.pop_f32()));
}

void ceil(si::context&, si::operand_stack& stack)
{
    stack.push_f32(std::ceilf(stack.pop_f32()));
}

void floor(si::context&, si::operand_stack& stack)
{
    stack.push_f32(std::floorf(stack.pop_f32()));
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
