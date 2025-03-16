/**
 * slang - a simple scripting language.
 *
 * token helpers.
 *
 * \author Felix Lubbe
 * \copyright Copyright (c) 2025
 * \license Distributed under the MIT software license (see accompanying LICENSE.txt).
 */

#include <fmt/core.h>

#include "token.h"
#include "utils.h"

namespace slang
{

archive& operator&(archive& ar, token_location& loc)
{
    vle_int i{utils::numeric_cast<std::int64_t>(loc.col)};
    ar & i;
    loc.col = i.i;

    i.i = utils::numeric_cast<std::int64_t>(loc.line);
    ar & i;
    loc.line = i.i;

    return ar;
}

archive& operator&(archive& ar, token_type& ty)
{
    auto i = static_cast<std::uint8_t>(ty);
    ar & i;
    if(i > static_cast<std::uint8_t>(token_type::last))
    {
        throw serialization_error(
          fmt::format(
            "Invalid token type ({} > {}).",
            i,
            static_cast<std::uint8_t>(token_type::last)));
    }
    ty = static_cast<token_type>(i);

    return ar;
}

std::string to_string(const token_location& loc)
{
    return fmt::format("{}:{}", loc.line, loc.col);
}

std::string to_string(token_type ty)
{
    switch(ty)
    {
    case token_type::unknown: return "unknown";
    case token_type::delimiter: return "delimiter";
    case token_type::identifier: return "identifier";
    case token_type::macro_identifier: return "macro_identifier";
    case token_type::macro_name: return "macro_name";
    case token_type::int_literal: return "int_literal";
    case token_type::fp_literal: return "fp_literal";
    case token_type::str_literal: return "str_literal";
    }

    return "<unknown>";
}

/**
 * Serialize a token value.
 *
 * @param ar The archive to use for serialization.
 * @param ty The token type.
 * @param v The token value.
 * @throw Throws a `serialization_error` if the token type is a literal and `v` contains no value,
 *        or if the token literal type is unknown.
 */
static void serialize_token_value(
  archive& ar,
  token_type ty,
  std::optional<std::variant<std::int32_t, float, std::string>>& v)
{
    bool has_value = v.has_value();
    ar & has_value;

    if(!has_value
       && (ty == token_type::int_literal
           || ty == token_type::fp_literal
           || ty == token_type::str_literal))
    {
        throw serialization_error(
          "Cannot serialize literal without value.");
    }

    if(ar.is_reading())
    {
        if(ty == token_type::int_literal)
        {
            std::int32_t i{0};
            ar & i;
            v = i;
        }
        else if(ty == token_type::fp_literal)
        {
            float f{0};
            ar & f;
            v = f;
        }
        else if(ty == token_type::str_literal)
        {
            std::string s;
            ar & s;
            v = s;
        }
        else if(has_value)
        {
            throw serialization_error(
              fmt::format(
                "Cannot serialize value for unknown literal type '{}'.",
                static_cast<std::int32_t>(ty)));
        }
    }
    else
    {
        if(ty == token_type::int_literal)
        {
            std::int32_t i{std::get<std::int32_t>(v.value())};
            ar & i;
        }
        else if(ty == token_type::fp_literal)
        {
            float f{std::get<float>(v.value())};
            ar & f;
        }
        else if(ty == token_type::str_literal)
        {
            std::string s{std::get<std::string>(v.value())};
            ar & s;
        }
        else if(has_value)
        {
            throw serialization_error(
              fmt::format(
                "Cannot serialize value for unknown literal type '{}'.",
                static_cast<std::int32_t>(ty)));
        }
    }
}

archive& operator&(archive& ar, token& tok)
{
    ar & tok.s;
    ar & tok.location;
    ar & tok.type;

    serialize_token_value(ar, tok.type, tok.value);

    return ar;
}

}    // namespace slang
