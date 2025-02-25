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

#include <algorithm>
#include <functional>
#include <limits>
#include <list>
#include <string>
#include <utility>
#include <vector>

namespace slang::utils
{

/**
 * Split a string at a delimiter.
 *
 * @param s The string to split.
 * @param delimiter The delimiter that separates the string's components.
 * @return A list of components of the original string.
 */
std::list<std::string> split(const std::string& s, const std::string& delimiter);

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

/**
 * Align a parameter according to the specified alignment.
 *
 * @note The alignment must be a power of 2.
 * @param alignment The alignment to use.
 * @param p The parameter to align.
 * @returns A number bigger or equal to `p`, satisfying the alignment requirement.
 */
template<typename T>
constexpr std::enable_if_t<!std::is_integral_v<T>, T> align(std::size_t alignment, T p)
{
    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
    return reinterpret_cast<T>((reinterpret_cast<uintptr_t>(p) + (alignment - 1)) & ~(alignment - 1));
}

/**
 * Align an integral parameter according to the specified alignment.
 *
 * @note The alignment must be a power of 2.
 * @param alignment The alignment to use.
 * @param p The parameter to align.
 * @returns A number bigger or equal to `p`, satisfying the alignment requirement.
 */
template<typename T>
constexpr std::enable_if_t<std::is_integral_v<T>, T> align(std::size_t alignment, T p)
{
    return static_cast<T>((static_cast<uintptr_t>(p) + (alignment - 1)) & ~(alignment - 1));
}

/*
 * Helpers for tuples.
 */

/** Check that all tuple types are the same. */
template<typename T, typename... Args>
struct all_same_type
{
    constexpr static bool value = std::is_same_v<std::tuple<T, Args...>,
                                                 std::tuple<Args..., T>>;
};

template<typename... Args>
struct all_same_type<std::tuple<Args...>> : all_same_type<Args...>
{
};

template<>
struct all_same_type<std::tuple<>>
{
    constexpr static bool value = true;
};

template<typename... Args>
constexpr bool all_same_type_v = all_same_type<Args...>::value;

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
T numeric_cast(S value)
{
    static_assert(std::is_integral_v<T>, "T must be an integral type.");
    static_assert(std::is_integral_v<S>, "S must be an integral type.");

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

}    // namespace slang::utils
