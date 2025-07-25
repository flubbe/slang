/**
 * slang - a simple scripting language.
 *
 * runtime array support.
 *
 * \author Felix Lubbe
 * \copyright Copyright (c) 2025
 * \license Distributed under the MIT software license (see accompanying LICENSE.txt).
 */

#include "interpreter/interpreter.h"
#include "runtime/runtime.h"

namespace slang::runtime
{

void array_copy(si::context& ctx, si::operand_stack& stack)
{
    auto [from_container, to_container] = get_args<gc_object<void>, gc_object<void>>(ctx, stack);

    void* to = to_container.get();
    void* from = from_container.get();

    if(to == nullptr)
    {
        throw si::interpreter_error("array_copy: 'to' is null.");
    }
    if(from == nullptr)
    {
        throw si::interpreter_error("array_copy: 'from' is null.");
    }

    auto& gc = ctx.get_gc();

    auto to_type = gc.get_object_type(to);
    auto from_type = gc.get_object_type(from);

    if(to_type != from_type)
    {
        throw si::interpreter_error("array_copy: type mismatch.");
    }

    // copy array with bounds check.
    if(to_type == gc::gc_object_type::array_i32 || to_type == gc::gc_object_type::array_f32)
    {
        auto* from_array = reinterpret_cast<si::fixed_vector<std::int32_t>*>(from);    // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast)
        auto* to_array = reinterpret_cast<si::fixed_vector<std::int32_t>*>(to);        // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast)

        if(to_array->size() < from_array->size())
        {
            throw si::interpreter_error("array_copy: destination array is too small.");
        }

        for(std::size_t i = 0; i < from_array->size(); ++i)
        {
            (*to_array)[i] = (*from_array)[i];
        }
    }
    else if(to_type == gc::gc_object_type::array_str
            || to_type == gc::gc_object_type::array_aref)
    {
        auto* from_array = reinterpret_cast<si::fixed_vector<void*>*>(from);    // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast)
        auto* to_array = reinterpret_cast<si::fixed_vector<void*>*>(to);        // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast)

        if(to_array->size() < from_array->size())
        {
            throw si::interpreter_error("array_copy: destination array is too small.");
        }

        for(std::size_t i = 0; i < from_array->size(); ++i)
        {
            (*to_array)[i] = (*from_array)[i];
        }
    }
    else
    {
        throw si::interpreter_error("array_copy: unsupported type.");
    }
}

}    // namespace slang::runtime
