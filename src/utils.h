/**
 * slang - a simple scripting language.
 *
 * utility functions.
 *
 * \author Felix Lubbe
 * \copyright Copyright (c) 2025
 * \license Distributed under the MIT software license (see accompanying LICENSE.txt).
 */

#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>
#include <list>
#include <limits>
#include <stdexcept>
#include <string>
#include <tuple>
#include <type_traits>
#include <vector>

namespace slang::utils
{

/** Returns if the given character is an alphabetic character. */
inline bool is_alpha(char c)
{
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z');
}

/** Returns if the given character is one of the 10 decimal digits: `0123456789`. */
inline bool is_digit(char c)
{
    return c >= '0' && c <= '9';
}

/** Returns if the given character is an alphanumeric character. */
inline bool is_alnum(char c)
{
    return is_alpha(c) || is_digit(c);
}

/**
 * Split a string at a delimiter.
 *
 * @param s The string to split.
 * @param delimiter The delimiter that separates the string's components.
 * @return A vector of components of the original string.
 */
std::vector<std::string> split(const std::string& s, const std::string& delimiter);

/**
 * Join a vector of strings.
 *
 * @param v The vector of strings.
 * @param separator The seperator to use between the strings.
 * @return A string made of the vector's strings joined together and separated bythe given separator.
 */
std::string join(const std::vector<std::string>& v, const std::string& separator);

/**
 * Join a vector of strings.
 *
 * @tparam T Type parameter of the vector.
 * @param v The vector with elements of type T.
 * @param transform A transformation from T to std::string.
 * @param separator The seperator to use between the strings.
 * @return A string made of the vector's transformed elements joined together and separated bythe given separator.
 */
template<typename T>
std::string join(
  const std::vector<T>& v,    // NOLINT(bugprone-easily-swappable-parameters)
  std::function<std::string(const T&)> transform,
  const std::string& separator)
{
    if(v.empty())
    {
        return {};
    }

    if(v.size() == 1)
    {
        return transform(v[0]);
    }

    std::string res = transform(v[0]);
    for(auto it = std::next(v.begin()); it != v.end(); ++it)
    {
        res.append(separator);
        res.append(transform(*it));
    }
    return res;
}

/**
 * Replace all occurrences of a substring.
 *
 * @param str The string to operate on.
 * @param old_value The value to replace.
 * @param new_value The value to use as a replacement.
 */
inline void replace_all(
  std::string& str,
  const std::string& old_value,    // NOLINT(bugprone-easily-swappable-parameters)
  const std::string& new_value)
{
    size_t i = 0;
    while((i = str.find(old_value, i)) != std::string::npos)
    {
        str.replace(i, old_value.length(), new_value);
        i += new_value.length();
    }
}

/** Same as `std::false_type`, but taking a parameter argument. */
template<typename T>
struct false_type : public std::false_type
{
};

/**
 * Align a parameter according to the specified alignment.
 *
 * @note The alignment must be a power of 2.
 * @param alignment The alignment to use.
 * @param p The parameter to align.
 * @returns A number bigger or equal to `p`, satisfying the alignment requirement.
 */
template<typename T>
    requires(!std::is_integral_v<T>)
constexpr T align(std::size_t alignment, T p)
{
    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
    return reinterpret_cast<T>((reinterpret_cast<std::uintptr_t>(p) + (alignment - 1)) & ~(alignment - 1));
}

/**
 * Align an integral parameter according to the specified alignment.
 *
 * @note The alignment must be a power of 2.
 * @param alignment The alignment to use.
 * @param p The parameter to align.
 * @returns A number bigger or equal to `p`, satisfying the alignment requirement.
 */
template<std::integral T>
constexpr T align(std::size_t alignment, T p)
{
    return static_cast<T>((static_cast<std::uintptr_t>(p) + (alignment - 1)) & ~(alignment - 1));
}

/*
 * Helpers for smart pointers.
 */

template<typename T>
concept smart_ptr =
  requires { typename T::element_type; }
  && (std::is_same_v<T, std::shared_ptr<typename T::element_type>>
      || std::is_same_v<T, std::unique_ptr<typename T::element_type>>);

/*
 * Safe casting.
 */

/**
 * Safely cast a value to another type.
 *
 * @param value The value to cast.
 * @returns Returns the input value for the new type.
 * @throws Throws a `std::out_of_range` exception if the value does not fit into the target type.
 */
template<typename T, typename S>
    requires(std::is_integral_v<T> && std::is_integral_v<S>)
constexpr T numeric_cast(S value)
{
    if constexpr(std::is_signed_v<S> == std::is_signed_v<T>)
    {
        using ComparisonType = std::common_type_t<S, T>;
        if(static_cast<ComparisonType>(value) < static_cast<ComparisonType>(std::numeric_limits<T>::min())
           || static_cast<ComparisonType>(value) > static_cast<ComparisonType>(std::numeric_limits<T>::max()))
        {
            throw std::out_of_range("Value out of range of target type.");
        }
    }
    else if constexpr(std::is_signed_v<S> && !std::is_signed_v<T>)
    {
        if(value < 0
           || static_cast<std::make_unsigned_t<S>>(value) > std::numeric_limits<T>::max())
        {
            throw std::out_of_range("Value out of range of target type.");
        }
    }
    else
    {
        if(value > static_cast<std::make_unsigned_t<T>>(std::numeric_limits<T>::max()))
        {
            throw std::out_of_range("Value out of range of target type.");
        }
    }

    return static_cast<T>(value);
}

/**
 * Check if a `std::int64_t` value fits into the range of another signed integral type.
 *
 * @tparam T The type to check against.
 * @param v The value to check.
 * @returns Returns whether the value fits into the type.
 */
template<typename T>
    requires(std::is_integral_v<T> && std::is_signed_v<T>)
constexpr bool fits_in(std::int64_t v)
{
    return v >= std::numeric_limits<T>::min()
           && v <= std::numeric_limits<T>::max();
}

}    // namespace slang::utils
