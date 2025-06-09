/**
 * slang - a simple scripting language.
 *
 * Interpreter tests.
 *
 * \author Felix Lubbe
 * \copyright Copyright (c) 2025
 * \license Distributed under the MIT software license (see accompanying LICENSE.txt).
 */

#include <format>
#include <print>

#include <gtest/gtest.h>

#include "archives/file.h"
#include "interpreter/interpreter.h"
#include "interpreter/invoke.h"
#include "runtime/runtime.h"
#include "shared/module.h"

namespace
{

namespace si = slang::interpreter;
namespace rt = slang::runtime;

TEST(interpreter, module_and_functions)
{
    slang::file_manager file_mgr;
    file_mgr.add_search_path(".");

    si::context ctx{file_mgr};
    ASSERT_NO_THROW(ctx.resolve_module("test_output.bin"));

    si::value res;
    ASSERT_NO_THROW(res = ctx.invoke("test_output.bin", "itest", {}));
    EXPECT_EQ(*res.get<int>(), 1);

    ASSERT_NO_THROW(res = ctx.invoke("test_output.bin", "iadd", {}));
    EXPECT_EQ(*res.get<int>(), 3);

    ASSERT_NO_THROW(res = ctx.invoke("test_output.bin", "isub", {}));
    EXPECT_EQ(*res.get<int>(), 1);

    ASSERT_NO_THROW(res = ctx.invoke("test_output.bin", "imul", {}));
    EXPECT_EQ(*res.get<int>(), 6);

    ASSERT_NO_THROW(res = ctx.invoke("test_output.bin", "idiv", {}));
    EXPECT_EQ(*res.get<int>(), 3);

    ASSERT_NO_THROW(res = ctx.invoke("test_output.bin", "ftest", {}));
    EXPECT_NEAR(*res.get<float>(), 1.1, 1e-6);

    ASSERT_NO_THROW(res = ctx.invoke("test_output.bin", "fadd", {}));
    EXPECT_NEAR(*res.get<float>(), 3.2, 1e-6);

    ASSERT_NO_THROW(res = ctx.invoke("test_output.bin", "fsub", {}));
    EXPECT_NEAR(*res.get<float>(), 1.0, 1e-6);

    ASSERT_NO_THROW(res = ctx.invoke("test_output.bin", "fmul", {}));
    EXPECT_NEAR(*res.get<float>(), 6.51, 1e-6);

    ASSERT_NO_THROW(res = ctx.invoke("test_output.bin", "fdiv", {}));
    EXPECT_NEAR(*res.get<float>(), 3.2, 1e-6);

    ASSERT_NO_THROW(res = ctx.invoke("test_output.bin", "stest", {}));
    EXPECT_EQ(*res.get<std::string>(), "Test");

    ASSERT_NO_THROW(res = ctx.invoke("test_output.bin", "arg", {si::value{1}}));
    EXPECT_EQ(*res.get<int>(), 2);
    ASSERT_NO_THROW(res = ctx.invoke("test_output.bin", "arg", {si::value{15}}));
    EXPECT_EQ(*res.get<int>(), 16);
    ASSERT_NO_THROW(res = ctx.invoke("test_output.bin", "arg", {si::value{-100}}));
    EXPECT_EQ(*res.get<int>(), -99);

    EXPECT_EQ(ctx.get_gc().object_count(), 0);
    EXPECT_EQ(ctx.get_gc().root_set_size(), 0);
    EXPECT_EQ(ctx.get_gc().byte_size(), 0);

    ASSERT_NO_THROW(res = ctx.invoke("test_output.bin", "arg2", {si::value{1.0f}}));
    EXPECT_NEAR(*res.get<float>(), 3.0, 1e-6);
    ASSERT_NO_THROW(res = ctx.invoke("test_output.bin", "arg2", {si::value{-1.0f}}));
    EXPECT_NEAR(*res.get<float>(), -1.0, 1e-6);
    ASSERT_NO_THROW(res = ctx.invoke("test_output.bin", "arg2", {si::value{0.f}}));
    EXPECT_NEAR(*res.get<float>(), 1.0, 1e-6);

    EXPECT_EQ(ctx.get_gc().object_count(), 0);
    EXPECT_EQ(ctx.get_gc().root_set_size(), 0);
    EXPECT_EQ(ctx.get_gc().byte_size(), 0);

    ASSERT_NO_THROW(res = ctx.invoke("test_output.bin", "sid", {si::value{"Test"}}));
    EXPECT_EQ(*res.get<std::string>(), "Test");

    ASSERT_NO_THROW(res = ctx.invoke("test_output.bin", "arg3", {si::value(1.0f), si::value{"Test"}}));
    EXPECT_NEAR(*res.get<float>(), 3.0, 1e-6);

    ASSERT_NO_THROW(res = ctx.invoke("test_output.bin", "call", {si::value{0}}));
    EXPECT_EQ(*res.get<int>(), 0);

    EXPECT_EQ(ctx.get_gc().object_count(), 0);
    EXPECT_EQ(ctx.get_gc().root_set_size(), 0);
    EXPECT_EQ(ctx.get_gc().byte_size(), 0);

    ASSERT_NO_THROW(res = ctx.invoke("test_output.bin", "local", {si::value{0}}));
    EXPECT_EQ(*res.get<int>(), -1);
    ASSERT_NO_THROW(res = ctx.invoke("test_output.bin", "local2", {si::value{0}}));
    EXPECT_EQ(*res.get<int>(), 1);
    ASSERT_NO_THROW(res = ctx.invoke("test_output.bin", "local3", {}));
    EXPECT_EQ(*res.get<std::string>(), "Test");

    EXPECT_EQ(ctx.get_gc().object_count(), 0);
    EXPECT_EQ(ctx.get_gc().root_set_size(), 0);
    EXPECT_EQ(ctx.get_gc().byte_size(), 0);

    ASSERT_NO_THROW(res = ctx.invoke("test_output.bin", "cast_i2f", {si::value{23}}));
    EXPECT_EQ(*res.get<float>(), 23.0);
    ASSERT_NO_THROW(res = ctx.invoke("test_output.bin", "cast_f2i", {si::value{92.3f}}));    // NOLINT(readability-magic-numbers)
    EXPECT_EQ(*res.get<int>(), 92);

    EXPECT_EQ(ctx.get_gc().object_count(), 0);
    EXPECT_EQ(ctx.get_gc().root_set_size(), 0);
    EXPECT_EQ(ctx.get_gc().byte_size(), 0);
}

/**
 * Register standard library functions and types.
 *
 * The functions `print` and `println` are redirected to a buffer.
 *
 * @param ctx The context to register the functions in.
 * @param print_buf Buffer for the print functions.
 */
void register_std_lib(si::context& ctx, std::vector<std::string>& print_buf)
{
    rt::register_builtin_type_layouts(ctx.get_gc());

    ctx.register_native_function("slang", "print",
                                 [&ctx, &print_buf](si::operand_stack& stack)
                                 {
                                     auto* s = stack.pop_addr<std::string>();
                                     print_buf.push_back(*s);
                                     ctx.get_gc().remove_temporary(s);
                                 });
    ctx.register_native_function("slang", "println",
                                 [&ctx, &print_buf](si::operand_stack& stack)
                                 {
                                     auto* s = stack.pop_addr<std::string>();
                                     print_buf.push_back(std::format("{}\n", *s));
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

TEST(interpreter, hello_world)
{
    slang::file_manager file_mgr;
    file_mgr.add_search_path(".");
    file_mgr.add_search_path("lang");
    si::context ctx{file_mgr};

    std::vector<std::string> print_buf;
    register_std_lib(ctx, print_buf);

    ASSERT_NO_THROW(ctx.resolve_module("hello_world"));
    EXPECT_NO_THROW(ctx.invoke("hello_world", "main", {si::value{std::vector<std::string>{"Test"}}}));
    ASSERT_EQ(print_buf.size(), 1);
    EXPECT_EQ(print_buf[0], "Hello, World!\n");

    // re-defining functions should fail.
    EXPECT_THROW(ctx.register_native_function("slang", "println",    // collides with a native function name
                                              []([[maybe_unused]] si::operand_stack& stack) {}),
                 si::interpreter_error);
    EXPECT_THROW(ctx.register_native_function("hello_world", "main",    // collides with a scripted function name
                                              []([[maybe_unused]] si::operand_stack& stack) {}),
                 si::interpreter_error);

    EXPECT_EQ(ctx.get_gc().object_count(), 0);
    EXPECT_EQ(ctx.get_gc().root_set_size(), 0);
    EXPECT_EQ(ctx.get_gc().byte_size(), 0);
}

TEST(interpreter, operators)
{
    slang::file_manager file_mgr;
    file_mgr.add_search_path(".");
    si::context ctx{file_mgr};

    ASSERT_NO_THROW(ctx.resolve_module("operators"));
    ASSERT_NO_THROW(ctx.invoke("operators", "main", {}));

    si::value res;
    ASSERT_NO_THROW(res = ctx.invoke("operators", "and", {si::value{27}, si::value{3}}));
    EXPECT_EQ(*res.get<int>(), 3);
    ASSERT_NO_THROW(res = ctx.invoke("operators", "land", {si::value{0}, si::value{0}}));
    EXPECT_EQ(*res.get<int>(), 0);
    ASSERT_NO_THROW(res = ctx.invoke("operators", "land", {si::value{1}, si::value{0}}));
    EXPECT_EQ(*res.get<int>(), 0);
    ASSERT_NO_THROW(res = ctx.invoke("operators", "land", {si::value{0}, si::value{1}}));
    EXPECT_EQ(*res.get<int>(), 0);
    ASSERT_NO_THROW(res = ctx.invoke("operators", "land", {si::value{1}, si::value{1}}));
    EXPECT_EQ(*res.get<int>(), 1);
    ASSERT_NO_THROW(res = ctx.invoke("operators", "or", {si::value{27}, si::value{4}}));
    EXPECT_EQ(*res.get<int>(), 31);
    ASSERT_NO_THROW(res = ctx.invoke("operators", "lor", {si::value{0}, si::value{0}}));
    EXPECT_EQ(*res.get<int>(), 0);
    ASSERT_NO_THROW(res = ctx.invoke("operators", "lor", {si::value{1}, si::value{0}}));
    EXPECT_EQ(*res.get<int>(), 1);
    ASSERT_NO_THROW(res = ctx.invoke("operators", "lor", {si::value{0}, si::value{1}}));
    EXPECT_EQ(*res.get<int>(), 1);
    ASSERT_NO_THROW(res = ctx.invoke("operators", "lor", {si::value{1}, si::value{1}}));
    EXPECT_EQ(*res.get<int>(), 1);
    ASSERT_NO_THROW(res = ctx.invoke("operators", "xor", {si::value{27}, si::value{3}}));
    EXPECT_EQ(*res.get<int>(), 24);
    ASSERT_NO_THROW(res = ctx.invoke("operators", "shl", {si::value{27}, si::value{3}}));
    EXPECT_EQ(*res.get<int>(), 216);
    ASSERT_NO_THROW(res = ctx.invoke("operators", "shr", {si::value{27}, si::value{3}}));
    EXPECT_EQ(*res.get<int>(), 3);
    ASSERT_NO_THROW(res = ctx.invoke("operators", "mod", {si::value{127}, si::value{23}}));
    EXPECT_EQ(*res.get<int>(), 12);

    EXPECT_EQ(ctx.get_gc().object_count(), 0);
    EXPECT_EQ(ctx.get_gc().root_set_size(), 0);
    EXPECT_EQ(ctx.get_gc().byte_size(), 0);
}

TEST(interpreter, control_flow)
{
    slang::file_manager file_mgr;
    file_mgr.add_search_path(".");
    file_mgr.add_search_path("lang");
    si::context ctx{file_mgr};

    std::vector<std::string> print_buf;
    register_std_lib(ctx, print_buf);

    ASSERT_NO_THROW(ctx.resolve_module("control_flow"));

    si::value res;

    ASSERT_NO_THROW(res = ctx.invoke("control_flow", "test_if_else", {si::value{2}}));
    EXPECT_EQ(*res.get<int>(), 1);
    ASSERT_NO_THROW(res = ctx.invoke("control_flow", "test_if_else", {si::value{-1}}));
    EXPECT_EQ(*res.get<int>(), 0);

    ASSERT_NO_THROW(ctx.invoke("control_flow", "conditional_hello_world", {si::value{3.f}}));    // NOLINT(readability-magic-numbers)
    ASSERT_EQ(print_buf.size(), 1);
    EXPECT_EQ(print_buf.back(), "Hello, World!\n");

    EXPECT_EQ(ctx.get_gc().object_count(), 0);
    EXPECT_EQ(ctx.get_gc().root_set_size(), 0);
    EXPECT_EQ(ctx.get_gc().byte_size(), 0);

    ASSERT_NO_THROW(ctx.invoke("control_flow", "conditional_hello_world", {si::value{2.2f}}));    // NOLINT(readability-magic-numbers)
    ASSERT_EQ(print_buf.size(), 2);
    EXPECT_EQ(print_buf.back(), "World, hello!\n");

    EXPECT_EQ(ctx.get_gc().object_count(), 0);
    EXPECT_EQ(ctx.get_gc().root_set_size(), 0);
    EXPECT_EQ(ctx.get_gc().byte_size(), 0);

    print_buf.clear();
    ASSERT_NO_THROW(ctx.invoke("control_flow", "no_else", {si::value{1}}));
    ASSERT_EQ(print_buf.size(), 2);
    EXPECT_EQ(print_buf[0], "a>0\n");
    EXPECT_EQ(print_buf[1], "Test\n");

    print_buf.clear();
    ASSERT_NO_THROW(ctx.invoke("control_flow", "no_else", {si::value{-2}}));
    ASSERT_EQ(print_buf.size(), 1);
    EXPECT_EQ(print_buf[0], "Test\n");

    EXPECT_EQ(ctx.get_gc().object_count(), 0);
    EXPECT_EQ(ctx.get_gc().root_set_size(), 0);
    EXPECT_EQ(ctx.get_gc().byte_size(), 0);
}

TEST(interpreter, loops)
{
    slang::file_manager file_mgr;
    file_mgr.add_search_path(".");
    file_mgr.add_search_path("lang");
    si::context ctx{file_mgr};

    std::vector<std::string> print_buf;
    register_std_lib(ctx, print_buf);

    ASSERT_NO_THROW(ctx.resolve_module("loops"));

    si::value res;

    ASSERT_NO_THROW(res = ctx.invoke("loops", "main", {}));
    EXPECT_EQ(print_buf.size(), 10);
    for(auto& it: print_buf)
    {
        EXPECT_EQ(it, "Hello, World!\n");
    }

    EXPECT_EQ(ctx.get_gc().object_count(), 0);
    EXPECT_EQ(ctx.get_gc().root_set_size(), 0);
    EXPECT_EQ(ctx.get_gc().byte_size(), 0);
}

TEST(interpreter, loop_break_continue)
{
    slang::file_manager file_mgr;
    file_mgr.add_search_path(".");
    file_mgr.add_search_path("lang");
    si::context ctx{file_mgr};

    std::vector<std::string> print_buf;
    register_std_lib(ctx, print_buf);

    ASSERT_NO_THROW(ctx.resolve_module("loops_bc"));

    si::value res;

    ASSERT_NO_THROW(res = ctx.invoke("loops_bc", "main_b", {}));
    ASSERT_EQ(print_buf.size(), 1);
    EXPECT_EQ(print_buf.back(), "Hello, World!\n");

    print_buf.clear();
    ASSERT_NO_THROW(res = ctx.invoke("loops_bc", "main_c", {}));
    ASSERT_EQ(print_buf.size(), 1);
    EXPECT_EQ(print_buf.back(), "Hello, World!\n");

    EXPECT_EQ(ctx.get_gc().object_count(), 0);
    EXPECT_EQ(ctx.get_gc().root_set_size(), 0);
    EXPECT_EQ(ctx.get_gc().byte_size(), 0);
}

TEST(interpreter, infinite_recursion)
{
    slang::file_manager file_mgr;
    file_mgr.add_search_path(".");
    si::context ctx{file_mgr};

    ASSERT_NO_THROW(ctx.resolve_module("inf_recursion"));

    si::value res;

    ASSERT_THROW(res = ctx.invoke("inf_recursion", "inf", {}), si::interpreter_error);

    EXPECT_EQ(ctx.get_gc().object_count(), 0);
    EXPECT_EQ(ctx.get_gc().root_set_size(), 0);
    EXPECT_EQ(ctx.get_gc().byte_size(), 0);
}

TEST(interpreter, arrays)
{
    slang::file_manager file_mgr;
    file_mgr.add_search_path(".");
    si::context ctx{file_mgr};

    ASSERT_NO_THROW(ctx.resolve_module("arrays"));

    si::value res;

    ASSERT_NO_THROW(res = ctx.invoke("arrays", "f", {}));
    EXPECT_EQ(*res.get<int>(), 2);

    EXPECT_EQ(ctx.get_gc().object_count(), 0);
    EXPECT_EQ(ctx.get_gc().root_set_size(), 0);
    EXPECT_EQ(ctx.get_gc().byte_size(), 0);

    ASSERT_NO_THROW(res = ctx.invoke("arrays", "g", {}));
    EXPECT_EQ(*res.get<int>(), 3);

    EXPECT_EQ(ctx.get_gc().object_count(), 0);
    EXPECT_EQ(ctx.get_gc().root_set_size(), 0);
    EXPECT_EQ(ctx.get_gc().byte_size(), 0);
}

TEST(interpreter, return_arrays)
{
    slang::file_manager file_mgr;
    file_mgr.add_search_path(".");
    si::context ctx{file_mgr};

    ASSERT_NO_THROW(ctx.resolve_module("return_array"));

    si::value res;

    ASSERT_NO_THROW(res = ctx.invoke("return_array", "return_array", {}));

    const auto* v_ptr = reinterpret_cast<si::fixed_vector<int>* const*>(res.get<void*>());    // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast)
    ASSERT_NE(v_ptr, nullptr);

    auto* v = *v_ptr;
    ASSERT_EQ(v->size(), 2);
    EXPECT_EQ((*v)[0], 1);
    EXPECT_EQ((*v)[1], 2);

    ASSERT_NO_THROW(ctx.get_gc().remove_temporary(v));
    ASSERT_NO_THROW(ctx.get_gc().run());

    EXPECT_EQ(ctx.get_gc().object_count(), 0);
    EXPECT_EQ(ctx.get_gc().root_set_size(), 0);
    EXPECT_EQ(ctx.get_gc().byte_size(), 0);
}

TEST(interpreter, pass_array)
{
    slang::file_manager file_mgr;
    file_mgr.add_search_path(".");
    si::context ctx{file_mgr};

    ASSERT_NO_THROW(ctx.resolve_module("return_array"));

    si::value res;

    ASSERT_NO_THROW(res = ctx.invoke("return_array", "pass_array", {}));
    EXPECT_EQ(*res.get<int>(), 3);

    ASSERT_NO_THROW(res = ctx.invoke("return_array", "f", {si::value{std::vector<int>{1, 2}}}));
    EXPECT_EQ(*res.get<int>(), 2);

    EXPECT_EQ(ctx.get_gc().object_count(), 0);
    EXPECT_EQ(ctx.get_gc().root_set_size(), 0);
    EXPECT_EQ(ctx.get_gc().byte_size(), 0);
}

TEST(interpreter, invalid_index)
{
    slang::file_manager file_mgr;
    file_mgr.add_search_path(".");
    si::context ctx{file_mgr};

    ASSERT_NO_THROW(ctx.resolve_module("return_array"));

    ASSERT_THROW(ctx.invoke("return_array", "invalid_index", {}), si::interpreter_error);
}

TEST(interpreter, return_str_array)
{
    slang::file_manager file_mgr;
    file_mgr.add_search_path(".");
    si::context ctx{file_mgr};

    ASSERT_NO_THROW(ctx.resolve_module("return_array"));

    si::value res;

    ASSERT_NO_THROW(res = ctx.invoke("return_array", "str_array", {}));

    {
        const auto* array_ptr = reinterpret_cast<si::fixed_vector<std::string*>* const*>(res.get<void*>());    // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast)
        ASSERT_NE(array_ptr, nullptr);

        auto* array = *array_ptr;
        ASSERT_EQ(array->size(), 3);
        EXPECT_EQ(*(*array)[0], "a");
        EXPECT_EQ(*(*array)[1], "test");
        EXPECT_EQ(*(*array)[2], "123");

        ASSERT_NO_THROW(ctx.get_gc().remove_temporary(array));
        ASSERT_NO_THROW(ctx.get_gc().run());

        EXPECT_EQ(ctx.get_gc().object_count(), 0);
        EXPECT_EQ(ctx.get_gc().root_set_size(), 0);
        EXPECT_EQ(ctx.get_gc().byte_size(), 0);
    }

    ASSERT_NO_THROW(res = ctx.invoke("return_array", "ret_str", {}));
    EXPECT_EQ(*res.get<std::string>(), "123");

    ASSERT_NO_THROW(res = ctx.invoke("return_array", "call_return", {}));
    EXPECT_EQ(*res.get<int>(), 1);

    EXPECT_EQ(ctx.get_gc().object_count(), 0);
    EXPECT_EQ(ctx.get_gc().root_set_size(), 0);
    EXPECT_EQ(ctx.get_gc().byte_size(), 0);
}

TEST(interpreter, array_length)
{
    slang::file_manager file_mgr;
    file_mgr.add_search_path(".");
    si::context ctx{file_mgr};

    ASSERT_NO_THROW(ctx.resolve_module("array_length"));

    si::value res;

    ASSERT_NO_THROW(res = ctx.invoke("array_length", "len", {}));
    EXPECT_EQ(*res.get<int>(), 2);

    EXPECT_EQ(ctx.get_gc().object_count(), 0);
    EXPECT_EQ(ctx.get_gc().root_set_size(), 0);
    EXPECT_EQ(ctx.get_gc().byte_size(), 0);

    ASSERT_THROW(ctx.invoke("array_length", "len2", {}), si::interpreter_error);
}

TEST(interpreter, array_copy)
{
    slang::file_manager file_mgr;
    file_mgr.add_search_path(".");
    si::context ctx{file_mgr};

    std::vector<std::string> print_buf;
    register_std_lib(ctx, print_buf);

    ASSERT_NO_THROW(ctx.resolve_module("array_copy"));

    si::value res;

    ASSERT_NO_THROW(res = ctx.invoke("array_copy", "test_copy", {}));
    ASSERT_EQ(*res.get<int>(), 1);

    ASSERT_NO_THROW(res = ctx.invoke("array_copy", "test_copy_str", {}));
    ASSERT_EQ(*res.get<int>(), 1);

    EXPECT_EQ(ctx.get_gc().object_count(), 0);
    EXPECT_EQ(ctx.get_gc().root_set_size(), 0);
    EXPECT_EQ(ctx.get_gc().byte_size(), 0);

    ASSERT_THROW(ctx.invoke("array_copy", "test_copy_fail_none", {}), si::interpreter_error);
    ASSERT_THROW(ctx.invoke("array_copy", "test_copy_fail_type", {}), si::interpreter_error);
}

TEST(interpreter, string_operations)
{
    slang::file_manager file_mgr;
    file_mgr.add_search_path(".");
    si::context ctx{file_mgr};

    std::vector<std::string> print_buf;
    register_std_lib(ctx, print_buf);

    ASSERT_NO_THROW(ctx.resolve_module("string_operations"));

    si::value res;

    ASSERT_NO_THROW(res = ctx.invoke("string_operations", "main", {}));
    ASSERT_EQ(*res.get<int>(), 10);

    EXPECT_EQ(ctx.get_gc().object_count(), 0);
    EXPECT_EQ(ctx.get_gc().root_set_size(), 0);
    EXPECT_EQ(ctx.get_gc().byte_size(), 0);
}

TEST(interpreter, operator_new)
{
    slang::file_manager file_mgr;
    file_mgr.add_search_path(".");
    si::context ctx{file_mgr};

    ASSERT_NO_THROW(ctx.resolve_module("return_array"));

    si::value res;

    ASSERT_NO_THROW(res = ctx.invoke("return_array", "new_array", {}));

    EXPECT_EQ(ctx.get_gc().object_count(), 0);
    EXPECT_EQ(ctx.get_gc().root_set_size(), 0);
    EXPECT_EQ(ctx.get_gc().byte_size(), 0);

    ASSERT_THROW(ctx.invoke("return_array", "new_array_invalid_size", {}), si::interpreter_error);
}

TEST(interpreter, prefix_postfix)
{
    slang::file_manager file_mgr;
    file_mgr.add_search_path(".");
    si::context ctx{file_mgr};

    ASSERT_NO_THROW(ctx.resolve_module("prefix_postfix"));

    si::value res;

    ASSERT_NO_THROW(res = ctx.invoke("prefix_postfix", "prefix_add_i32", {si::value{1}}));
    ASSERT_EQ(*res.get<int>(), 2);
    ASSERT_NO_THROW(res = ctx.invoke("prefix_postfix", "postfix_add_i32", {si::value{1}}));
    ASSERT_EQ(*res.get<int>(), 1);
    ASSERT_NO_THROW(res = ctx.invoke("prefix_postfix", "prefix_sub_i32", {si::value{1}}));
    ASSERT_EQ(*res.get<int>(), 0);
    ASSERT_NO_THROW(res = ctx.invoke("prefix_postfix", "postfix_sub_i32", {si::value{1}}));
    ASSERT_EQ(*res.get<int>(), 1);
    ASSERT_NO_THROW(res = ctx.invoke("prefix_postfix", "prefix_add_f32", {si::value{1.f}}));
    ASSERT_EQ(*res.get<float>(), 2.f);
    ASSERT_NO_THROW(res = ctx.invoke("prefix_postfix", "postfix_add_f32", {si::value{1.f}}));
    ASSERT_EQ(*res.get<float>(), 1.f);
    ASSERT_NO_THROW(res = ctx.invoke("prefix_postfix", "prefix_sub_f32", {si::value{1.f}}));
    ASSERT_EQ(*res.get<float>(), 0.f);
    ASSERT_NO_THROW(res = ctx.invoke("prefix_postfix", "postfix_sub_f32", {si::value{1.f}}));
    ASSERT_EQ(*res.get<float>(), 1.f);

    EXPECT_EQ(ctx.get_gc().object_count(), 0);
    EXPECT_EQ(ctx.get_gc().root_set_size(), 0);
    EXPECT_EQ(ctx.get_gc().byte_size(), 0);
}

TEST(interpreter, return_discard)
{
    slang::file_manager file_mgr;
    file_mgr.add_search_path(".");
    si::context ctx{file_mgr};

    ASSERT_NO_THROW(ctx.resolve_module("return_discard"));

    si::value res;

    ASSERT_NO_THROW(res = ctx.invoke("return_discard", "f", {}));

    EXPECT_EQ(ctx.get_gc().object_count(), 0);
    EXPECT_EQ(ctx.get_gc().root_set_size(), 0);
    EXPECT_EQ(ctx.get_gc().byte_size(), 0);
}

TEST(interpreter, return_discard_array)
{
    slang::file_manager file_mgr;
    file_mgr.add_search_path(".");
    si::context ctx{file_mgr};

    ASSERT_NO_THROW(ctx.resolve_module("return_discard_array"));

    si::value res;

    ASSERT_NO_THROW(res = ctx.invoke("return_discard_array", "f", {}));

    EXPECT_EQ(ctx.get_gc().object_count(), 0);
    EXPECT_EQ(ctx.get_gc().root_set_size(), 0);
    EXPECT_EQ(ctx.get_gc().byte_size(), 0);
}

TEST(interpreter, return_discard_strings)
{
    slang::file_manager file_mgr;
    file_mgr.add_search_path(".");
    si::context ctx{file_mgr};

    ASSERT_NO_THROW(ctx.resolve_module("return_discard_strings"));

    si::value res;

    ASSERT_NO_THROW(res = ctx.invoke("return_discard_strings", "f", {}));

    EXPECT_EQ(ctx.get_gc().object_count(), 0);
    EXPECT_EQ(ctx.get_gc().root_set_size(), 0);
    EXPECT_EQ(ctx.get_gc().byte_size(), 0);
}

TEST(interpreter, structs)
{
    slang::module_::language_module mod;

    try
    {
        slang::file_read_archive read_ar("structs.cmod");
        EXPECT_NO_THROW(read_ar & mod);
    }
    catch(const std::runtime_error& e)
    {
        std::print("Error loading 'structs.cmod'. Make sure to run 'test_output' to generate the file.\n");
        throw e;
    }

    const auto& header = mod.get_header();
    ASSERT_EQ(header.exports.size(), 2);
    EXPECT_EQ(header.exports[0].type, slang::module_::symbol_type::type);
    EXPECT_EQ(header.exports[0].name, "S");
    EXPECT_EQ(header.exports[1].type, slang::module_::symbol_type::type);
    EXPECT_EQ(header.exports[1].name, "T");

    {
        const auto& desc = std::get<slang::module_::struct_descriptor>(header.exports[0].desc);
        ASSERT_EQ(desc.member_types.size(), 2);
        EXPECT_EQ(desc.member_types[0].first, "i");
        EXPECT_EQ(desc.member_types[0].second, slang::module_::field_descriptor("i32", false));
        EXPECT_EQ(desc.member_types[1].first, "j");
        EXPECT_EQ(desc.member_types[1].second, slang::module_::field_descriptor("f32", false));
    }

    {
        const auto& desc = std::get<slang::module_::struct_descriptor>(header.exports[1].desc);
        ASSERT_EQ(desc.member_types.size(), 2);
        EXPECT_EQ(desc.member_types[0].first, "s");
        EXPECT_EQ(desc.member_types[0].second, slang::module_::field_descriptor("S", false));
        EXPECT_EQ(desc.member_types[1].first, "t");
        EXPECT_EQ(desc.member_types[1].second, slang::module_::field_descriptor("str", false));
    }

    slang::file_manager file_mgr;
    file_mgr.add_search_path(".");
    si::context ctx{file_mgr};

    ASSERT_NO_THROW(ctx.resolve_module("structs"));

    EXPECT_EQ(ctx.get_gc().object_count(), 0);
    EXPECT_EQ(ctx.get_gc().root_set_size(), 0);
    EXPECT_EQ(ctx.get_gc().byte_size(), 0);
}

TEST(interpreter, structs_access)
{
    {
        slang::file_manager file_mgr;
        file_mgr.add_search_path(".");
        si::context ctx{file_mgr};

        ASSERT_NO_THROW(ctx.resolve_module("structs_access"));

        si::value res;

        ASSERT_NO_THROW(res = ctx.invoke("structs_access", "test", {}));
        EXPECT_EQ(*res.get<int>(), 4);

        EXPECT_EQ(ctx.get_gc().object_count(), 0);
        EXPECT_EQ(ctx.get_gc().root_set_size(), 0);
        EXPECT_EQ(ctx.get_gc().byte_size(), 0);
    }
    {
        slang::file_manager file_mgr;
        file_mgr.add_search_path(".");
        si::context ctx{file_mgr};

        ASSERT_NO_THROW(ctx.resolve_module("structs_access2"));

        si::value res;

        ASSERT_NO_THROW(res = ctx.invoke("structs_access2", "test", {}));
        EXPECT_EQ(*res.get<int>(), 2);

        EXPECT_EQ(ctx.get_gc().object_count(), 0);
        EXPECT_EQ(ctx.get_gc().root_set_size(), 0);
        EXPECT_EQ(ctx.get_gc().byte_size(), 0);
    }
    {
        slang::file_manager file_mgr;
        file_mgr.add_search_path(".");
        si::context ctx{file_mgr};

        ASSERT_NO_THROW(ctx.resolve_module("structs_access2"));

        si::value res;

        ASSERT_NO_THROW(res = ctx.invoke("structs_access2", "test_local", {}));
        EXPECT_EQ(*res.get<int>(), 4);

        EXPECT_EQ(ctx.get_gc().object_count(), 0);
        EXPECT_EQ(ctx.get_gc().root_set_size(), 0);
        EXPECT_EQ(ctx.get_gc().byte_size(), 0);
    }
    {
        slang::file_manager file_mgr;
        file_mgr.add_search_path(".");
        si::context ctx{file_mgr};

        ASSERT_NO_THROW(ctx.resolve_module("structs_references"));

        ASSERT_NO_THROW(ctx.invoke("structs_references", "test", {}));

        EXPECT_EQ(ctx.get_gc().object_count(), 0);
        EXPECT_EQ(ctx.get_gc().root_set_size(), 0);
        EXPECT_EQ(ctx.get_gc().byte_size(), 0);
    }
    {
        slang::file_manager file_mgr;
        file_mgr.add_search_path(".");
        si::context ctx{file_mgr};

        ASSERT_NO_THROW(ctx.resolve_module("null_dereference"));

        ASSERT_THROW(ctx.invoke("null_dereference", "test", {}), si::interpreter_error);

        EXPECT_EQ(ctx.get_gc().object_count(), 0);
        EXPECT_EQ(ctx.get_gc().root_set_size(), 0);
        EXPECT_EQ(ctx.get_gc().byte_size(), 0);
    }
}

TEST(interpreter, returned_struct)
{
    {
        slang::file_manager file_mgr;
        file_mgr.add_search_path(".");
        si::context ctx{file_mgr};

        ASSERT_NO_THROW(ctx.resolve_module("return_struct"));

        si::value res;
        ASSERT_NO_THROW(res = ctx.invoke("return_struct", "return_struct", {}));

        // needs to match the definition inside the script.
        struct S
        {
            std::int32_t i;
            float j;
            std::string* s;
        };

        S* s = static_cast<S*>(*res.get<void*>());

        EXPECT_EQ(s->i, 1);
        EXPECT_FLOAT_EQ(s->j, 2.3);
        EXPECT_EQ(*s->s, "test");

        ctx.get_gc().remove_temporary(s);
        ctx.get_gc().run();

        EXPECT_EQ(ctx.get_gc().object_count(), 0);
        EXPECT_EQ(ctx.get_gc().root_set_size(), 0);
        EXPECT_EQ(ctx.get_gc().byte_size(), 0);
    }
}

TEST(interpreter, struct_argument)
{
    {
        slang::file_manager file_mgr;
        file_mgr.add_search_path(".");
        si::context ctx{file_mgr};

        ASSERT_NO_THROW(ctx.resolve_module("struct_arg"));

        // needs to match the definition inside the script.
        struct S
        {
            std::int32_t i;
            float j;
            std::string* s;
        };
        S s{0, 0.f, nullptr};

        std::size_t layout_id = 0;
        ASSERT_NO_THROW(layout_id = ctx.get_gc().get_type_layout_id(si::make_type_name("struct_arg", "S")));

        ASSERT_NO_THROW(ctx.invoke("struct_arg", "struct_arg", {si::value{layout_id, &s}}));

        // prevent garbage collection for `s.s`.
        ctx.get_gc().add_root(s.s);

        EXPECT_EQ(s.i, 1);
        EXPECT_FLOAT_EQ(s.j, 2.3);
        EXPECT_EQ(*s.s, "test");

        ctx.get_gc().run();

        EXPECT_EQ(ctx.get_gc().object_count(), 1);
        EXPECT_EQ(ctx.get_gc().root_set_size(), 1);

        // collect `s.s`.
        ctx.get_gc().remove_root(s.s);
        ctx.get_gc().run();

        EXPECT_EQ(ctx.get_gc().object_count(), 0);
        EXPECT_EQ(ctx.get_gc().root_set_size(), 0);
        EXPECT_EQ(ctx.get_gc().byte_size(), 0);
    }
}

TEST(interpreter, nested_structs)
{
    {
        slang::file_manager file_mgr;
        file_mgr.add_search_path(".");
        si::context ctx{file_mgr};

        ASSERT_NO_THROW(ctx.resolve_module("nested_structs"));

        ASSERT_NO_THROW(ctx.invoke("nested_structs", "test", {}));

        EXPECT_EQ(ctx.get_gc().object_count(), 0);
        EXPECT_EQ(ctx.get_gc().root_set_size(), 0);
        EXPECT_EQ(ctx.get_gc().byte_size(), 0);
    }
    {
        slang::file_manager file_mgr;
        file_mgr.add_search_path(".");
        si::context ctx{file_mgr};

        ASSERT_NO_THROW(ctx.resolve_module("nested_structs2"));

        si::value res;
        ASSERT_NO_THROW(res = ctx.invoke("nested_structs2", "test", {}));

        EXPECT_EQ(*res.get<int>(), 2);

        EXPECT_EQ(ctx.get_gc().object_count(), 0);
        EXPECT_EQ(ctx.get_gc().root_set_size(), 0);
        EXPECT_EQ(ctx.get_gc().byte_size(), 0);
    }
}

TEST(interpreter, type_imports)
{
    {
        slang::file_manager file_mgr;
        file_mgr.add_search_path(".");

        si::context ctx{file_mgr};

        si::value res;
        ASSERT_NO_THROW(ctx.resolve_module("type_imports"));
        ASSERT_NO_THROW(res = ctx.invoke("type_imports", "test", {}));

        EXPECT_EQ(*res.get<int>(), 2);

        EXPECT_EQ(ctx.get_gc().object_count(), 0);
        EXPECT_EQ(ctx.get_gc().root_set_size(), 0);
        EXPECT_EQ(ctx.get_gc().byte_size(), 0);
    }
}

TEST(interpreter, null_access)
{
    {
        slang::file_manager file_mgr;
        file_mgr.add_search_path(".");

        si::context ctx{file_mgr};

        si::value res;
        ASSERT_NO_THROW(ctx.resolve_module("null_access"));
        ASSERT_THROW(res = ctx.invoke("null_access", "main", {}), si::interpreter_error);

        EXPECT_EQ(ctx.get_gc().object_count(), 0);
        EXPECT_EQ(ctx.get_gc().root_set_size(), 0);
        EXPECT_EQ(ctx.get_gc().byte_size(), 0);
    }
}

TEST(interpreter, multiple_modules)
{
    slang::file_manager file_mgr;
    file_mgr.add_search_path(".");

    si::context ctx{file_mgr};

    ASSERT_NO_THROW(ctx.resolve_module("mod3"));

    si::value res;

    ASSERT_NO_THROW(res = ctx.invoke("mod3", "f", {si::value{1.0f}}));
    EXPECT_EQ(*res.get<int>(), 4);

    EXPECT_EQ(ctx.get_gc().object_count(), 0);
    EXPECT_EQ(ctx.get_gc().root_set_size(), 0);
    EXPECT_EQ(ctx.get_gc().byte_size(), 0);
}

TEST(interpreter, invocation_api)
{
    {
        slang::file_manager file_mgr;
        file_mgr.add_search_path(".");

        si::context ctx{file_mgr};

        ASSERT_NO_THROW(ctx.resolve_module("mod3"));

        si::value res;

        ASSERT_NO_THROW(res = si::invoke(ctx, "mod3", "f", 1.0f));
        EXPECT_EQ(*res.get<int>(), 4);

        EXPECT_EQ(ctx.get_gc().object_count(), 0);
        EXPECT_EQ(ctx.get_gc().root_set_size(), 0);
        EXPECT_EQ(ctx.get_gc().byte_size(), 0);
    }
    {
        slang::file_manager file_mgr;
        file_mgr.add_search_path(".");

        si::context ctx{file_mgr};
        si::module_loader* loader{nullptr};

        ASSERT_NO_THROW(loader = &ctx.resolve_module("mod3"));
        si::function& f = loader->get_function("f");

        si::value res;
        ASSERT_NO_THROW(res = si::invoke(f, 1.0f));
        EXPECT_EQ(*res.get<int>(), 4);

        EXPECT_EQ(ctx.get_gc().object_count(), 0);
        EXPECT_EQ(ctx.get_gc().root_set_size(), 0);
        EXPECT_EQ(ctx.get_gc().byte_size(), 0);
    }
}

}    // namespace
