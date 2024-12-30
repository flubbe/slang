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
const auto builtin_sinf = ::sinf;
const auto builtin_cosf = ::cosf;
const auto builtin_tanf = ::tanf;
const auto builtin_asinf = ::asinf;
const auto builtin_acosf = ::acosf;
const auto builtin_atanf = ::atanf;
const auto builtin_atan2f = ::atan2f;
#else
const auto builtin_fabsf = std::fabsf;
const auto builtin_sqrtf = std::sqrtf;
const auto builtin_ceilf = std::ceilf;
const auto builtin_floorf = std::floorf;
const auto builtin_sinf = std::sinf;
const auto builtin_cosf = std::cosf;
const auto builtin_tanf = std::tanf;
const auto builtin_asinf = std::asinf;
const auto builtin_acosf = std::acosf;
const auto builtin_atanf = std::atanf;
const auto builtin_atan2f = std::atan2f;
#endif

void abs(si::context&, si::operand_stack& stack)
{
    stack.modify_top<float, float>(
      [](float value) -> float
      { return builtin_fabsf(value); });
}

void sqrt(si::context&, si::operand_stack& stack)
{
    stack.modify_top<float, float>(
      [](float value) -> float
      { return builtin_sqrtf(value); });
}

void ceil(si::context&, si::operand_stack& stack)
{
    stack.modify_top<float, float>(
      [](float value) -> float
      { return builtin_ceilf(value); });
}

void floor(si::context&, si::operand_stack& stack)
{
    stack.modify_top<float, float>(
      [](float value) -> float
      { return builtin_floorf(value); });
}

void trunc(si::context&, si::operand_stack& stack)
{
    stack.modify_top<float, float>(
      [](float value) -> float
      { return std::truncf(value); });
}

void round(si::context&, si::operand_stack& stack)
{
    stack.modify_top<float, float>(
      [](float value) -> float
      { return std::roundf(value); });
}

void sin(si::context&, si::operand_stack& stack)
{
    stack.modify_top<float, float>(
      [](float value) -> float
      { return builtin_sinf(value); });
}

void cos(si::context&, si::operand_stack& stack)
{
    stack.modify_top<float, float>(
      [](float value) -> float
      { return builtin_cosf(value); });
}

void tan(si::context&, si::operand_stack& stack)
{
    stack.modify_top<float, float>(
      [](float value) -> float
      { return builtin_tanf(value); });
}

void asin(si::context&, si::operand_stack& stack)
{
    stack.modify_top<float, float>(
      [](float value) -> float
      { return builtin_asinf(value); });
}

void acos(si::context&, si::operand_stack& stack)
{
    stack.modify_top<float, float>(
      [](float value) -> float
      { return builtin_acosf(value); });
}

void atan(si::context&, si::operand_stack& stack)
{
    stack.modify_top<float, float>(
      [](float value) -> float
      { return builtin_atanf(value); });
}

void atan2(si::context&, si::operand_stack& stack)
{
    float y = stack.pop_f32();
    float x = stack.pop_f32();
    stack.push_f32(builtin_atan2f(x, y));
}

}    // namespace slang::runtime
