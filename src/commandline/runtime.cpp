/**
 * slang - a simple scripting language.
 *
 * Runtime setup for commandline tools.
 *
 * \author Felix Lubbe
 * \copyright Copyright (c) 2025
 * \license Distributed under the MIT software license (see accompanying LICENSE.txt).
 */

#include <print>

#include "archives/file.h"
#include "interpreter/interpreter.h"
#include "interpreter/invoke.h"
#include "runtime/runtime.h"
#include "shared/module.h"
#include "commandline.h"
#include "utils.h"

namespace rt = slang::runtime;
namespace si = slang::interpreter;

namespace slang::commandline
{

void runtime_setup(si::context& ctx, bool verbose)
{
    if(verbose)
    {
        std::println("Info: Registering type layouts.");
    }

    rt::register_builtin_type_layouts(ctx.get_gc());

    if(verbose)
    {
        std::println("Info: Registering native functions.");
    }

    ctx.register_native_function("slang", "print",
                                 [&ctx](si::operand_stack& stack)
                                 {
                                     auto* s = stack.pop_addr<std::string>();
                                     std::print("{}", *s);
                                     ctx.get_gc().remove_temporary(s);
                                 });
    ctx.register_native_function("slang", "println",
                                 [&ctx](si::operand_stack& stack)
                                 {
                                     auto* s = stack.pop_addr<std::string>();
                                     std::println("{}", *s);
                                     ctx.get_gc().remove_temporary(s);
                                 });
    ctx.register_native_function("slang", "array_copy",
                                 [&ctx](si::operand_stack& stack)
                                 {
                                     rt::array_copy(ctx, stack);
                                 });
    ctx.register_native_function("slang", "string_length",
                                 [&ctx](si::operand_stack& stack)
                                 {
                                     rt::string_length(ctx, stack);
                                 });
    ctx.register_native_function("slang", "string_equals",
                                 [&ctx](si::operand_stack& stack)
                                 {
                                     rt::string_equals(ctx, stack);
                                 });
    ctx.register_native_function("slang", "string_concat",
                                 [&ctx](si::operand_stack& stack)
                                 {
                                     rt::string_concat(ctx, stack);
                                 });
    ctx.register_native_function("slang", "i32_to_string",
                                 [&ctx](si::operand_stack& stack)
                                 {
                                     rt::i32_to_string(ctx, stack);
                                 });
    ctx.register_native_function("slang", "f32_to_string",
                                 [&ctx](si::operand_stack& stack)
                                 {
                                     rt::f32_to_string(ctx, stack);
                                 });
    ctx.register_native_function("slang", "parse_i32",
                                 [&ctx](si::operand_stack& stack)
                                 {
                                     rt::parse_i32(ctx, stack);
                                 });
    ctx.register_native_function("slang", "parse_f32",
                                 [&ctx](si::operand_stack& stack)
                                 {
                                     rt::parse_f32(ctx, stack);
                                 });
    ctx.register_native_function("slang", "assert",
                                 [&ctx](si::operand_stack& stack)
                                 {
                                     rt::assert_(ctx, stack);
                                 });

    /*
     * Math.
     */

    ctx.register_native_function("slang", "abs",
                                 [&ctx](si::operand_stack& stack)
                                 {
                                     rt::abs(ctx, stack);
                                 });
    ctx.register_native_function("slang", "sqrt",
                                 [&ctx](si::operand_stack& stack)
                                 {
                                     rt::sqrt(ctx, stack);
                                 });
    ctx.register_native_function("slang", "ceil",
                                 [&ctx](si::operand_stack& stack)
                                 {
                                     rt::ceil(ctx, stack);
                                 });
    ctx.register_native_function("slang", "floor",
                                 [&ctx](si::operand_stack& stack)
                                 {
                                     rt::floor(ctx, stack);
                                 });
    ctx.register_native_function("slang", "trunc",
                                 [&ctx](si::operand_stack& stack)
                                 {
                                     rt::trunc(ctx, stack);
                                 });
    ctx.register_native_function("slang", "round",
                                 [&ctx](si::operand_stack& stack)
                                 {
                                     rt::round(ctx, stack);
                                 });
    ctx.register_native_function("slang", "sin",
                                 [&ctx](si::operand_stack& stack)
                                 {
                                     rt::sin(ctx, stack);
                                 });
    ctx.register_native_function("slang", "cos",
                                 [&ctx](si::operand_stack& stack)
                                 {
                                     rt::cos(ctx, stack);
                                 });
    ctx.register_native_function("slang", "tan",
                                 [&ctx](si::operand_stack& stack)
                                 {
                                     rt::tan(ctx, stack);
                                 });
    ctx.register_native_function("slang", "asin",
                                 [&ctx](si::operand_stack& stack)
                                 {
                                     rt::asin(ctx, stack);
                                 });
    ctx.register_native_function("slang", "acos",
                                 [&ctx](si::operand_stack& stack)
                                 {
                                     rt::acos(ctx, stack);
                                 });
    ctx.register_native_function("slang", "atan",
                                 [&ctx](si::operand_stack& stack)
                                 {
                                     rt::atan(ctx, stack);
                                 });
    ctx.register_native_function("slang", "atan2",
                                 [&ctx](si::operand_stack& stack)
                                 {
                                     rt::atan2(ctx, stack);
                                 });
}

}    // namespace slang::commandline
