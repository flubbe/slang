/**
 * slang - a simple scripting language.
 *
 * Interpreter tests.
 *
 * \author Felix Lubbe
 * \copyright Copyright (c) 2024
 * \license Distributed under the MIT software license (see accompanying LICENSE.txt).
 */

#include <fmt/core.h>
#include <gtest/gtest.h>

#include "module.h"

#include "archives/file.h"
#include "interpreter.h"

namespace
{

TEST(interpreter, loading)
{
    slang::language_module mod;

    try
    {
        slang::file_read_archive read_ar("test_output.bin");
        EXPECT_NO_THROW(read_ar & mod);
    }
    catch(const std::runtime_error& e)
    {
        fmt::print("Error loading 'test_output.bin'. Make sure to run 'test_output' to generate the file.\n");
        throw e;
    }

    slang::file_manager file_mgr;
    slang::interpreter::context ctx{file_mgr};
    ASSERT_NO_THROW(ctx.load_module("test_output", mod));

    slang::interpreter::value res;
    EXPECT_NO_THROW(res = ctx.invoke("test_output", "itest", {}));
    EXPECT_EQ(*res.get<int>(), 1);

    EXPECT_NO_THROW(res = ctx.invoke("test_output", "iadd", {}));
    EXPECT_EQ(*res.get<int>(), 3);

    EXPECT_NO_THROW(res = ctx.invoke("test_output", "isub", {}));
    EXPECT_EQ(*res.get<int>(), 1);

    EXPECT_NO_THROW(res = ctx.invoke("test_output", "imul", {}));
    EXPECT_EQ(*res.get<int>(), 6);

    EXPECT_NO_THROW(res = ctx.invoke("test_output", "idiv", {}));
    EXPECT_EQ(*res.get<int>(), 3);

    EXPECT_NO_THROW(res = ctx.invoke("test_output", "ftest", {}));
    EXPECT_NEAR(*res.get<float>(), 1.1, 1e-6);

    EXPECT_NO_THROW(res = ctx.invoke("test_output", "fadd", {}));
    EXPECT_NEAR(*res.get<float>(), 3.2, 1e-6);

    EXPECT_NO_THROW(res = ctx.invoke("test_output", "fsub", {}));
    EXPECT_NEAR(*res.get<float>(), 1.0, 1e-6);

    EXPECT_NO_THROW(res = ctx.invoke("test_output", "fmul", {}));
    EXPECT_NEAR(*res.get<float>(), 6.51, 1e-6);

    EXPECT_NO_THROW(res = ctx.invoke("test_output", "fdiv", {}));
    EXPECT_NEAR(*res.get<float>(), 3.2, 1e-6);

    EXPECT_NO_THROW(res = ctx.invoke("test_output", "stest", {}));
    EXPECT_EQ(*res.get<std::string>(), "Test");

    EXPECT_NO_THROW(res = ctx.invoke("test_output", "arg", {1}));
    EXPECT_EQ(*res.get<int>(), 2);
    EXPECT_NO_THROW(res = ctx.invoke("test_output", "arg", {15}));
    EXPECT_EQ(*res.get<int>(), 16);
    EXPECT_NO_THROW(res = ctx.invoke("test_output", "arg", {-100}));
    EXPECT_EQ(*res.get<int>(), -99);

    EXPECT_NO_THROW(res = ctx.invoke("test_output", "arg2", {1.0f}));
    EXPECT_NEAR(*res.get<float>(), 3.0, 1e-6);
    EXPECT_NO_THROW(res = ctx.invoke("test_output", "arg2", {-1.0f}));
    EXPECT_NEAR(*res.get<float>(), -1.0, 1e-6);
    EXPECT_NO_THROW(res = ctx.invoke("test_output", "arg2", {0.f}));
    EXPECT_NEAR(*res.get<float>(), 1.0, 1e-6);

    EXPECT_NO_THROW(res = ctx.invoke("test_output", "sid", {"Test"}));
    EXPECT_EQ(*res.get<std::string>(), "Test");

    EXPECT_NO_THROW(res = ctx.invoke("test_output", "call", {0}));
    EXPECT_EQ(*res.get<int>(), 0);
}

TEST(interpreter, hello_world)
{
    slang::language_module mod;

    try
    {
        slang::file_read_archive read_ar("hello_world.cmod");
        EXPECT_NO_THROW(read_ar & mod);
    }
    catch(const std::runtime_error& e)
    {
        fmt::print("Error loading 'hello_world.cmod'. Make sure to run 'test_output' to generate the file.\n");
        throw e;
    }

    slang::file_manager file_mgr;
    file_mgr.add_search_path("src/lang");
    slang::interpreter::context ctx{file_mgr};

    std::vector<std::string> print_buf;

    ctx.register_native_function("slang", "print",
                                 [&print_buf](slang::interpreter::operand_stack& stack)
                                 {
                                     std::string* s = stack.pop_addr<std::string>();
                                     print_buf.push_back(*s);
                                 });
    ctx.register_native_function("slang", "println",
                                 [&print_buf](slang::interpreter::operand_stack& stack)
                                 {
                                     std::string* s = stack.pop_addr<std::string>();
                                     print_buf.push_back(fmt::format("{}\n", *s));
                                 });

    ASSERT_NO_THROW(ctx.load_module("hello_world", mod));
    EXPECT_NO_THROW(ctx.invoke("hello_world", "main", {"Test"}));
    ASSERT_EQ(print_buf.size(), 1);
    EXPECT_EQ(print_buf[0], "Hello, World!\n");

    // re-defining functions should fail.
    EXPECT_THROW(ctx.register_native_function("slang", "println",
                                              [](slang::interpreter::operand_stack& stack) {}),
                 slang::interpreter::interpreter_error);
    EXPECT_THROW(ctx.register_native_function("hello_world", "main",
                                              [](slang::interpreter::operand_stack& stack) {}),
                 slang::interpreter::interpreter_error);
}

}    // namespace