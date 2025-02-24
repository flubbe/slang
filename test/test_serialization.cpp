/**
 * slang - a simple scripting language.
 *
 * Serialization tests.
 *
 * \author Felix Lubbe
 * \copyright Copyright (c) 2024
 * \license Distributed under the MIT software license (see accompanying LICENSE.txt).
 */

#include <cstring>

#include <fmt/core.h>
#include <gtest/gtest.h>

#include "archives/archive.h"
#include "archives/file.h"

// NOTE There a lot of random/magic numbers in the tests.
// NOLINTBEGIN(readability-magic-numbers)

namespace
{

std::vector<std::uint8_t> to_little_endian(std::uint16_t i)
{
    return {
      static_cast<std::uint8_t>(i & 0xff),
      static_cast<std::uint8_t>((i >> 8) & 0xff)};
}

[[maybe_unused]]
std::vector<std::uint8_t> to_little_endian(std::int16_t i)
{
    return to_little_endian(*reinterpret_cast<std::uint16_t*>(&i));    // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast)
}

std::vector<std::uint8_t> to_little_endian(std::uint32_t i)
{
    return {
      static_cast<std::uint8_t>(i & 0xff),
      static_cast<std::uint8_t>((i >> 8) & 0xff),
      static_cast<std::uint8_t>((i >> 16) & 0xff),
      static_cast<std::uint8_t>((i >> 24) & 0xff)};
}

[[maybe_unused]]
std::vector<std::uint8_t> to_little_endian(std::int32_t i)
{
    return to_little_endian(*reinterpret_cast<std::uint32_t*>(&i));    // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast)
}

std::vector<std::uint8_t> to_little_endian(std::uint64_t i)
{
    return {
      static_cast<std::uint8_t>(i & 0xff),
      static_cast<std::uint8_t>((i >> 8) & 0xff),
      static_cast<std::uint8_t>((i >> 16) & 0xff),
      static_cast<std::uint8_t>((i >> 24) & 0xff),
      static_cast<std::uint8_t>((i >> 32) & 0xff),
      static_cast<std::uint8_t>((i >> 40) & 0xff),
      static_cast<std::uint8_t>((i >> 48) & 0xff),
      static_cast<std::uint8_t>((i >> 56) & 0xff)};
}

[[maybe_unused]]
std::vector<std::uint8_t> to_little_endian(std::int64_t i)
{
    return to_little_endian(*reinterpret_cast<std::uint64_t*>(&i));    // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast)
}

std::vector<std::uint8_t> to_little_endian(float f)
{
    static_assert(sizeof(float) == sizeof(std::uint32_t), "Float has to be the same size as std::uint32_t.");
    uint32_t i{0};
    std::memcpy(&i, &f, sizeof(i));
    return to_little_endian(i);
}

std::vector<std::uint8_t> to_little_endian(double d)
{
    static_assert(sizeof(double) == sizeof(std::uint64_t), "Double has to be the same size as std::uint64_t.");
    uint64_t i{0};
    std::memcpy(&i, &d, sizeof(i));
    return to_little_endian(i);
}

std::vector<std::uint8_t> to_big_endian(std::uint16_t i)
{
    return {
      static_cast<std::uint8_t>((i >> 8) & 0xff),
      static_cast<std::uint8_t>(i & 0xff)};
}

[[maybe_unused]]
std::vector<std::uint8_t> to_big_endian(std::int16_t i)
{
    return to_big_endian(*reinterpret_cast<std::uint16_t*>(&i));    // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast)
}

std::vector<std::uint8_t> to_big_endian(std::uint32_t i)
{
    return {
      static_cast<std::uint8_t>((i >> 24) & 0xff),
      static_cast<std::uint8_t>((i >> 16) & 0xff),
      static_cast<std::uint8_t>((i >> 8) & 0xff),
      static_cast<std::uint8_t>(i & 0xff)};
}

[[maybe_unused]]
std::vector<std::uint8_t> to_big_endian(std::int32_t i)
{
    return to_big_endian(*reinterpret_cast<std::uint32_t*>(&i));    // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast)
}

std::vector<std::uint8_t> to_big_endian(std::uint64_t i)
{
    return {
      static_cast<std::uint8_t>((i >> 56) & 0xff),
      static_cast<std::uint8_t>((i >> 48) & 0xff),
      static_cast<std::uint8_t>((i >> 40) & 0xff),
      static_cast<std::uint8_t>((i >> 32) & 0xff),
      static_cast<std::uint8_t>((i >> 24) & 0xff),
      static_cast<std::uint8_t>((i >> 16) & 0xff),
      static_cast<std::uint8_t>((i >> 8) & 0xff),
      static_cast<std::uint8_t>(i & 0xff)};
}

[[maybe_unused]]
std::vector<std::uint8_t> to_big_endian(std::int64_t i)
{
    return to_big_endian(*reinterpret_cast<std::uint64_t*>(&i));    // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast)
}

std::vector<std::uint8_t> to_big_endian(float f)
{
    static_assert(sizeof(float) == sizeof(std::uint32_t), "Float has to be the same size as std::uint32_t.");
    uint32_t i{0};
    std::memcpy(&i, &f, sizeof(i));
    return to_big_endian(i);
}

std::vector<std::uint8_t> to_big_endian(double d)
{
    static_assert(sizeof(double) == sizeof(std::uint64_t), "Double has to be the same size as std::uint64_t.");
    uint64_t i{0};
    std::memcpy(&i, &d, sizeof(d));
    return to_big_endian(i);
}

TEST(serialization, big_endian_file_archive)
{
    {
        slang::file_write_archive ar{"big_endian.bin", slang::endian::big};

        EXPECT_EQ(ar.get_target_byte_order(), slang::endian::big);
        EXPECT_EQ(ar.is_persistent(), true);
        EXPECT_EQ(ar.is_reading(), false);
        EXPECT_EQ(ar.is_writing(), true);

        bool bo = true;
        std::uint8_t by = 0x01;
        std::uint16_t w = 0x1234;
        std::uint32_t dw = 0x12345678;
        float f = 1.234f;
        double d = -123.4561234;
        long i = 0x123456789012345;
        slang::vle_int vi{i};

        ar & bo /* 1 byte */
          & by  /* 1 byte */
          & w   /* 2 bytes */
          & dw  /* 4 bytes */
          & f   /* 4 bytes */
          & d   /* 8 bytes */
          & vi; /* 9 bytes */

        ASSERT_EQ(ar.tell(), 29);
    }
    {
        std::ifstream file{"big_endian.bin", std::ifstream::binary};
        ASSERT_TRUE(file);

        file.seekg(0, std::ios::end);
        std::size_t offs = file.tellg();
        file.seekg(0, std::ios::beg);
        offs -= file.tellg();
        ASSERT_EQ(offs, 29);

        std::vector<std::uint8_t> buf{std::istreambuf_iterator<char>(file), {}};
        ASSERT_EQ(buf.size(), 29);

        bool bo = true;
        std::uint8_t by = 0x01;
        std::uint16_t w = 0x1234;
        std::uint32_t dw = 0x12345678;
        float f = 1.234f;
        double d = -123.4561234;

        EXPECT_EQ(buf[0], bo);
        EXPECT_EQ(buf[1], by);

        auto mem = to_big_endian(w);
        for(std::size_t i = 0; i < 2; ++i)
        {
            EXPECT_EQ(buf[2 + i], mem[i]);
        }

        mem = to_big_endian(dw);
        for(std::size_t i = 0; i < 4; ++i)
        {
            EXPECT_EQ(buf[4 + i], mem[i]);
        }

        mem = to_big_endian(f);
        for(std::size_t i = 0; i < 4; ++i)
        {
            EXPECT_EQ(buf[8 + i], mem[i]);
        }

        mem = to_big_endian(d);
        for(std::size_t i = 0; i < 8; ++i)
        {
            EXPECT_EQ(buf[12 + i], mem[i]);
        }
    }
    {
        slang::file_read_archive ar{"big_endian.bin", slang::endian::big};

        EXPECT_EQ(ar.get_target_byte_order(), slang::endian::big);
        EXPECT_EQ(ar.is_persistent(), true);
        EXPECT_EQ(ar.is_reading(), true);
        EXPECT_EQ(ar.is_writing(), false);

        bool bo{false};
        std::uint8_t by{0};
        std::uint16_t w{0};
        std::uint32_t dw{0};
        float f{0.f};
        double d{0.0};

        ar & bo & by & w & dw & f & d;

        ASSERT_EQ(ar.tell(), 20);

        EXPECT_EQ(bo, true);
        EXPECT_EQ(by, 0x01);
        EXPECT_EQ(w, 0x1234);
        EXPECT_EQ(dw, 0x12345678);
        EXPECT_EQ(f, 1.234f);
        EXPECT_EQ(d, -123.4561234);
    }
}

TEST(serialization, little_endian_file_archive)
{
    {
        slang::file_write_archive ar{"little_endian.bin", slang::endian::little};

        EXPECT_EQ(ar.get_target_byte_order(), slang::endian::little);
        EXPECT_EQ(ar.is_persistent(), true);
        EXPECT_EQ(ar.is_reading(), false);
        EXPECT_EQ(ar.is_writing(), true);

        bool bo = true;
        std::uint8_t by = 0x01;
        std::uint16_t w = 0x1234;
        std::uint32_t dw = 0x12345678;
        float f = 1.234f;
        double d = -123.4561234;

        ar & bo & by & w & dw & f & d;

        ASSERT_EQ(ar.tell(), 20);
    }
    {
        std::ifstream file{"little_endian.bin", std::ifstream::binary};
        ASSERT_TRUE(file);

        file.seekg(0, std::ios::end);
        std::size_t offs = file.tellg();
        file.seekg(0, std::ios::beg);
        offs -= file.tellg();
        ASSERT_EQ(offs, 20);

        std::vector<std::uint8_t> buf{std::istreambuf_iterator<char>(file), {}};
        ASSERT_EQ(buf.size(), 20);

        bool bo = true;
        std::uint8_t by = 0x01;
        std::uint16_t w = 0x1234;
        std::uint32_t dw = 0x12345678;
        float f = 1.234f;
        double d = -123.4561234;

        EXPECT_EQ(buf[0], bo);
        EXPECT_EQ(buf[1], by);

        auto mem = to_little_endian(w);
        for(std::size_t i = 0; i < 2; ++i)
        {
            EXPECT_EQ(buf[2 + i], mem[i]);
        }

        mem = to_little_endian(dw);
        for(std::size_t i = 0; i < 4; ++i)
        {
            EXPECT_EQ(buf[4 + i], mem[i]);
        }

        mem = to_little_endian(f);
        for(std::size_t i = 0; i < 4; ++i)
        {
            EXPECT_EQ(buf[8 + i], mem[i]);
        }

        mem = to_little_endian(d);
        for(std::size_t i = 0; i < 8; ++i)
        {
            EXPECT_EQ(buf[12 + i], mem[i]);
        }
    }
    {
        slang::file_read_archive ar{"little_endian.bin", slang::endian::little};

        EXPECT_EQ(ar.get_target_byte_order(), slang::endian::little);
        EXPECT_EQ(ar.is_persistent(), true);
        EXPECT_EQ(ar.is_reading(), true);
        EXPECT_EQ(ar.is_writing(), false);

        bool bo{false};
        std::uint8_t by{0};
        std::uint16_t w{0};
        std::uint32_t dw{0};
        float f{0.f};
        double d{0.0};

        ar & bo & by & w & dw & f & d;

        ASSERT_EQ(ar.tell(), 20);

        EXPECT_EQ(bo, true);
        EXPECT_EQ(by, 0x01);
        EXPECT_EQ(w, 0x1234);
        EXPECT_EQ(dw, 0x12345678);
        EXPECT_EQ(f, 1.234f);
        EXPECT_EQ(d, -123.4561234);
    }
}

TEST(serialization, strings)
{
    {
        slang::file_write_archive ar{"little_endian.bin", slang::endian::little};
        std::string s1 = "Hello, ";
        std::string s2 = "World!";
        ar & s1 & s2;
    }
    {
        slang::file_read_archive ar{"little_endian.bin", slang::endian::little};
        std::string s1 = "Hello, ";
        std::string s2 = "World!";
        ar & s1 & s2;

        EXPECT_EQ(s1, "Hello, ");
        EXPECT_EQ(s2, "World!");
    }
    {
        slang::file_write_archive ar{"big_endian.bin", slang::endian::big};
        std::string s1 = "Hello, ";
        std::string s2 = "World!";
        ar & s1 & s2;
    }
    {
        slang::file_read_archive ar{"big_endian.bin", slang::endian::big};
        std::string s1 = "Hello, ";
        std::string s2 = "World!";
        ar & s1 & s2;

        EXPECT_EQ(s1, "Hello, ");
        EXPECT_EQ(s2, "World!");
    }
}

TEST(serialization, vectors)
{
    {
        slang::file_write_archive ar{"little_endian.bin", slang::endian::little};
        std::vector<std::string> v = {"Hello, ", "World!"};
        ar & v;
    }
    {
        slang::file_read_archive ar{"little_endian.bin", slang::endian::little};
        std::vector<std::string> v;

        ar & v;

        ASSERT_EQ(v.size(), 2);
        EXPECT_EQ(v[0], "Hello, ");
        EXPECT_EQ(v[1], "World!");
    }
    {
        slang::file_write_archive ar{"big_endian.bin", slang::endian::big};
        std::vector<std::string> v = {"Hello, ", "World!"};
        ar & v;
    }
    {
        slang::file_read_archive ar{"big_endian.bin", slang::endian::big};
        std::vector<std::string> v;

        ar & v;

        ASSERT_EQ(v.size(), 2);
        EXPECT_EQ(v[0], "Hello, ");
        EXPECT_EQ(v[1], "World!");
    }
}

}    // namespace

// NOLINTEND(readability-magic-numbers)
