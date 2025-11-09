/**
 * slang - a simple scripting language.
 *
 * runtime utilities.
 *
 * \author Felix Lubbe
 * \copyright Copyright (c) 2025
 * \license Distributed under the MIT software license (see accompanying LICENSE.txt).
 */

#include "interpreter/interpreter.h"
#include "runtime/runtime.h"

namespace slang::runtime
{

/*
 * Garbage collected temporary.
 */

gc_object_base::~gc_object_base()
{
    if(obj != nullptr)
    {
        ctx.get_gc().remove_temporary(obj);
        obj = nullptr;
    }
}

gc_object_base gc_pop(si::context& ctx, si::operand_stack& stack)
{
    return {ctx, stack.pop_addr<void>()};
}

/*
 * get_args specializations.
 */

template<>
std::tuple<gc_object<void>> get_args(si::context& ctx, si::operand_stack& stack)
{
    return gc_pop(ctx, stack);
}

template<>
std::tuple<gc_object<std::string>> get_args(si::context& ctx, si::operand_stack& stack)
{
    return gc_pop(ctx, stack);
}

template<>
std::tuple<std::int32_t> get_args([[maybe_unused]] si::context& ctx, si::operand_stack& stack)
{
    return stack.pop_cat1<std::int32_t>();
}

template<>
std::tuple<float> get_args([[maybe_unused]] si::context& ctx, si::operand_stack& stack)
{
    return stack.pop_cat1<float>();
}

}    // namespace slang::runtime
