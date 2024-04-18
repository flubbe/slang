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
    EXPECT_EQ(std::get<int>(res), 1);

    EXPECT_NO_THROW(res = ctx.invoke("test_output", "iadd", {}));
    EXPECT_EQ(std::get<int>(res), 3);

    EXPECT_NO_THROW(res = ctx.invoke("test_output", "isub", {}));
    EXPECT_EQ(std::get<int>(res), 1);

    EXPECT_NO_THROW(res = ctx.invoke("test_output", "imul", {}));
    EXPECT_EQ(std::get<int>(res), 6);

    EXPECT_NO_THROW(res = ctx.invoke("test_output", "idiv", {}));
    EXPECT_EQ(std::get<int>(res), 3);

    EXPECT_NO_THROW(res = ctx.invoke("test_output", "ftest", {}));
    EXPECT_NEAR(std::get<float>(res), 1.1, 1e-6);

    EXPECT_NO_THROW(res = ctx.invoke("test_output", "fadd", {}));
    EXPECT_NEAR(std::get<float>(res), 3.2, 1e-6);

    EXPECT_NO_THROW(res = ctx.invoke("test_output", "fsub", {}));
    EXPECT_NEAR(std::get<float>(res), 1.0, 1e-6);

    EXPECT_NO_THROW(res = ctx.invoke("test_output", "fmul", {}));
    EXPECT_NEAR(std::get<float>(res), 6.51, 1e-6);

    EXPECT_NO_THROW(res = ctx.invoke("test_output", "fdiv", {}));
    EXPECT_NEAR(std::get<float>(res), 3.2, 1e-6);

    EXPECT_NO_THROW(res = ctx.invoke("test_output", "stest", {}));
    EXPECT_EQ(std::get<std::string>(res), "Test");

    EXPECT_NO_THROW(res = ctx.invoke("test_output", "arg", {1}));
    EXPECT_EQ(std::get<int>(res), 2);
    EXPECT_NO_THROW(res = ctx.invoke("test_output", "arg", {15}));
    EXPECT_EQ(std::get<int>(res), 16);
    EXPECT_NO_THROW(res = ctx.invoke("test_output", "arg", {-100}));
    EXPECT_EQ(std::get<int>(res), -99);

    EXPECT_NO_THROW(res = ctx.invoke("test_output", "arg2", {1.0f}));
    EXPECT_NEAR(std::get<float>(res), 3.0, 1e-6);
    EXPECT_NO_THROW(res = ctx.invoke("test_output", "arg2", {-1.0f}));
    EXPECT_NEAR(std::get<float>(res), -1.0, 1e-6);
    EXPECT_NO_THROW(res = ctx.invoke("test_output", "arg2", {0.f}));
    EXPECT_NEAR(std::get<float>(res), 1.0, 1e-6);

    EXPECT_NO_THROW(res = ctx.invoke("test_output", "sid", {"Test"}));
    EXPECT_EQ(std::get<std::string>(res), "Test");

    EXPECT_NO_THROW(res = ctx.invoke("test_output", "call", {0}));
    EXPECT_EQ(std::get<int>(res), 0);
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

    ctx.register_native_function("slang", "print",
                                 [](slang::interpreter::operand_stack& stack)
                                 {
                                     std::string* s = stack.pop_addr<std::string>();
                                     fmt::print("{}", *s);
                                 });
    ctx.register_native_function("slang", "println",
                                 [](slang::interpreter::operand_stack& stack)
                                 {
                                     std::string* s = stack.pop_addr<std::string>();
                                     fmt::print("{}\n", *s);
                                 });

    ASSERT_NO_THROW(ctx.load_module("hello_world", mod));
    EXPECT_NO_THROW(ctx.invoke("hello_world", "main", {"Test"}));
}

}    // namespace