/**
 * slang - a simple scripting language.
 *
 * type system context.
 *
 * \author Felix Lubbe
 * \copyright Copyright (c) 2025
 * \license Distributed under the MIT software license (see accompanying LICENSE.txt).
 */

#include <format>

#include "shared/type_utils.h"
#include "ast/ast.h"
#include "package.h"
#include "type.h"
#include "typing.h"
#include "utils.h"

namespace ty = slang::typing;

namespace slang::typing
{

/*
 * Exceptions.
 */

type_error::type_error(const source_location& loc, const std::string& message)
: std::runtime_error{std::format("{}: {}", to_string(loc), message)}
{
}

/*
 * type_kind.
 */

std::string to_string(type_kind kind)
{
    switch(kind)
    {
    case type_kind::builtin: return "builtin";
    case type_kind::array: return "array";
    case type_kind::struct_: return "struct";
    default:
        throw std::runtime_error(
          std::format(
            "Cannot convert type kind {} to string.",
            static_cast<int>(kind)));
    }
}

/*
 * context.
 */

std::size_t context::generate_type_id()
{
    return next_type_id++;
}

/**
 * Get the string representation of a builtin type.
 *
 * @param b The builtin type.
 * @returns String represenation of the type.
 * @throws Throws `std::runtime_error` if the type is unknown.
 */
static std::string to_string(builtins b)
{
    switch(b)
    {
    case builtins::null: return "null";
    case builtins::void_: return "void";
    case builtins::i32: return "i32";
    case builtins::f32: return "f32";
    case builtins::str: return "str";
    default:
        throw std::runtime_error(
          std::format(
            "Cannot convert builtin type {} to string.",
            static_cast<int>(b)));
    }
}

void context::initialize_builtins()
{
    null_type = add_builtin(builtins::null);
    void_type = add_builtin(builtins::void_);
    i32_type = add_builtin(builtins::i32);
    f32_type = add_builtin(builtins::f32);
    str_type = add_builtin(builtins::str);
}

type_id context::add_builtin(builtins builtin)
{
    auto it = std::ranges::find_if(
      type_info_map,
      [builtin](const auto& p) -> bool
      {
          return p.second.kind == type_kind::builtin
                 && std::get<builtin_info>(p.second.data).id == builtin;
      });
    if(it != type_info_map.end())
    {
        throw std::runtime_error(
          std::format(
            "Built-in type '{}' already defined.",
            ty::to_string(builtin)));
    }

    std::size_t id = generate_type_id();
    type_info_map.insert(
      {id,
       type_info{
         .kind = type_kind::builtin,
         .data = builtin_info{
           .id = builtin}}});

    return id;
}

type_id context::declare_struct(
  std::string name,
  std::optional<std::string> qualified_name)
{
    auto it = std::ranges::find_if(
      type_info_map,
      [&name, &qualified_name](const auto& p) -> bool
      {
          return p.second.kind == type_kind::struct_
                 && std::get<struct_info>(p.second.data).name == name
                 && std::get<struct_info>(p.second.data).qualified_name == qualified_name;
      });
    if(it != type_info_map.end())
    {
        throw type_error(
          std::format(
            "Struct '{}' {} already defined.",
            name,
            qualified_name.has_value()
              ? std::format("({})", qualified_name.value())
              : std::string{}));
    }

    std::size_t id = generate_type_id();
    type_info_map.insert(
      {id,
       type_info{
         .kind = type_kind::struct_,
         .data = struct_info{
           .name = std::move(name),
           .qualified_name = std::move(qualified_name),
           .fields = {},
           .fields_by_name = {},
           .origin_module_index = std::nullopt}}});

    return id;
}

std::size_t context::add_field(
  type_id struct_type_id,
  std::string field_name,
  type_id field_type_id)
{
    auto it = type_info_map.find(struct_type_id);
    if(it == type_info_map.end())
    {
        throw type_error(
          std::format(
            "No type with id '{}' found.",
            struct_type_id));
    }

    if(it->second.kind != type_kind::struct_)
    {
        throw type_error(
          std::format(
            "Cannot add field to non-struct type '{}'.",
            to_string(it->first)));
    }

    auto& info = std::get<struct_info>(it->second.data);
    if(info.is_sealed)
    {
        throw type_error(
          std::format(
            "Cannot add field '{}' to struct '{}': Struct is sealed.",
            field_name,
            info.name));
    }

    if(info.fields_by_name.contains(field_name))
    {
        throw type_error(
          std::format(
            "Cannot add field '{}' to struct '{}': Field already exists.",
            field_name,
            info.name));
    }

    std::size_t field_index = info.fields.size();
    info.fields_by_name.insert({field_name, field_index});

    info.fields.emplace_back(field_info{
      .name = std::move(field_name),
      .type = field_type_id});

    return field_index;
}

void context::seal_struct(type_id struct_type_id)
{
    auto it = type_info_map.find(struct_type_id);
    if(it == type_info_map.end())
    {
        throw type_error(
          std::format(
            "No type with id '{}' found.",
            struct_type_id));
    }

    if(it->second.kind != type_kind::struct_)
    {
        throw type_error(
          std::format(
            "Cannot seal non-struct type '{}'.",
            to_string(it->first)));
    }

    auto& info = std::get<struct_info>(it->second.data);
    info.is_sealed = true;
}

struct_info& context::get_struct_info(type_id struct_type_id)
{
    auto it = type_info_map.find(struct_type_id);
    if(it == type_info_map.end())
    {
        throw type_error(
          std::format(
            "No type with id '{}' found.",
            struct_type_id));
    }

    if(it->second.kind != type_kind::struct_)
    {
        throw type_error(
          std::format(
            "Cannot get field for non-struct type '{}'.",
            to_string(it->first)));
    }

    return std::get<struct_info>(it->second.data);
}

const struct_info& context::get_struct_info(type_id struct_type_id) const
{
    auto it = type_info_map.find(struct_type_id);
    if(it == type_info_map.end())
    {
        throw type_error(
          std::format(
            "No type with id '{}' found.",
            struct_type_id));
    }

    if(it->second.kind != type_kind::struct_)
    {
        throw type_error(
          std::format(
            "Cannot get field for non-struct type '{}'.",
            to_string(it->first)));
    }

    return std::get<struct_info>(it->second.data);
}

type_id context::get_field_type(
  type_id struct_type_id,
  std::size_t field_index) const
{
    auto it = type_info_map.find(struct_type_id);
    if(it == type_info_map.end())
    {
        throw type_error(
          std::format(
            "No type with id '{}' found.",
            struct_type_id));
    }

    // Special case: arrays.
    if(is_array(struct_type_id))
    {
        if(field_index == 0)
        {
            // length property.
            return get_i32_type();
        }

        throw type_error(
          std::format(
            "Field index {} out of range (0-0).",
            field_index));
    }

    if(it->second.kind != type_kind::struct_)
    {
        throw type_error(
          std::format(
            "Cannot get field for non-struct type '{}'.",
            to_string(it->first)));
    }

    const auto& info = std::get<struct_info>(it->second.data);
    if(field_index >= info.fields.size())
    {
        throw type_error(
          std::format(
            "Field index {} out of range (0-{}).",
            field_index,
            info.fields.size()));
    }

    return info.fields[field_index].type;
}

std::size_t context::get_field_index(
  type_id struct_type_id,
  const std::string& name) const
{
    auto it = type_info_map.find(struct_type_id);
    if(it == type_info_map.end())
    {
        throw type_error(
          std::format(
            "No type with id '{}' found.",
            struct_type_id));
    }

    // Special case: arrays.
    if(is_array(struct_type_id))
    {
        if(name == "length")
        {
            return 0;
        }

        throw type_error(
          std::format(
            "Unknown array property '{}'.",
            name));
    }

    if(it->second.kind != type_kind::struct_)
    {
        throw type_error(
          std::format(
            "Cannot get field for non-struct type '{}'.",
            to_string(it->first)));
    }

    const auto& info = std::get<struct_info>(it->second.data);
    auto field_it = info.fields_by_name.find(name);
    if(field_it == info.fields_by_name.end())
    {
        throw type_error(
          std::format(
            "No field '{}' found in struct '{}'.",
            name,
            info.name));
    }

    return field_it->second;
}

bool context::has_type(const std::string& name) const
{
    auto it = std::ranges::find_if(
      type_info_map,
      [this, &name](const auto& p) -> bool
      {
          // NOTE Returns/checks the qualified name for imports
          return to_string(p.first) == name;
      });

    return it != type_info_map.end();
}

type_id context::get_type(const std::string& name) const
{
    bool is_name_qualified = name.find("::") != std::string::npos;

    auto it = std::ranges::find_if(
      type_info_map,
      [this, &name, is_name_qualified](const auto& p) -> bool
      {
          if(is_name_qualified)
          {
              return p.second.kind == ty::type_kind::struct_
                     && std::get<ty::struct_info>(p.second.data).qualified_name.has_value()
                     && std::get<ty::struct_info>(p.second.data).qualified_name.value() == name;
          }

          return to_string(p.first) == name;
      });
    if(it != type_info_map.end())
    {
        return it->first;
    }

    throw type_error(
      std::format(
        "Type '{}' not found.",
        name));
}

type_info context::get_type_info(type_id id) const
{
    auto it = type_info_map.find(id);
    if(it == type_info_map.end())
    {
        throw type_error(
          std::format(
            "Type with id '{}' not found.",
            id));
    }

    return it->second;
}

std::pair<bool, bool> context::get_type_flags(type_id id) const
{
    auto it = type_info_map.find(id);
    if(it == type_info_map.end())
    {
        throw type_error(
          std::format(
            "Type with id '{}' not found.",
            id));
    }

    if(it->second.kind != type_kind::struct_)
    {
        throw type_error(
          std::format(
            "Cannot get flags for non-struct type '{}'.",
            to_string(id)));
    }

    return std::make_pair(
      std::get<struct_info>(it->second.data).native,
      std::get<struct_info>(it->second.data).allow_cast);
}

void context::set_type_flags(
  type_id id,
  std::optional<bool> native,
  std::optional<bool> allow_cast)
{
    auto it = type_info_map.find(id);
    if(it == type_info_map.end())
    {
        throw type_error(
          std::format(
            "Type with id '{}' not found.",
            id));
    }

    if(it->second.kind != type_kind::struct_)
    {
        throw type_error(
          std::format(
            "Cannot get flags for non-struct type '{}'.",
            to_string(id)));
    }

    if(native.has_value())
    {
        std::get<struct_info>(it->second.data).native = native.value();
    }

    if(allow_cast.has_value())
    {
        std::get<struct_info>(it->second.data).allow_cast = allow_cast.value();
    }
}

type_id context::get_base_type(type_id id) const
{
    auto it = type_info_map.find(id);
    if(it == type_info_map.end())
    {
        throw type_error(
          std::format(
            "Type with id {} not found in type map.",
            id));
    }

    if(it->second.kind != type_kind::array)
    {
        return id;
    }

    return std::get<array_info>(it->second.data).element_type_id;
}

std::size_t context::get_array_rank(type_id id) const
{
    auto it = type_info_map.find(id);
    if(it == type_info_map.end())
    {
        throw type_error(
          std::format(
            "Type with id {} not found in type map.",
            id));
    }

    if(it->second.kind != type_kind::array)
    {
        return 0;
    }

    return std::get<array_info>(it->second.data).rank;
}

// FIXME is this needed?
type_id context::resolve_type(const std::string& name)
{
    return get_type(name);
}

bool context::has_expression_type(const ast::expression& expr) const
{
    return expression_types.contains(&expr);
}

void context::set_expression_type(const ast::expression& expr, std::optional<type_id> id)
{
    if(expression_types.contains(&expr))
    {
        if(expression_types[&expr] != id)
        {
            throw type_error(
              expr.get_location(),
              std::format(
                "Could not set type: Expression type differs from last evaluation ('{}' != '{}').",
                expression_types[&expr].has_value()
                  ? to_string(expression_types[&expr].value())    // NOLINT(bugprone-unchecked-optional-access)
                  : std::string{"<none>"},
                id.has_value()
                  ? to_string(id.value())
                  : std::string{"<none>"}));
        }
    }
    else
    {
        expression_types[&expr] = id;
    }
}

std::optional<type_id> context::get_expression_type(const ast::expression& expr) const
{
    auto it = expression_types.find(&expr);
    if(it == expression_types.end())
    {
        throw type_error("Cannot get type for expression: Expression not known.");
    }

    return it->second;
}

bool context::is_builtin(type_id id) const
{
    return get_type_info(id).kind == type_kind::builtin;
}

bool context::is_nullable(type_id id) const
{
    // Non-primitive types and str's are nullable.
    return id != void_type && id != i32_type && id != f32_type;
}

bool context::is_reference(type_id id) const
{
    return id == get_null_type()
           || id == get_str_type()
           || is_array(id)
           || is_struct(id);
}

bool context::is_array(type_id id) const
{
    auto it = type_info_map.find(id);
    return it != type_info_map.end()
           && it->second.kind == type_kind::array;
}

bool context::is_struct(type_id id) const
{
    auto it = type_info_map.find(id);
    return it != type_info_map.end()
           && it->second.kind == type_kind::struct_;
}

type_id context::array_element_type(type_id id) const
{
    auto it = type_info_map.find(id);
    if(it == type_info_map.end())
    {
        throw type_error(
          std::format(
            "Type with id '{}' not found in type map.",
            id));
    }

    if(it->second.kind != type_kind::array)
    {
        throw type_error(
          std::format(
            "Type with id '{}' is not an array type.",
            id));
    }

    return std::get<array_info>(it->second.data).element_type_id;
}

bool context::are_types_compatible(type_id expected, type_id actual) const
{
    if(expected == actual)
    {
        return true;
    }

    if(actual == null_type)
    {
        return is_nullable(expected);
    }

    // check for sink types.
    if(is_struct(expected) && is_reference(actual))
    {
        auto info = get_type_info(expected);
        if(std::get<struct_info>(info.data).allow_cast)
        {
            return true;
        }
    }

    return false;
}

type_id context::get_array(type_id id, std::size_t rank)
{
    // normalize element id and rank.
    if(is_array(id))
    {
        rank += get_array_rank(id);
        id = get_base_type(id);
    }

    // check if the requested array type already exists.
    for(const auto& [existing_id, info]: type_info_map)
    {
        if(info.kind != type_kind::array)
        {
            continue;
        }

        const auto& data = std::get<array_info>(info.data);
        if(data.element_type_id == id && data.rank == rank)
        {
            return existing_id;
        }
    }

    // create a new array type.
    auto new_type_id = generate_type_id();
    type_info_map.insert({new_type_id,
                          type_info{
                            .kind = type_kind::array,
                            .data = array_info{
                              .element_type_id = id,
                              .rank = rank}}});

    return new_type_id;
}

std::string context::to_string(type_id id) const
{
    std::optional<std::size_t> rank{std::nullopt};

    if(is_array(id))
    {
        rank = get_array_rank(id);
        id = get_base_type(id);
    }

    auto it = type_info_map.find(id);
    if(it == type_info_map.end())
    {
        throw type_error(
          std::format(
            "Type with id '{}' not found in type map.",
            static_cast<int>(id)));
    }

    std::string ret;

    if(it->second.kind == type_kind::builtin)
    {
        ret = ::ty::to_string(std::get<builtin_info>(it->second.data).id);
    }
    else if(it->second.kind == type_kind::struct_)
    {
        const auto& info = std::get<struct_info>(it->second.data);
        if(info.qualified_name.has_value())
        {
            ret = info.qualified_name.value();
        }
        else
        {
            ret = info.name;
        }
    }
    else
    {
        throw std::runtime_error(
          std::format(
            "Unknown base type of kind {}",
            ::ty::to_string(it->second.kind)));
    }

    if(rank.has_value())
    {
        while(rank.value() > 0)
        {
            ret += "[]";
            --rank.value();
        }
    }

    return ret;
}

std::string context::to_string() const
{
    std::string ret =
      "--- Type Environment ---\n";

    if(!type_info_map.empty())
    {
        ret +=
          "\n"
          "    Type id    Name\n"
          "    -------------------\n";

        for(const auto& [id, _]: type_info_map)
        {
            ret += std::format(
              "    {:>7}    {}\n",
              id,
              to_string(id));
        }
    }

    return ret;
}

}    // namespace slang::typing
