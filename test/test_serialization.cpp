/**
 * slang - a simple scripting language.
 *
 * Binary serialization tests.
 *
 * \author Felix Lubbe
 * \copyright Copyright (c) 2024
 * \license Distributed under the MIT software license (see accompanying LICENSE.txt).
 */

#include "archives/archive.h"
#include "archives/file.h"

#include <fmt/core.h>

#include <gtest/gtest.h>

namespace
{

std::vector<std::byte> to_little_endian(std::uint16_t i)
{
    return {
      static_cast<std::byte>(i & 0xff),
      static_cast<std::byte>((i >> 8) & 0xff)};
}

std::vector<std::byte> to_little_endian(std::int16_t i)
{
    return to_little_endian(*reinterpret_cast<std::uint16_t*>(&i));
}

std::vector<std::byte> to_little_endian(std::uint32_t i)
{
    return {
      static_cast<std::byte>(i & 0xff),
      static_cast<std::byte>((i >> 8) & 0xff),
      static_cast<std::byte>((i >> 16) & 0xff),
      static_cast<std::byte>((i >> 24) & 0xff)};
}

std::vector<std::byte> to_little_endian(std::int32_t i)
{
    return to_little_endian(*reinterpret_cast<std::uint32_t*>(&i));
}

std::vector<std::byte> to_little_endian(std::uint64_t i)
{
    return {
      static_cast<std::byte>(i & 0xff),
      static_cast<std::byte>((i >> 8) & 0xff),
      static_cast<std::byte>((i >> 16) & 0xff),
      static_cast<std::byte>((i >> 24) & 0xff),
      static_cast<std::byte>((i >> 32) & 0xff),
      static_cast<std::byte>((i >> 40) & 0xff),
      static_cast<std::byte>((i >> 48) & 0xff),
      static_cast<std::byte>((i >> 56) & 0xff)};
}

std::vector<std::byte> to_little_endian(std::int64_t i)
{
    return to_little_endian(*reinterpret_cast<std::uint64_t*>(&i));
}

std::vector<std::byte> to_little_endian(float f)
{
    static_assert(sizeof(float) == sizeof(std::uint32_t), "Float has to be the same size as std::uint32_t.");
    uint32_t i = *reinterpret_cast<std::uint32_t*>(&f);
    return to_little_endian(i);
}

std::vector<std::byte> to_little_endian(double d)
{
    uint64_t i = *reinterpret_cast<std::uint64_t*>(&d);
    static_assert(sizeof(double) == sizeof(std::uint64_t), "Double has to be the same size as std::uint64_t.");
    return to_little_endian(i);
}

std::vector<std::byte> to_big_endian(std::uint16_t i)
{
    return {
      static_cast<std::byte>((i >> 8) & 0xff),
      static_cast<std::byte>(i & 0xff)};
}

std::vector<std::byte> to_big_endian(std::int16_t i)
{
    return to_big_endian(*reinterpret_cast<std::uint16_t*>(&i));
}

std::vector<std::byte> to_big_endian(std::uint32_t i)
{
    return {
      static_cast<std::byte>((i >> 24) & 0xff),
      static_cast<std::byte>((i >> 16) & 0xff),
      static_cast<std::byte>((i >> 8) & 0xff),
      static_cast<std::byte>(i & 0xff)};
}

std::vector<std::byte> to_big_endian(std::int32_t i)
{
    return to_big_endian(*reinterpret_cast<std::uint32_t*>(&i));
}

std::vector<std::byte> to_big_endian(std::uint64_t i)
{
    return {
      static_cast<std::byte>((i >> 56) & 0xff),
      static_cast<std::byte>((i >> 48) & 0xff),
      static_cast<std::byte>((i >> 40) & 0xff),
      static_cast<std::byte>((i >> 32) & 0xff),
      static_cast<std::byte>((i >> 24) & 0xff),
      static_cast<std::byte>((i >> 16) & 0xff),
      static_cast<std::byte>((i >> 8) & 0xff),
      static_cast<std::byte>(i & 0xff)};
}

std::vector<std::byte> to_big_endian(std::int64_t i)
{
    return to_big_endian(*reinterpret_cast<std::uint64_t*>(&i));
}

std::vector<std::byte> to_big_endian(float f)
{
    static_assert(sizeof(float) == sizeof(std::uint32_t), "Float has to be the same size as std::uint32_t.");
    uint32_t i = *reinterpret_cast<std::uint32_t*>(&f);
    return to_big_endian(i);
}

std::vector<std::byte> to_big_endian(double d)
{
    uint64_t i = *reinterpret_cast<std::uint64_t*>(&d);
    static_assert(sizeof(double) == sizeof(std::uint64_t), "Double has to be the same size as std::uint64_t.");
    return to_big_endian(i);
}

TEST(serialization, file_read_write)
{
    {
        slang::file_write_archive ar("test.bin");

        EXPECT_EQ(ar.is_persistent(), true);
        EXPECT_EQ(ar.is_reading(), false);
        EXPECT_EQ(ar.is_writing(), true);

        std::int8_t i = 'a';
        std::string s = "Hello, World!";

        ar & i;
        EXPECT_EQ(ar.tell(), 1);

        ar & s;
        EXPECT_EQ(ar.tell(), 15);

        std::vector<std::string> v{"World!", "Hello, "};
        ar & v;
    }
    {
        slang::file_read_archive ar("test.bin");

        EXPECT_EQ(ar.is_persistent(), true);
        EXPECT_EQ(ar.is_reading(), true);
        EXPECT_EQ(ar.is_writing(), false);

        std::int8_t i;
        std::string s;

        ar & i;
        EXPECT_EQ(ar.tell(), 1);

        ar & s;
        EXPECT_EQ(ar.tell(), 15);

        EXPECT_EQ(i, 'a');
        EXPECT_EQ(s, "Hello, World!");

        std::vector<std::string> v;
        ar & v;
        ASSERT_EQ(v.size(), 2);
        EXPECT_EQ(v[0], "World!");
        EXPECT_EQ(v[1], "Hello, ");
    }
}

}    // namespace