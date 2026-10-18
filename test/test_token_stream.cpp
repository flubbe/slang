/**
 * slang - a simple scripting language.
 *
 * token stream tests.
 *
 * \author Felix Lubbe
 * \copyright Copyright (c) 2026
 * \license Distributed under the MIT software license (see accompanying LICENSE.txt).
 */

#include <gtest/gtest.h>

#include "archives/memory.h"
#include "compiler/lexer.h"
#include "compiler/token_stream.h"

namespace
{

TEST(token_stream, empty)
{
    slang::token_stream ts;
    EXPECT_TRUE(ts.empty());
    EXPECT_EQ(ts.size(), 0);
}

TEST(token_stream, from_vector)
{
    std::vector<slang::token> tokens = {
      slang::token{"a", {1, 1}, slang::token_type::identifier},
      slang::token{"+", {1, 3}, slang::token_type::delimiter},
      slang::token{"b", {1, 5}, slang::token_type::identifier}};

    slang::token_stream ts{tokens};
    EXPECT_FALSE(ts.empty());
    EXPECT_EQ(ts.size(), 3);
    EXPECT_EQ(ts[0].s, "a");
    EXPECT_EQ(ts[1].s, "+");
    EXPECT_EQ(ts[2].s, "b");
    EXPECT_EQ(ts.front().s, "a");
    EXPECT_EQ(ts.back().s, "b");
}

TEST(token_stream, from_lexer)
{
    slang::token_stream ts{
      slang::lexer::lex(
        "let x: i32 = 42;")};

    EXPECT_FALSE(ts.empty());
    EXPECT_EQ(ts.size(), 7);    // "let", "x", ":", "i32", "=", "42", ";"
    EXPECT_EQ(ts[0].s, "let");
    EXPECT_EQ(ts[1].s, "x");
    EXPECT_EQ(ts[5].s, "42");
    EXPECT_EQ(ts[6].s, ";");
}

TEST(token_stream, append)
{
    slang::token_stream ts;
    slang::token t1{"foo", {1, 1}, slang::token_type::identifier};
    slang::token t2{"bar", {1, 5}, slang::token_type::identifier};

    auto ts1 = ts.append(t1);
    auto ts2 = ts1.append(t2);

    EXPECT_TRUE(ts.empty());
    EXPECT_EQ(ts1.size(), 1);
    EXPECT_EQ(ts1[0].s, "foo");
    EXPECT_EQ(ts2.size(), 2);
    EXPECT_EQ(ts2[0].s, "foo");
    EXPECT_EQ(ts2[1].s, "bar");
}

TEST(token_stream, concat)
{
    slang::token_stream ts1{std::vector<slang::token>{
      slang::token{"a", {1, 1}, slang::token_type::identifier},
      slang::token{"b", {1, 3}, slang::token_type::identifier}}};

    slang::token_stream ts2{std::vector<slang::token>{
      slang::token{"c", {1, 5}, slang::token_type::identifier},
      slang::token{"d", {1, 7}, slang::token_type::identifier}}};

    auto merged = ts1.concat(ts2);
    EXPECT_EQ(ts1.size(), 2);
    EXPECT_EQ(ts2.size(), 2);
    EXPECT_EQ(merged.size(), 4);
    EXPECT_EQ(merged[0].s, "a");
    EXPECT_EQ(merged[1].s, "b");
    EXPECT_EQ(merged[2].s, "c");
    EXPECT_EQ(merged[3].s, "d");
}

TEST(token_stream, cow_push_back)
{
    slang::token_stream ts;
    ts.push_back(slang::token{"x", {1, 1}, slang::token_type::identifier});

    slang::token_stream shared_copy = ts;
    EXPECT_EQ(ts.size(), 1);
    EXPECT_EQ(shared_copy.size(), 1);

    // Modifying ts should not alter shared_copy.
    ts.push_back(slang::token{"y", {1, 3}, slang::token_type::identifier});
    EXPECT_EQ(ts.size(), 2);
    EXPECT_EQ(shared_copy.size(), 1);
}

TEST(token_stream, iteration)
{
    slang::token_stream ts{
      slang::lexer::lex(
        "a b c")};

    std::vector<std::string> names;
    for(const auto& tok: ts)
    {
        names.push_back(tok.s);
    }

    ASSERT_EQ(names.size(), 3);
    EXPECT_EQ(names[0], "a");
    EXPECT_EQ(names[1], "b");
    EXPECT_EQ(names[2], "c");
}

TEST(token_stream, serialization)
{
    slang::token_stream ts{
      slang::lexer::lex("let val = 123;")};

    slang::memory_write_archive write_ar{true};
    write_ar & ts;

    slang::memory_read_archive read_ar{write_ar.get_buffer(), true};
    slang::token_stream deserialized;
    read_ar & deserialized;

    ASSERT_EQ(ts.size(), deserialized.size());
    for(std::size_t i = 0; i < ts.size(); ++i)
    {
        EXPECT_EQ(ts[i].s, deserialized[i].s);
        EXPECT_EQ(ts[i].type, deserialized[i].type);
        EXPECT_EQ(ts[i].location, deserialized[i].location);
    }
}

}    // namespace
