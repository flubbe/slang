/**
 * slang - a simple scripting language.
 *
 * runtime garbage collector interface.
 *
 * \author Felix Lubbe
 * \copyright Copyright (c) 2025
 * \license Distributed under the MIT software license (see accompanying LICENSE.txt).
 */

#include <format>

#include "interpreter/interpreter.h"
#include "runtime/runtime.h"

namespace gc = slang::gc;

namespace slang::runtime::gci
{

void run(si::context& ctx, [[maybe_unused]] si::operand_stack& stack)
{
    ctx.get_gc().run();
}

void object_count(si::context& ctx, si::operand_stack& stack)
{
    stack.push_cat1(
      static_cast<std::int32_t>(
        ctx.get_gc().object_count()));
}

void root_set_size(si::context& ctx, si::operand_stack& stack)
{
    stack.push_cat1(
      static_cast<std::int32_t>(
        ctx.get_gc().root_set_size()));
}

void allocated_bytes(si::context& ctx, si::operand_stack& stack)
{
    stack.push_cat1(
      static_cast<std::int32_t>(
        ctx.get_gc().byte_size()));
}

void allocated_bytes_since_gc(si::context& ctx, si::operand_stack& stack)
{
    stack.push_cat1(
      static_cast<std::int32_t>(
        ctx.get_gc().byte_size_since_gc()));
}

void min_threshold_bytes(si::context& ctx, si::operand_stack& stack)
{
    stack.push_cat1(
      static_cast<std::int32_t>(
        ctx.get_gc().min_threshold_bytes()));
}

void threshold_bytes(si::context& ctx, si::operand_stack& stack)
{
    stack.push_cat1(
      static_cast<std::int32_t>(
        ctx.get_gc().threshold_bytes()));
}

void growth_factor(si::context& ctx, si::operand_stack& stack)
{
    stack.push_cat1(
      ctx.get_gc().growth_factor());
}

}    // namespace slang::runtime::gci
