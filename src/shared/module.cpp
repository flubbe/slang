/**
 * slang - a simple scripting language.
 *
 * compiled binary file (=module) support.
 *
 * \author Felix Lubbe
 * \copyright Copyright (c) 2025
 * \license Distributed under the MIT software license (see accompanying LICENSE.txt).
 */

#include <algorithm>
#include <format>
#include <utility>

#include "module.h"
#include "type_utils.h" /* for slang::typing::is_reference_type */
#include "utils.h"

namespace slang::module_
{

namespace ty = slang::typing;

/*
 * type encoding and decoding.
 */

/** Type encoding pairs as `(type, encoded_type)`. */
static const std::vector<std::pair<std::string, std::string>> type_encoding = {
  {"void", "v"},
  {"i8", "b"},
  {"i16", "s"},
  {"i32", "i"},
  {"i64", "l"},
  {"f32", "f"},
  {"f64", "d"},
  {"str", "a"}};

static constexpr char type_prefix = 'C';

std::string variable_type::encode() const
{
    auto it = std::ranges::find_if(
      std::as_const(type_encoding),
      [this](const std::pair<std::string, std::string>& p) -> bool
      { return p.first == decoded_type_string; });

    if(it != type_encoding.cend())
    {
        return std::format("{:[>{}}{}", "", array_dims.value_or(0), it->second);
    }

    // assume it is a struct.
    if(decoded_type_string.empty())
    {
        throw module_error("Cannot encode empty struct name.");
    }

    return std::format("{:[>{}}{}{};", "", array_dims.value_or(0), type_prefix, decoded_type_string);
}

void variable_type::set_from_encoded(const std::string& s)
{
    std::string base_s;

    std::size_t array_dim_indicator_end = s.find_first_not_of('[');
    if(array_dim_indicator_end == std::string::npos)
    {
        throw module_error(std::format("Cannot decode invalid type '{}'.", s));
    }

    base_s = s.substr(array_dim_indicator_end);
    array_dims = array_dim_indicator_end > 0 ? std::make_optional(array_dim_indicator_end) : std::nullopt;

    auto it = std::ranges::find_if(
      std::as_const(type_encoding),
      [&base_s](const std::pair<std::string, std::string>& p) -> bool
      { return p.second == base_s; });

    if(it != type_encoding.cend())
    {
        decoded_type_string = it->first;
    }
    else if(base_s.length() >= 3 && base_s[0] == type_prefix)
    {
        if(base_s[base_s.length() - 1] != ';')
        {
            throw module_error("Cannot decode type with invalid name.");
        }
        decoded_type_string = base_s.substr(1, base_s.length() - 2);
    }
    else
    {
        throw module_error(std::format("Cannot decode unknown type '{}'.", s));
    }
}

archive& operator&(archive& ar, variable_type& ty)
{
    if(ar.is_reading())
    {
        char c{0};
        std::string s;

        do    // NOLINT(cppcoreguidelines-avoid-do-while)
        {
            ar & c;
            s += c;
        } while(c == '[');

        if(c == type_prefix)
        {
            do    // NOLINT(cppcoreguidelines-avoid-do-while)
            {
                ar & c;
                s += c;
            } while(c != ';');
        }

        ty.set_from_encoded(s);

        vle_int i;
        ar & i;
        ty.import_index = i.i >= 0 ? std::make_optional(i.i) : std::nullopt;
    }
    else if(ar.is_writing())
    {
        auto encoded_type = ty.encode();
        for(char c: encoded_type)
        {
            ar & c;
        }

        vle_int i{ty.import_index.has_value()
                    ? utils::numeric_cast<std::int64_t>(ty.import_index.value())
                    : static_cast<std::int64_t>(-1)};
        ar & i;
    }
    else
    {
        throw module_error("Invalid archive mode.");
    }

    return ar;
}

std::string to_string(const variable_type& t)
{
    std::string s = t.base_type();
    for(std::size_t i = 0; i < t.get_array_dims(); ++i)
    {
        s += "[]";
    }
    return s;
}

/*
 * variable_descriptor.
 */

variable_descriptor::variable_descriptor(variable_type type)
: symbol{.size = 0, .offset = 0}
, type{std::move(type)}
, reference{ty::is_reference_type(this->type.base_type())}
{
}

archive& operator&(archive& ar, variable_descriptor& desc)
{
    ar & desc.type;
    desc.reference = ty::is_reference_type(desc.type.base_type());
    return ar;
}

/*
 * language_module implementation.
 */

std::size_t language_module::add_import(symbol_type type, std::string name, std::uint32_t package_index)
{
    auto it = std::ranges::find_if(
      std::as_const(header.imports),
      [type, &name](const imported_symbol& s) -> bool
      {
          return s.type == type && s.name == name;
      });
    if(it != header.imports.cend())
    {
        return std::distance(header.imports.cbegin(), it);
    }

    header.imports.emplace_back(type, std::move(name), package_index);
    return header.imports.size() - 1;
}

void language_module::add_function(
  std::string name,
  variable_type return_type,
  std::vector<variable_type> arg_types,
  std::size_t size, std::size_t entry_point,
  std::vector<variable_descriptor> locals)
{
    if(std::ranges::find_if(
         std::as_const(header.exports),
         [&name](const exported_symbol& s) -> bool
         {
             return s.type == symbol_type::function && s.name == name;
         })
       != header.exports.cend())
    {
        throw module_error(std::format("Cannot add function: Symbol '{}' already defined.", name));
    }

    function_descriptor desc{
      function_signature{std::move(return_type), std::move(arg_types)},
      false,
      function_details{size, entry_point, std::move(locals)}};
    header.exports.emplace_back(symbol_type::function, name, std::move(desc));
}

void language_module::add_native_function(
  std::string name,
  variable_type return_type,
  std::vector<variable_type> arg_types,
  std::string lib_name)
{
    if(std::ranges::find_if(
         std::as_const(header.exports),
         [&name](const exported_symbol& s) -> bool
         {
             return s.type == symbol_type::function && s.name == name;
         })
       != header.exports.cend())
    {
        throw module_error(std::format("Cannot add native function: '{}' already defined.", name));
    }

    function_descriptor desc{
      function_signature{std::move(return_type), std::move(arg_types)},
      true,
      native_function_details{std::move(lib_name)}};
    header.exports.emplace_back(symbol_type::function, name, std::move(desc));
}

void language_module::add_struct(
  std::string name,
  std::vector<std::pair<std::string, field_descriptor>> members,
  uint8_t flags)
{
    if(std::ranges::find_if(
         std::as_const(header.exports),
         [&name](const exported_symbol& s) -> bool
         {
             return s.type == symbol_type::type && s.name == name;
         })
       != header.exports.cend())
    {
        throw module_error(std::format("Cannot add type: '{}' already defined.", name));
    }

    struct_descriptor desc{
      .flags = flags, .member_types = std::move(members)};
    header.exports.emplace_back(symbol_type::type, name, std::move(desc));
}

void language_module::add_constant(std::string name, std::size_t i)
{
    if(std::ranges::find_if(
         std::as_const(header.exports),
         [&name](const exported_symbol& s) -> bool
         {
             return s.type == symbol_type::constant && s.name == name;
         })
       != header.exports.cend())
    {
        throw module_error(std::format("Cannot add constant: '{}' already defined.", name));
    }

    header.exports.emplace_back(symbol_type::constant, name, i);
}

void language_module::add_macro(std::string name, macro_descriptor desc)
{
    if(std::ranges::find_if(
         std::as_const(header.exports),
         [&name](const exported_symbol& s) -> bool
         {
             return s.type == symbol_type::macro && s.name == name;
         })
       != header.exports.cend())
    {
        throw module_error(std::format("Cannot add macro: '{}' already defined.", name));
    }

    header.exports.emplace_back(symbol_type::macro, name, std::move(desc));
}

}    // namespace slang::module_
