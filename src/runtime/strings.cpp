/**
 * slang - a simple scripting language.
 *
 * runtime string support.
 *
 * \author Felix Lubbe
 * \copyright Copyright (c) 2025
 * \license Distributed under the MIT software license (see accompanying LICENSE.txt).
 */

#include <format>

#include "interpreter/interpreter.h"
#include "runtime/runtime.h"

namespace gc = slang::gc;

namespace slang::runtime
{

void string_length(si::context& ctx, si::operand_stack& stack)
{
    auto [container] = get_args<gc_object<std::string>>(ctx, stack);

    std::string* s = container.get();
    if(s == nullptr)
    {
        throw si::interpreter_error("string_length: argument cannot be null.");
    }

    auto type = ctx.get_gc().get_object_type(s);
    if(type != gc::gc_object_type::str)
    {
        throw si::interpreter_error("string_length: argument is not a string.");
    }

    stack.push_cat1(utils::numeric_cast<std::int32_t>(s->length()));
}

void string_equals(si::context& ctx, si::operand_stack& stack)
{
    auto [s1_container, s2_container] = get_args<gc_object<std::string>, gc_object<std::string>>(ctx, stack);

    std::string* s1 = s1_container.get();
    std::string* s2 = s2_container.get();

    if(s1 == nullptr || s2 == nullptr)
    {
        throw si::interpreter_error("string_equals: arguments cannot be null.");
    }

    auto& gc = ctx.get_gc();

    auto s1_type = gc.get_object_type(s1);
    auto s2_type = gc.get_object_type(s2);

    if(s1_type != gc::gc_object_type::str || s2_type != gc::gc_object_type::str)
    {
        throw si::interpreter_error("string_equals: arguments are not strings.");
    }

    stack.push_cat1((*s1 == *s2) ? 1 : 0);
}

void string_concat(si::context& ctx, si::operand_stack& stack)
{
    auto [s1_container, s2_container] = get_args<gc_object<std::string>, gc_object<std::string>>(ctx, stack);

    std::string* s1 = s1_container.get();
    std::string* s2 = s2_container.get();

    if(s1 == nullptr || s2 == nullptr)
    {
        throw si::interpreter_error("string_concat: arguments cannot be null.");
    }

    auto& gc = ctx.get_gc();

    auto s1_type = gc.get_object_type(s1);
    auto s2_type = gc.get_object_type(s2);

    if(s1_type != gc::gc_object_type::str || s2_type != gc::gc_object_type::str)
    {
        throw si::interpreter_error("string_concat: arguments are not strings.");
    }

    auto* str = gc.gc_new<std::string>(gc::gc_object::of_temporary);
    *str = *s1 + *s2;

    stack.push_addr<std::string>(str);
}

void i32_to_string(si::context& ctx, si::operand_stack& stack)
{
    auto [i] = get_args<std::int32_t>(ctx, stack);
    auto* str = ctx.get_gc().gc_new<std::string>(gc::gc_object::of_temporary);
    str->assign(std::format("{}", i));
    stack.push_addr<std::string>(str);
}

void f32_to_string(si::context& ctx, si::operand_stack& stack)
{
    auto [f] = get_args<float>(ctx, stack);
    auto* str = ctx.get_gc().gc_new<std::string>(gc::gc_object::of_temporary);
    str->assign(std::format("{}", f));
    stack.push_addr<std::string>(str);
}

void parse_i32(si::context& ctx, si::operand_stack& stack)
{
    auto [container] = get_args<gc_object<std::string>>(ctx, stack);
    std::string* s = container.get();

    if(s == nullptr)
    {
        throw si::interpreter_error("parse_i32: argument cannot be null.");
    }

    std::size_t result_layout_id = ctx.get_gc().get_type_layout_id(si::make_type_name("std", "result"));
    std::size_t i32s_layout_id = ctx.get_gc().get_type_layout_id(si::make_type_name("std", "i32s"));

    auto* r = reinterpret_cast<result*>(ctx.get_gc().gc_new(    // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast)
      result_layout_id, sizeof(result), std::alignment_of_v<result>,
      gc::gc_object::of_temporary));
    stack.push_addr(r);

    try
    {
        // convert first, so that no memory for `i32s` is allocated in case of failure.
        std::int32_t i = std::stoi(*s);

        r->value = ctx.get_gc().gc_new(
          i32s_layout_id, sizeof(i32s), std::alignment_of_v<i32s>,
          gc::gc_object::of_none, false);
        static_cast<i32s*>(r->value)->value = i;
        r->ok = 1;
    }
    catch(const std::invalid_argument& e)
    {
        r->value = ctx.get_gc().gc_new<std::string>(gc::gc_object::of_none, false);
        *static_cast<std::string*>(r->value) = e.what();
        r->ok = 0;
    }
    catch(const std::out_of_range& e)
    {
        r->value = ctx.get_gc().gc_new<std::string>(gc::gc_object::of_none, false);
        *static_cast<std::string*>(r->value) = e.what();
        r->ok = 0;
    }
}

void parse_f32(si::context& ctx, si::operand_stack& stack)
{
    auto [container] = get_args<gc_object<std::string>>(ctx, stack);
    std::string* s = container.get();

    if(s == nullptr)
    {
        throw si::interpreter_error("parse_i32: argument cannot be null.");
    }

    std::size_t result_layout_id = ctx.get_gc().get_type_layout_id(si::make_type_name("std", "result"));
    std::size_t f32s_layout_id = ctx.get_gc().get_type_layout_id(si::make_type_name("std", "f32s"));

    auto* r = reinterpret_cast<result*>(ctx.get_gc().gc_new(    // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast)
      result_layout_id, sizeof(result), std::alignment_of_v<result>,
      gc::gc_object::of_temporary));
    stack.push_addr(r);

    try
    {
        // convert first, so that no memory for `i32s` is allocated in case of failure.
        float f = std::stof(*s);

        r->value = ctx.get_gc().gc_new(
          f32s_layout_id, sizeof(f32s), std::alignment_of_v<f32s>,
          gc::gc_object::of_none, false);
        static_cast<f32s*>(r->value)->value = f;
        r->ok = 1;
    }
    catch(const std::invalid_argument& e)
    {
        r->value = ctx.get_gc().gc_new<std::string>(gc::gc_object::of_none, false);
        *static_cast<std::string*>(r->value) = e.what();
        r->ok = 0;
    }
    catch(const std::out_of_range& e)
    {
        r->value = ctx.get_gc().gc_new<std::string>(gc::gc_object::of_none, false);
        *static_cast<std::string*>(r->value) = e.what();
        r->ok = 0;
    }
}

}    // namespace slang::runtime
