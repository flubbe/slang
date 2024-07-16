/**
 * slang - a simple scripting language.
 *
 * Custom vector class tests.
 *
 * \author Felix Lubbe
 * \copyright Copyright (c) 2024
 * \license Distributed under the MIT software license (see accompanying LICENSE.txt).
 */

#include <string>

#include <gtest/gtest.h>

#include <fmt/core.h>

#include "vector.h"

namespace si = slang::interpreter;

namespace
{

struct S
{
    int a;
    float b;
    char c;
};

static_assert(sizeof(si::fixed_vector<char>) == sizeof(void*));
static_assert(sizeof(si::fixed_vector<short>) == sizeof(void*));
static_assert(sizeof(si::fixed_vector<int>) == sizeof(void*));
static_assert(sizeof(si::fixed_vector<long>) == sizeof(void*));
static_assert(sizeof(si::fixed_vector<float>) == sizeof(void*));
static_assert(sizeof(si::fixed_vector<double>) == sizeof(void*));
static_assert(sizeof(si::fixed_vector<S>) == sizeof(void*));

TEST(fixed_vector, create)
{
    si::fixed_vector<int> v1;
    si::fixed_vector<char> v2{0, 1, 2, 3};
    si::fixed_vector<long> v3{0, 1, 2, 3, 4, 5, 6, 7, 8, 9};

    EXPECT_EQ(v1.size(), 0);
    ASSERT_EQ(v2.size(), 4);
    ASSERT_EQ(v3.size(), 10);

    for(std::size_t i = 0; i < v2.size(); ++i)
    {
        EXPECT_EQ(v2[i], i);
    }
    for(std::size_t i = 0; i < v3.size(); ++i)
    {
        EXPECT_EQ(v3[i], i);
    }
}

TEST(fixed_vector, empty_vector)
{
    si::fixed_vector<S> v{0};
    EXPECT_EQ(v.size(), 0);
}

static std::size_t constructor_count = 0;
static std::size_t destructor_count = 0;
TEST(fixed_vector, construct)
{
    struct S
    {
        int i = 0;
        char c = 1;
        short d = 2;

        S()
        {
            ++constructor_count;
        }
        ~S()
        {
            ++destructor_count;
        }
    };

    {
        si::fixed_vector<S> v{3};
    }
    EXPECT_EQ(constructor_count, 3);
    EXPECT_EQ(destructor_count, 3);
}

TEST(fixed_vector, begin_end)
{
    si::fixed_vector<long> v{0, 1, 2, 3, 4, 5, 6, 7, 8, 9};

    EXPECT_EQ(v.end() - v.begin(), v.size());
    EXPECT_EQ(v.cend() - v.cbegin(), v.size());

    long ctr = 0;
    for(auto it = v.begin(); it != v.end(); ++it)
    {
        EXPECT_EQ(*it, ctr);
        ++ctr;
    }

    ctr = 0;
    for(auto it = v.cbegin(); it != v.cend(); ++it)
    {
        EXPECT_EQ(*it, ctr);
        ++ctr;
    }
}

TEST(fixed_vector, range_based_for)
{
    si::fixed_vector<int> v1;
    si::fixed_vector<char> v2{0, 1, 2, 3};
    si::fixed_vector<long> v3{0, 1, 2, 3, 4, 5, 6, 7, 8, 9};

    // test range-based for loops.
    std::size_t ctr = 0;
    for(auto it: v3)
    {
        EXPECT_EQ(it, ctr);
        ++ctr;
    }

    ctr = 0;
    for(auto& it: v3)
    {
        EXPECT_EQ(it, ctr);
        ++ctr;
    }

    ctr = 0;
    for(const auto& it: v3)
    {
        EXPECT_EQ(it, ctr);
        ++ctr;
    }

    {
        struct S
        {
            int i = 0;
            char c = 1;
            short d = 2;
        };

        si::fixed_vector<S> v{3};
        for(auto& s: v)
        {
            EXPECT_EQ(s.i, 0);
            EXPECT_EQ(s.c, 1);
            EXPECT_EQ(s.d, 2);

            s.i = -1;
            s.c = -2;
            s.d = -3;
        }
        for(auto& s: v)
        {
            EXPECT_EQ(s.i, -1);
            EXPECT_EQ(s.c, -2);
            EXPECT_EQ(s.d, -3);
        }
    }
}

TEST(fixed_vector, assignment)
{
    si::fixed_vector<int> v1 = {1, 2, -2, -1};
    si::fixed_vector<int> v2{v1};
    si::fixed_vector<int> v3{std::move(v1)};

    EXPECT_EQ(v1.size(), 0);
    EXPECT_EQ(v1.capacity(), 0);
    EXPECT_EQ(v1.max_size(), 0);
    EXPECT_TRUE(v1.empty());

    EXPECT_THROW(v1.at(0), std::out_of_range);

    ASSERT_EQ(v2.size(), 4);
    EXPECT_EQ(v2.capacity(), 4);
    EXPECT_EQ(v2.max_size(), 4);
    EXPECT_FALSE(v2.empty());

    EXPECT_EQ(v2.at(0), 1);
    EXPECT_EQ(v2.at(1), 2);
    EXPECT_EQ(v2.at(2), -2);
    EXPECT_EQ(v2.at(3), -1);
    EXPECT_THROW(v2.at(4), std::out_of_range);

    ASSERT_EQ(v3.size(), 4);
    EXPECT_EQ(v3.capacity(), 4);
    EXPECT_EQ(v3.max_size(), 4);
    EXPECT_FALSE(v3.empty());

    EXPECT_EQ(v3.at(0), 1);
    EXPECT_EQ(v3.at(1), 2);
    EXPECT_EQ(v3.at(2), -2);
    EXPECT_EQ(v3.at(3), -1);
    EXPECT_THROW(v3.at(4), std::out_of_range);

    si::fixed_vector<int> v4, v5;
    v4 = v2;
    v5 = std::move(v3);

    ASSERT_EQ(v4.size(), 4);
    EXPECT_EQ(v4.capacity(), 4);
    EXPECT_EQ(v4.max_size(), 4);
    EXPECT_FALSE(v4.empty());

    EXPECT_EQ(v4.at(0), 1);
    EXPECT_EQ(v4.at(1), 2);
    EXPECT_EQ(v4.at(2), -2);
    EXPECT_EQ(v4.at(3), -1);
    EXPECT_THROW(v4.at(4), std::out_of_range);

    ASSERT_EQ(v3.size(), 0);
    EXPECT_EQ(v3.capacity(), 0);
    EXPECT_EQ(v3.max_size(), 0);
    EXPECT_TRUE(v3.empty());

    ASSERT_EQ(v5.size(), 4);
    EXPECT_EQ(v5.capacity(), 4);
    EXPECT_EQ(v5.max_size(), 4);
    EXPECT_FALSE(v5.empty());

    EXPECT_EQ(v5.at(0), 1);
    EXPECT_EQ(v5.at(1), 2);
    EXPECT_EQ(v5.at(2), -2);
    EXPECT_EQ(v5.at(3), -1);
    EXPECT_THROW(v5.at(4), std::out_of_range);
}

TEST(fixed_vector, access)
{
    si::fixed_vector<float> v = {1, 2, -2, -1};

    EXPECT_NEAR(v.at(0), 1.f, 1e-6);
    EXPECT_NEAR(v.at(1), 2.f, 1e-6);
    EXPECT_NEAR(v.at(2), -2.f, 1e-6);
    EXPECT_NEAR(v.at(3), -1.f, 1e-6);

    EXPECT_NEAR(v[0], 1.f, 1e-6);
    EXPECT_NEAR(v[1], 2.f, 1e-6);
    EXPECT_NEAR(v[2], -2.f, 1e-6);
    EXPECT_NEAR(v[3], -1.f, 1e-6);

    v.at(0) = 2;
    v[3] = -2;

    EXPECT_NEAR(v[0], 2, 1e-6);
    EXPECT_NEAR(v.at(3), -2, 1e-6);

    EXPECT_NEAR(v.front(), 2, 1e-6);
    EXPECT_NEAR(v.back(), -2, 1e-6);

    v.front() = 0;
    v.back() = 5;

    EXPECT_NEAR(v.front(), 0, 1e-6);
    EXPECT_NEAR(v.back(), 5, 1e-6);
}

TEST(fixed_vector, strings)
{
    si::fixed_vector<std::string> sv = {
      "Hello", "World", ""};

    ASSERT_EQ(sv.size(), 3);
    EXPECT_EQ(sv.max_size(), 3);
    EXPECT_EQ(sv.capacity(), 3);

    EXPECT_EQ(sv[0], "Hello");
    EXPECT_EQ(sv.at(1), "World");
    EXPECT_EQ(sv[2].length(), 0);

    sv[1] = "Sun";

    ASSERT_EQ(sv.size(), 3);
    EXPECT_EQ(sv[1], "Sun");
}

TEST(fixed_vector, nested_vectors)
{
    si::fixed_vector<si::fixed_vector<int>> v1 = {
      {1, 2},
      {3}};

    ASSERT_EQ(v1.size(), 2);
    ASSERT_EQ(v1[0].size(), 2);
    ASSERT_EQ(v1[1].size(), 1);

    EXPECT_EQ(v1[0][0], 1);
    EXPECT_EQ(v1[0][1], 2);
    EXPECT_EQ(v1[1][0], 3);

    si::fixed_vector<si::fixed_vector<std::string>> v2 = {
      {"a", "b"},
      {"c"}};

    ASSERT_EQ(v2.size(), 2);
    ASSERT_EQ(v2[0].size(), 2);
    ASSERT_EQ(v2[1].size(), 1);

    EXPECT_EQ(v2[0][0], "a");
    EXPECT_EQ(v2[0][1], "b");
    EXPECT_EQ(v2[1][0], "c");
}

TEST(fixed_vector, size_after_type_cast)
{
    si::fixed_vector<int> v_int = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9};

    auto v_char_ptr = reinterpret_cast<si::fixed_vector<char>*>(&v_int);
    auto v_short_ptr = reinterpret_cast<si::fixed_vector<short>*>(&v_int);
    auto v_long_ptr = reinterpret_cast<si::fixed_vector<long>*>(&v_int);
    auto v_float_ptr = reinterpret_cast<si::fixed_vector<float>*>(&v_int);
    auto v_double_ptr = reinterpret_cast<si::fixed_vector<double>*>(&v_int);
    auto v_S_ptr = reinterpret_cast<si::fixed_vector<S>*>(&v_int);

    EXPECT_EQ(v_int.size(), 10);
    EXPECT_EQ(v_char_ptr->size(), v_int.size());
    EXPECT_EQ(v_short_ptr->size(), v_int.size());
    EXPECT_EQ(v_long_ptr->size(), v_int.size());
    EXPECT_EQ(v_double_ptr->size(), v_int.size());
    EXPECT_EQ(v_S_ptr->size(), v_int.size());
}

}    // namespace
