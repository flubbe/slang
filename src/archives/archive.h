/**
 * slang - a simple scripting language.
 *
 * archive and serialization support.
 *
 * \author Felix Lubbe
 * \copyright Copyright (c) 2024
 * \license Distributed under the MIT software license (see accompanying LICENSE.txt).
 */

#pragma once

#include <exception>
#include <cstddef>
#include <cstdint>
#include <string>

namespace slang
{

/** The endianness of the system. Only 'big' and 'little' are supported. */
enum class endian
{
#if defined(_MSC_VER) && !defined(__clang__)
    little = 0,
    big = 1,
    native = little
#else
    little = __ORDER_LITTLE_ENDIAN__,
    big = __ORDER_BIG_ENDIAN__,
    native = __BYTE_ORDER__
#endif
};

static_assert(
  endian::native == endian::little || endian::native == endian::big,
  "Only little endian or big endian CPU architectures are supported.");

static_assert(
  sizeof(bool) == 1, "Only 1-byte bools are supported.");

static_assert(
  sizeof(char) == 1, "Only 1-byte chars are supported.");

static_assert(
  sizeof(float) == 4, "Only 4-byte floats are supported.");

static_assert(
  sizeof(double) == 8, "Only 8-byte doubles are supported.");

static_assert(
  sizeof(std::size_t) == 8, "Only 8-byte std::size_t's are supported.");

/** A serialization error. */
class serialization_error : public std::runtime_error
{
public:
    /**
     * Construct a `serialization_error`.
     *
     * @param message The error message.
     */
    serialization_error(const std::string& message)
    : std::runtime_error{message}
    {
    }
};

/** A variable-length-encoded integer. */
struct vle_int
{
    /** The integer. */
    std::int64_t i;

    /** Default constructors. */
    vle_int() = default;
    vle_int(const vle_int&) = default;
    vle_int(vle_int&&) = default;

    /** Default assignments. */
    vle_int& operator=(const vle_int&) = default;
    vle_int& operator=(vle_int&&) = default;

    /**
     * Construct a variable length integer instance.
     * Just initializes the stored integer.
     *
     * @param i The integer.
     */
    vle_int(std::int64_t i)
    : i{i}
    {
    }
};

/**
 * Serialize a variable-length-encoded integer.
 *
 * @param ar The archive to use for serialization.
 * @param i The integer to be serialized.
 */
class archive& operator&(class archive& ar, vle_int& i);

/** An abstract archive for byte-order independent serialization. */
class archive
{
protected:
    /** Whether this is a read archive. */
    bool read;

    /** Whether this is a write archive. */
    bool write;

    /** Whether this is a persistent archive. */
    bool persistent;

    /**
     * Serialize raw bytes.
     *
     * @param buffer The raw byte buffer.
     * @param c The buffer size, in bytes.
     */
    virtual void serialize_bytes(std::byte* buffer, std::size_t c) = 0;

public:
    /** Defaulted and deleted constructors. */
    archive() = delete;
    archive(const archive&) = default;
    archive(archive&&) = default;

    /** Default destructor. */
    virtual ~archive() = default;

    /**
     * Set up an archive.
     *
     * @param read Whether this is a read archive.
     * @param write Whether this is a write archive.
     * @param persistent Whether this is a persistent archive.
     */
    archive(bool read, bool write, bool persistent)
    : read{read}
    , write{write}
    , persistent{persistent}
    {
        if(read && write)
        {
            throw std::runtime_error("An archive cannot be both readable and writable.");
        }
    }

    /** Default assignments. */
    archive& operator=(const archive&) = default;
    archive& operator=(archive&&) = default;

    /** Get the position in the archive. */
    virtual std::size_t tell() = 0;

    /**
     * Seek to a position in the archive.
     *
     * @param pos The position to seek to.
     * @returns Returns the new position.
     */
    virtual std::size_t seek(std::size_t pos) = 0;

    /** Get the size of the archive. */
    virtual std::size_t size() = 0;

    /** Returns whether this is a read archive. */
    bool is_reading() const
    {
        return read;
    }

    /** Returns whether this is a write archive. */
    bool is_writing() const
    {
        return write;
    }

    /** Return whether this is a persistent archive. */
    bool is_persistent() const
    {
        return persistent;
    }

    /**
     * Serialize byte buffers to little endian. That is, if `c>1`,
     * the buffer is reversed on big endian architectures.
     *
     * @param buffer The raw byte buffer.
     * @param c The buffer size, in bytes.
     */
    virtual void serialize(std::byte* buffer, std::size_t c)
    {
        if(!is_persistent())
        {
            // in-memory archive.
            serialize_bytes(buffer, c);
        }
        else
        {
            // persistent archive.
            if constexpr(endian::native == endian::little)
            {
                serialize_bytes(buffer, c);
            }
            else
            {
                for(std::size_t i = c - 1; i >= 0; --i)
                {
                    serialize_bytes(&buffer[i], 1);
                }
            }
        }
    }

    /** Serializa a bool. */
    archive& operator&(bool& b)
    {
        serialize(reinterpret_cast<std::byte*>(&b), 1);
        return *this;
    }

    /** Serialize a char. */
    archive& operator&(char& c)
    {
        serialize(reinterpret_cast<std::byte*>(&c), 1);
        return *this;
    }

    /** Serialize a std::int8_t. */
    archive& operator&(std::int8_t& i)
    {
        serialize(reinterpret_cast<std::byte*>(&i), 1);
        return *this;
    }

    /** Serialize a std::uint8_t. */
    archive& operator&(std::uint8_t& i)
    {
        serialize(reinterpret_cast<std::byte*>(&i), 1);
        return *this;
    }

    /** Serialize a std::int16_t. */
    archive& operator&(std::int16_t& i)
    {
        serialize(reinterpret_cast<std::byte*>(&i), 2);
        return *this;
    }

    /** Serialize a std::uint16_t. */
    archive& operator&(std::uint16_t& i)
    {
        serialize(reinterpret_cast<std::byte*>(&i), 2);
        return *this;
    }

    /** Serialize a std::int32_t. */
    archive& operator&(std::int32_t& i)
    {
        serialize(reinterpret_cast<std::byte*>(&i), 4);
        return *this;
    }

    /** Serialize a std::uint32_t. */
    archive& operator&(std::uint32_t& i)
    {
        serialize(reinterpret_cast<std::byte*>(&i), 4);
        return *this;
    }

    /** Serialize a std::int64_t. */
    archive& operator&(std::int64_t& i)
    {
        serialize(reinterpret_cast<std::byte*>(&i), 8);
        return *this;
    }

    /** Serialize a std::int64_t. */
    archive& operator&(std::uint64_t& i)
    {
        serialize(reinterpret_cast<std::byte*>(&i), 8);
        return *this;
    }

    /** Serialize a std::size_t. */
    archive& operator&(std::size_t& i)
    {
        serialize(reinterpret_cast<std::byte*>(&i), 8);
        return *this;
    }

    /** Serialize a float. */
    archive& operator&(float& f)
    {
        serialize(reinterpret_cast<std::byte*>(&f), 4);
        return *this;
    }

    /** Serialize a double. */
    archive& operator&(double& d)
    {
        serialize(reinterpret_cast<std::byte*>(&d), 8);
        return *this;
    }

    /** Serialize a string. */
    archive& operator&(std::string& s)
    {
        vle_int len = s.length();
        (*this) & len;
        if(is_reading())
        {
            s.resize(len.i);
        }
        for(std::int64_t i = 0; i < len.i; ++i)
        {
            (*this) & s[i];
        }
        return *this;
    }
};

/*
 * Variable length integers.
 */

inline archive& operator&(archive& ar, vle_int& i)
{
    std::uint64_t v = std::abs(i.i);

    std::uint8_t b0 = (i.i < 0) ? 0x80 : 0x00;    // sign bit
    b0 |= (v >= 0x40) ? 0x40 : 0x00;              // continuation bit
    b0 |= (v & 0x3f);                             // data

    ar & b0;
    i.i = (b0 & 0x3f);

    if(b0 & 0x40)    // continuation bit
    {
        i.i <<= 6;
        v >>= 6;

        std::uint8_t b = (v >= 0x80 ? 0x80 : 0x00);    // continuation bit
        b |= (v & 0x7f);                               // data
        ar & b;

        // loop while continuation bit is set, but at most 7 times for a 64-bit integer.
        for(std::size_t j = 0; j < 7 && (b & 0x80); ++j)
        {
            v >>= 7;
            b = (v >= 0x80 ? 0x80 : 0x00) | (v & 0x7f);

            ar & b;

            i.i <<= 7;
            i.i |= b;
        }

        if(b & 0x80)
        {
            throw serialization_error("Inconsistent VLE integer encoding (continuation bit is set, exceeding maximum integer size).");
        }
    }

    if(b0 & 0x80)
    {
        i.i = -i.i;
    }

    return ar;
}

/*
 * std::vector serializer.
 */

/**
 * Serialize a templated std::vector.
 */
template<typename T>
archive& operator&(archive& ar, std::vector<T>& v)
{
    vle_int len = v.size();
    ar & len;
    if(ar.is_reading())
    {
        v.resize(len.i);
    }
    for(std::int64_t i = 0; i < len.i; ++i)
    {
        ar& v[i];
    }
    return ar;
}

}    // namespace slang