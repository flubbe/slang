/**
 * slang - a simple scripting language.
 *
 * code generation.
 *
 * \author Felix Lubbe
 * \copyright Copyright (c) 2025
 * \license Distributed under the MIT software license (see accompanying LICENSE.txt).
 */

#include <format>

#include "shared/module.h"
#include "shared/opcodes.h"
#include "shared/type_utils.h"
#include "ast/ast.h"
#include "codegen.h"
#include "resolve.h"
#include "utils.h"

namespace slang::codegen
{

namespace ty = slang::typing;
namespace rs = slang::resolve;

/*
 * Exceptions.
 */

codegen_error::codegen_error(const token_location& loc, const std::string& message)
: std::runtime_error{std::format("{}: {}", to_string(loc), message)}
{
}

/*
 * Binary operators.
 */

std::string to_string(binary_op op)
{
    static const std::array<std::string, 18> strs = {
      "mul",
      "div",
      "mod",
      "add",
      "sub",
      "shl",
      "shr",
      "cmpl",
      "cmple",
      "cmpg",
      "cmpge",
      "cmpeq",
      "cmpne",
      "and",
      "xor",
      "or",
      "land",
      "lor"};

    auto idx = static_cast<std::size_t>(op);
    if(idx >= strs.size())
    {
        throw codegen_error("Invalid operator index in to_string(binary_op).");
    }

    return strs[idx];    // NOLINT(cppcoreguidelines-pro-bounds-constant-array-index)
}

/*
 * Type casts.
 */

std::string to_string(type_cast tc)
{
    static const std::array<std::string, 2> strs = {"i32_to_f32", "f32_to_i32"};

    auto idx = static_cast<std::size_t>(tc);
    if(idx >= strs.size())
    {
        throw codegen_error("Invalid type cast index in to_string(type_cast).");
    }

    return strs[idx];    // NOLINT(cppcoreguidelines-pro-bounds-constant-array-index)
}

/*
 * type.
 */

type_class to_type_class(const std::string& s)
{
    static const std::unordered_map<std::string, type_class> map = {
      {"void", type_class::void_},
      {"i32", type_class::i32},
      {"f32", type_class::f32},
      {"str", type_class::str}};

    auto it = map.find(s);
    if(it == map.end())
    {
        throw codegen_error(std::format("No type class for type '{}'.", s));
    }

    return it->second;
}

std::string type::to_string() const
{
    static const std::unordered_map<type_class, std::string> map = {
      {type_class::void_, "void"},
      {type_class::null, "null"},
      {type_class::i32, "i32"},
      {type_class::f32, "f32"},
      {type_class::str, "str"},
      {type_class::addr, "@addr"},
      {type_class::fn, "fn"}};

    auto it = map.find(ty);
    if(it != map.end())
    {
        if(is_array())
        {
            return std::format("[{}]", it->second);
        }
        return it->second;
    }

    if(ty == type_class::struct_)
    {
        if(is_array())
        {
            return std::format("[{}]", struct_name.value_or("<unnamed-struct>"));
        }
        return struct_name.value_or("<unnamed-struct>");
    }

    throw std::runtime_error("Invalid type class.");
}

/*
 * value.
 */

void value::validate() const
{
    bool is_builtin = (ty.get_type_class() == type_class::void_)
                      || (ty.get_type_class() == type_class::i32)
                      || (ty.get_type_class() == type_class::f32)
                      || (ty.get_type_class() == type_class::str)
                      || (ty.get_type_class() == type_class::fn);

    if(is_builtin || ty.is_null())
    {
        if(ty.is_struct())
        {
            throw codegen_error("Type cannot be both: struct and reference.");
        }

        if(ty.is_void() && ty.is_array())
        {
            throw codegen_error("Type cannot be both: void and array.");
        }

        return;
    }

    if(ty.get_type_class() == type_class::addr)
    {
        return;
    }

    if(!ty.is_struct())
    {
        throw codegen_error(std::format("Invalid value type '{}'.", ty.to_string()));
    }

    if(!ty.get_struct_name().has_value() || ty.get_struct_name()->empty())
    {
        throw codegen_error("Empty struct type.");
    }

    if(ty::is_builtin_type(*ty.get_struct_name()))
    {
        throw codegen_error(std::format("Aggregate type cannot have the same name '{}' as a built-in type.", *ty.get_struct_name()));
    }
}

std::string value::to_string() const
{
    if(has_name())
    {
        return std::format("{} %{}", get_type().to_string(), get_name().value());    // NOLINT(bugprone-unchecked-optional-access)
    }
    return std::format("{}", get_type().to_string());
}

/*
 * const_argument.
 */

std::string const_argument::to_string() const
{
    std::string type_name = type->get_type().to_string();

    if(type_name == "i32")
    {
        return std::format("i32 {}", static_cast<constant_int*>(type.get())->get_int());    // NOLINT(cppcoreguidelines-pro-type-static-cast-downcast)
    }

    if(type_name == "f32")
    {
        return std::format("f32 {}", static_cast<constant_float*>(type.get())->get_float());    // NOLINT(cppcoreguidelines-pro-type-static-cast-downcast)
    }

    if(type_name == "str")
    {
        return std::format("str @{}", static_cast<constant_str*>(type.get())->get_constant_index());    // NOLINT(cppcoreguidelines-pro-type-static-cast-downcast)
    }

    throw codegen_error(std::format("Unrecognized const_argument type."));
}

/*
 * instruction.
 */

std::string instruction::to_string() const
{
    std::string buf;
    if(!args.empty())
    {
        buf = " ";
        for(std::size_t i = 0; i < args.size() - 1; ++i)
        {
            buf += args[i]->to_string() + ", ";
        }
        buf += args.back()->to_string();
    }
    return std::format("{}{}", name, buf);
}

/*
 * basic_block.
 */

std::string basic_block::to_string() const
{
    if(instrs.empty())
    {
        return std::format("{}:", label);
    }

    std::string buf = std::format("{}:\n", label);
    for(std::size_t i = 0; i < instrs.size() - 1; ++i)
    {
        buf += std::format(" {}\n", instrs[i]->to_string());
    }
    buf += std::format(" {}", instrs.back()->to_string());
    return buf;
}

bool basic_block::is_valid() const
{
    std::size_t branch_return_count = 0;
    bool last_instruction_branch_return = false;
    for(const auto& it: instrs)
    {
        last_instruction_branch_return = it->is_branching() || it->is_return();
        if(last_instruction_branch_return)
        {
            branch_return_count += 1;
        }
    }
    return branch_return_count == 1 && last_instruction_branch_return;
}

/*
 * scope.
 */

bool scope::contains(const std::string& name) const
{
    if(std::ranges::find_if(
         args,
         [&name](const std::unique_ptr<value>& v) -> bool
         {
             if(!v->has_name())
             {
                 throw codegen_error("Scope contains unnamed value.");
             }

             return *v->get_name() == name;
         })
       != args.cend())
    {
        return true;
    }

    if(std::ranges::find_if(
         locals,
         [&name](const std::unique_ptr<value>& v) -> bool
         {
             if(!v->has_name())
             {
                 throw codegen_error("Scope contains unnamed value.");
             }

             return *v->get_name() == name;
         })
       != locals.cend())
    {
        return true;
    }

    return false;
}

bool scope::contains_struct(const std::string& name, const std::optional<std::string>& import_path) const
{
    auto it = std::ranges::find_if(
      structs,
      [&name, &import_path](const std::pair<std::string, struct_>& p) -> bool
      {
          return p.first == name
                 && p.second.get_import_path() == import_path;
      });
    return it != structs.cend();
}

value* scope::get_value(const std::string& name)
{
    auto it = std::ranges::find_if(
      args,
      [&name](const std::unique_ptr<value>& v) -> bool
      {
          if(!v->has_name())
          {
              throw codegen_error("Scope contains unnamed value.");
          }

          return *v->get_name() == name;
      });
    if(it != args.end())
    {
        return it->get();
    }

    it = std::ranges::find_if(
      locals,
      [&name](const std::unique_ptr<value>& v) -> bool
      {
          if(!v->has_name())
          {
              throw codegen_error("Scope contains unnamed value.");
          }

          return *v->get_name() == name;
      });

    if(it != locals.end())
    {
        return it->get();
    }

    return nullptr;
}

std::size_t scope::get_index(const std::string& name) const
{
    auto it = std::ranges::find_if(
      args,
      [&name](const std::unique_ptr<value>& v) -> bool
      {
          if(!v->has_name())
          {
              throw codegen_error("Scope contains unnamed value.");
          }

          return *v->get_name() == name;
      });
    if(it != args.cend())
    {
        return std::distance(args.cbegin(), it);
    }

    it = std::ranges::find_if(
      locals,
      [&name](const std::unique_ptr<value>& v) -> bool
      {
          if(!v->has_name())
          {
              throw codegen_error("Scope contains unnamed value.");
          }

          return *v->get_name() == name;
      });

    if(it != locals.cend())
    {
        return args.size() + std::distance(locals.cbegin(), it);
    }

    throw codegen_error(std::format("Name '{}' not found in scope.", name));
}

void scope::add_argument(std::unique_ptr<value> arg)
{
    if(!arg->has_name())
    {
        throw codegen_error("Cannot add unnamed argument to scope.");
    }

    if(contains(arg->get_name().value()))    // NOLINT(bugprone-unchecked-optional-access)
    {
        throw codegen_error(
          std::format(
            "Name '{}' already contained in scope.",
            arg->get_name().value()));    // NOLINT(bugprone-unchecked-optional-access)
    }
    args.emplace_back(std::move(arg));
}

void scope::add_local(std::unique_ptr<value> arg)
{
    if(!arg->has_name())
    {
        throw codegen_error("Cannot add unnamed argument to scope.");
    }

    if(contains(arg->get_name().value()))    // NOLINT(bugprone-unchecked-optional-access)
    {
        throw codegen_error(
          std::format(
            "Name '{}' already contained in scope.",
            arg->get_name().value()));    // NOLINT(bugprone-unchecked-optional-access)
    }
    locals.emplace_back(std::move(arg));
}

void scope::add_struct(
  std::string name,
  std::vector<std::pair<std::string, value>> members,
  std::uint8_t flags,
  std::optional<std::string> import_path)
{
    auto it = std::ranges::find_if(
      structs,
      [&name, &import_path](const std::pair<std::string, struct_>& p) -> bool
      {
          return p.first == name
                 && p.second.get_import_path() == import_path;
      });
    if(it != structs.end())
    {
        if(import_path.has_value())
        {
            throw codegen_error(std::format("Type '{}' from '{}' already exists in scope.", name, import_path.value()));
        }
        throw codegen_error(std::format("Type '{}' already exists in scope.", name));
    }

    std::string name_copy = name;
    structs.insert({name, struct_{std::move(name_copy), std::move(members), flags, import_path}});
}

const std::vector<std::pair<std::string, value>>& scope::get_struct(const std::string& name, std::optional<std::string> import_path) const
{
    auto it = std::ranges::find_if(
      structs,
      [&name, &import_path](const std::pair<std::string, struct_>& p) -> bool
      {
          return p.first == name
                 && p.second.get_import_path() == import_path;
      });
    if(it == structs.cend())
    {
        if(import_path.has_value())
        {
            throw codegen_error(std::format("Type '{}' from '{}' not found in scope.", name, import_path.value()));
        }
        throw codegen_error(std::format("Type '{}' not found in scope.", name));
    }
    return it->second.get_members();
}

/*
 * function.
 */

std::string function::to_string() const
{
    std::string buf;
    if(native)
    {
        buf = std::format("native ({}) {} @{}(", import_library, return_type->to_string(), name);
    }
    else
    {
        buf = std::format("define {} @{}(", return_type->to_string(), name);
    }

    const auto& args = scope.get_args();
    if(!args.empty())
    {
        for(std::size_t i = 0; i < args.size() - 1; ++i)
        {
            buf += std::format("{}, ", args[i]->to_string());
        }
        buf += std::format("{})", args.back()->to_string());
    }
    else
    {
        buf += ")";
    }

    if(!native)
    {
        buf += " {\n";
        for(const auto& v: scope.get_locals())
        {
            buf += std::format("local {}\n", v->to_string());
        }
        for(const auto& b: instr_blocks)
        {
            buf += std::format("{}\n", b->to_string());
        }
        buf += "}";
    }

    return buf;
}

/*
 * type.
 */

std::string struct_::to_string() const
{
    std::string buf = std::format("%{} = type {{\n", name);
    if(!members.empty())
    {
        for(std::size_t i = 0; i < members.size() - 1; ++i)
        {
            buf += std::format(" {} %{},\n", members[i].second.get_type().to_string(), members[i].first);
        }
        buf += std::format(" {} %{},\n", members.back().second.get_type().to_string(), members.back().first);
    }
    buf += "}";
    return buf;
}

/*
 * context.
 */

void context::add_import(module_::symbol_type type, std::string import_path, std::string name)
{
    auto it = std::ranges::find_if(
      imports,
      [&name](const imported_symbol& s) -> bool
      {
          return name == s.name;
      });
    if(it != imports.end())
    {
        // check whether the imports match.
        if(import_path != it->import_path)
        {
            throw codegen_error(std::format("Found different paths for name '{}': '{}' and '{}'", name, import_path, it->import_path));
        }

        if(it->type != type)
        {
            throw codegen_error(
              std::format("Found different symbol types for import '{}': '{}' and '{}'.",
                          name,
                          slang::module_::to_string(it->type),
                          slang::module_::to_string(type)));
        }
    }
    else
    {
        // add the import.
        imports.emplace_back(type, std::move(name), std::move(import_path));
    }
}

std::size_t context::get_import_index(
  module_::symbol_type type,
  std::string import_path,
  std::string name) const
{
    auto it = std::ranges::find_if(
      imports,
      [&name](const imported_symbol& s) -> bool
      {
          return name == s.name;
      });
    if(it != imports.cend())
    {
        if(it->import_path != import_path)
        {
            throw codegen_error(std::format("Found different paths for name '{}': '{}' and '{}'", name, import_path, it->import_path));
        }

        if(it->type != type)
        {
            throw codegen_error(
              std::format("Found different symbol types for import '{}': '{}' and '{}'.",
                          name,
                          slang::module_::to_string(it->type),
                          slang::module_::to_string(type)));
        }

        return std::distance(imports.cbegin(), it);
    }

    throw codegen_error(
      std::format("Symbol '{}' of type '{}' with path '{}' not found in imports.",
                  name,
                  slang::module_::to_string(type),
                  import_path));
}

void context::make_import_explicit(
  const std::string& import_path)
{
    for(auto& m: macros)
    {
        if(m->get_import_path() == import_path)
        {
            m->set_transitive(false);
        }
    }

    for(auto& c: imported_constants)
    {
        if(c.import_path == import_path)
        {
            if(c.name.value().at(0) == '$')
            {
                c.name = c.name.value().substr(1);
            }
        }
    }

    for(auto& sym: imports)
    {
        if(sym.import_path == import_path)
        {
            if(sym.name.at(0) == '$')
            {
                sym.name = sym.name.substr(1);
            }
        }
    }

    for(auto& it: prototypes)
    {
        if(it->get_import_path() == import_path)
        {
            it->make_import_explicit();
        }
    }

    for(auto& it: types)
    {
        if(it->get_import_path() == import_path)
        {
            if(it->name.at(0) == '$')
            {
                it->name = it->name.substr(1);
            }
        }
    }
}

struct_* context::add_struct(
  std::string name,
  std::vector<std::pair<std::string, value>> members,
  std::uint8_t flags,
  std::optional<std::string> import_path)
{
    if(std::ranges::find_if(
         types,
         [&name](const std::unique_ptr<struct_>& t) -> bool
         {
             return t->get_name() == name;
         })
       != types.end())
    {
        throw codegen_error(std::format("Type '{}' already defined.", name));
    }

    return types.emplace_back(
                  std::make_unique<struct_>(
                    name,
                    std::move(members),
                    flags,
                    std::move(import_path)))
      .get();
}

struct_* context::get_type(const std::string& name, std::optional<std::string> import_path)
{
    auto it = std::ranges::find_if(
      types,
      [&name, &import_path](const std::unique_ptr<struct_>& t) -> bool
      {
          return t->get_name() == name
                 && ((!import_path.has_value() && !t->get_import_path().has_value())
                     || *import_path == *t->get_import_path());
      });
    if(it == types.end())
    {
        if(import_path.has_value())
        {
            throw codegen_error(std::format("Type '{}' from import '{}' not found.", name, *import_path));
        }

        throw codegen_error(std::format("Type '{}' not found.", name));
    }

    return it->get();
}

/** Same as `std::false_type`, but taking a parameter argument. */
template<typename T>
struct false_type : public std::false_type
{
};

/**
 * Maps `std::int32_t`, `float` and `std::string` to the corresponding
 * values in `module_::constant_type`.
 *
 * @param value The type to map.
 */
template<typename T>
constexpr module_::constant_type map_constant_type()
{
    if constexpr(std::same_as<T, std::int32_t>)
    {
        return module_::constant_type::i32;
    }
    else if constexpr(std::same_as<T, float>)
    {
        return module_::constant_type::f32;
    }
    else if constexpr(std::same_as<T, std::string>)
    {
        return module_::constant_type::str;
    }
    else
    {
        static_assert(false_type<T>::value, "Unsupported constant type.");
    }
}

/**
 * Add a constant to the corresponding table. That is, it is added to the import table
 * if `import_path` is specified, and to the module's constant table otherwise.
 *
 * @param module_constants The module's constant table.
 * @param imported_constants The module's imported constants.
 * @param name The constant's name.
 * @param value The constant's value.
 * @param import_path An import path or `std::nullopt`.
 */
template<typename T>
void add_constant(
  std::vector<constant_table_entry>& module_constants,
  std::vector<constant_table_entry>& imported_constants,
  std::string name,
  const T& value,
  std::optional<std::string> import_path)
{
    if(import_path.has_value())
    {
        // add constant to imported constants table.
        auto it = std::ranges::find_if(
          imported_constants,
          [&name, &import_path](const constant_table_entry& entry) -> bool
          {
              return entry.name.has_value() && *entry.name == name && entry.import_path == import_path;
          });
        if(it != imported_constants.end())
        {
            throw codegen_error(std::format("Imported constant with name '{}' already exists.", name));
        }

        imported_constants.emplace_back(
          map_constant_type<T>(),
          std::move(value),
          std::move(import_path),
          std::move(name));
    }
    else
    {
        // add constant to constants table.
        auto it = std::ranges::find_if(
          module_constants,
          [&name](const constant_table_entry& entry) -> bool
          {
              return entry.name.has_value() && *entry.name == name;
          });
        if(it != module_constants.end())
        {
            throw codegen_error(std::format("Constant with name '{}' already exists.", name));
        }

        module_constants.emplace_back(
          map_constant_type<T>(),
          std::move(value),
          std::move(import_path),
          std::move(name),
          true);
    }
}

void context::add_constant(
  std::string name,
  std::int32_t i,
  std::optional<std::string> import_path)
{
    slang::codegen::add_constant(
      constants,
      imported_constants,
      std::move(name),
      i,
      std::move(import_path));
}

void context::add_constant(
  std::string name,
  float f,
  std::optional<std::string> import_path)
{
    slang::codegen::add_constant(
      constants,
      imported_constants,
      std::move(name),
      f,
      std::move(import_path));
}

void context::add_constant(
  std::string name,
  const std::string& s,
  std::optional<std::string> import_path)
{
    slang::codegen::add_constant(
      constants,
      imported_constants,
      std::move(name),
      s,
      std::move(import_path));
}

void context::register_constant_name(token name)
{
    constant_names.emplace(std::move(name));
}

bool context::has_registered_constant_name(const std::string& name)
{
    return std::ranges::find_if(
             constant_names,
             [&name](const token& s) -> bool
             {
                 return s.s == name;
             })
           != constant_names.end();
}

std::size_t context::get_string(std::string str)
{
    auto it = std::ranges::find_if(
      constants,
      [&str](const module_::constant_table_entry& t) -> bool
      {
          return t.type == module_::constant_type::str && std::get<std::string>(t.data) == str;
      });
    if(it != constants.end())
    {
        it->import_path = std::nullopt;
        return std::distance(constants.begin(), it);
    }

    constants.emplace_back(module_::constant_type::str, std::move(str));
    return constants.size() - 1;
}

std::optional<constant_table_entry> context::get_constant(
  const std::string& name,
  const std::optional<std::string>& import_path)
{
    /*
     * First try to find the constant in the module's constant table.
     * If not found, search the import table and copy the constant into
     * the import table.
     */
    auto it = std::ranges::find_if(
      constants,
      [&name, &import_path](const constant_table_entry& entry) -> bool
      {
          return entry.name == name && entry.import_path == import_path;
      });
    if(it != constants.cend())
    {
        return *it;
    }

    it = std::ranges::find_if(
      imported_constants,
      [&name, &import_path](const constant_table_entry& entry) -> bool
      {
          return entry.name == name && entry.import_path == import_path;
      });
    if(it != imported_constants.end())
    {
        // copy string constants to constant table.
        if(it->type == module_::constant_type::str)
        {
            return constants.emplace_back(*it);
        }

        // return primitive constant.
        return *it;
    }

    return std::nullopt;
}

void context::add_prototype(
  std::string name,
  value return_type,
  std::vector<value> args,
  std::optional<std::string> import_path)
{
    if(std::ranges::find_if(
         prototypes,
         [&name, &import_path](const std::unique_ptr<prototype>& p) -> bool
         {
             return p->get_name() == name
                    && p->get_import_path() == import_path;
         })
       != prototypes.end())
    {
        throw codegen_error(std::format("Prototype '{}' already defined.", name));
    }

    prototypes.emplace_back(
      std::make_unique<prototype>(
        std::move(name),
        std::move(return_type),
        std::move(args),
        std::move(import_path)));
}

const prototype& context::get_prototype(const std::string& name, std::optional<std::string> import_path) const
{
    auto it = std::ranges::find_if(
      std::as_const(prototypes),
      [&name, &import_path](const std::unique_ptr<prototype>& p) -> bool
      {
          return p->get_name() == name
                 && p->get_import_path() == import_path;
      });
    if(it == prototypes.cend())
    {
        if(import_path.has_value())
        {
            throw codegen_error(std::format("Prototype '{}' not found in '{}'.", name, *import_path));
        }
        throw codegen_error(std::format("Prototype '{}' not found.", name));
    }
    return **it;
}

function* context::create_function(std::string name,
                                   std::unique_ptr<value> return_type,
                                   std::vector<std::unique_ptr<value>> args)
{
    if(std::ranges::find_if(
         funcs,
         [&name](const std::unique_ptr<function>& fn) -> bool
         {
             return fn->get_name() == name;
         })
       != funcs.end())
    {
        throw codegen_error(std::format("Function '{}' already defined.", name));
    }

    return funcs.emplace_back(std::make_unique<function>(std::move(name), std::move(return_type), std::move(args))).get();
}

void context::create_native_function(std::string lib_name,
                                     std::string name,
                                     std::unique_ptr<value> return_type,
                                     std::vector<std::unique_ptr<value>> args)
{
    if(std::ranges::find_if(
         funcs,
         [&name](const std::unique_ptr<function>& fn) -> bool
         {
             return fn->get_name() == name;
         })
       != funcs.end())
    {
        throw codegen_error(std::format("Function '{}' already defined.", name));
    }

    funcs.emplace_back(
      std::make_unique<function>(
        std::move(lib_name),
        std::move(name),
        std::move(return_type),
        std::move(args)));
}

void context::add_macro(
  std::string name,
  module_::macro_descriptor desc,
  std::optional<std::string> import_path)
{
    if(std::ranges::find_if(
         macros,
         [&name, &import_path](const std::unique_ptr<macro>& m) -> bool
         {
             return m->get_name() == name
                    && m->get_import_path() == import_path;
         })
       != macros.end())
    {
        throw codegen_error(std::format("Macro '{}' already defined.", name));
    }

    macros.emplace_back(
      std::make_unique<macro>(
        std::move(name),
        std::move(desc),
        std::move(import_path)));
}

macro* context::get_macro(
  const token& name,
  std::optional<std::string> import_path)
{
    auto it = std::ranges::find_if(
      macros,
      [&name, &import_path](const std::unique_ptr<macro>& m) -> bool
      {
          return m->get_name() == name.s
                 && m->get_import_path() == import_path;
      });

    if(it != macros.end())
    {
        return it->get();
    }

    // macro was not found.
    if(import_path.has_value())
    {
        throw codegen_error(
          name.location,
          std::format(
            "Macro '{}::{}' not found.",
            import_path.value(),
            name.s));
    }
    throw codegen_error(
      name.location,
      std::format(
        "Macro '{}' not found.",
        name.s));
}

std::size_t context::generate_macro_invocation_id()
{
    return macro_invocation_id++;
}

void context::set_insertion_point(basic_block* ip)
{
    basic_block* old_ip = insertion_point;
    insertion_point = nullptr;
    if(old_ip != nullptr)
    {
        old_ip->set_inserting_context(nullptr);
    }

    insertion_point = ip;
    if(insertion_point != nullptr && insertion_point->get_inserting_context() != this)
    {
        insertion_point->set_inserting_context(this);
    }
}

/*
 * Struct access.
 */

void context::push_struct_access(type ty)
{
    struct_access.push_back(std::move(ty));
}

void context::pop_struct_access()
{
    if(!struct_access.empty())
    {
        struct_access.pop_back();
    }
    else
    {
        throw codegen_error("Cannot pop struct from access stack: The stack is empty.");
    }
}

type context::get_accessed_struct() const
{
    if(struct_access.empty())
    {
        throw codegen_error("Cannot get struct access name: No struct accessed.");
    }

    return struct_access.back();
}

value context::get_struct_member(
  token_location loc,
  const std::string& struct_name,
  const std::string& member_name,
  std::optional<std::string> import_path) const
{
    const scope* s = get_global_scope();
    const auto& members = s->get_struct(struct_name, std::move(import_path));

    auto it = std::ranges::find_if(
      std::as_const(members),
      [&member_name](const std::pair<std::string, value>& v)
      {
          return v.first == member_name;
      });
    if(it == members.cend())
    {
        throw codegen_error(
          loc,
          std::format("Struct '{}' does not contain a field with name '{}'.",
                      struct_name, member_name));
    }

    return it->second;
}

/*
 * Compile-time expression evaluation.
 */

void context::set_expression_constant(const ast::expression& expr, bool is_constant)
{
    constant_expressions[&expr] = is_constant;
}

bool context::get_expression_constant(const ast::expression& expr) const
{
    auto it = constant_expressions.find(&expr);
    if(it == constant_expressions.end())
    {
        throw codegen_error(expr.get_location(), "Expression is not known to be constant.");
    }

    return it->second;
}

bool context::has_expression_constant(const ast::expression& expr) const
{
    return constant_expressions.contains(&expr);
}

void context::set_expression_value(const ast::expression& expr, std::unique_ptr<value> v)
{
    expression_values[&expr] = std::move(v);
}

const value& context::get_expression_value(const ast::expression& expr) const
{
    auto it = expression_values.find(&expr);
    if(it == expression_values.end())
    {
        throw codegen_error(expr.get_location(), "Expression value not found.");
    }

    return *it->second;
}

bool context::has_expression_value(const ast::expression& expr) const
{
    return expression_values.contains(&expr);
}

/*
 * Code generation.
 */

void context::generate_arraylength()
{
    validate_insertion_point();
    insertion_point->add_instruction(std::make_unique<instruction>("arraylength"));
}

void context::generate_binary_op(binary_op op, const value& op_type)
{
    validate_insertion_point();
    std::vector<std::unique_ptr<argument>> args;
    args.emplace_back(std::make_unique<type_argument>(op_type));
    insertion_point->add_instruction(std::make_unique<instruction>(codegen::to_string(op), std::move(args)));
}

void context::generate_branch(basic_block* block)
{
    if(block == nullptr)
    {
        throw codegen_error("context::generate_branch: Invalid block.");
    }

    validate_insertion_point();
    auto arg = std::make_unique<label_argument>(block->get_label());
    std::vector<std::unique_ptr<argument>> args;
    args.emplace_back(std::move(arg));
    insertion_point->add_instruction(std::make_unique<instruction>("jmp", std::move(args)));
}

void context::generate_cast(type_cast tc)
{
    validate_insertion_point();
    auto arg0 = std::make_unique<cast_argument>(tc);
    std::vector<std::unique_ptr<argument>> args;
    args.emplace_back(std::move(arg0));
    insertion_point->add_instruction(std::make_unique<instruction>("cast", std::move(args)));
}

void context::generate_checkcast(type target_type)
{
    validate_insertion_point();
    auto arg0 = std::make_unique<type_argument>(value{std::move(target_type)});
    std::vector<std::unique_ptr<argument>> args;
    args.emplace_back(std::move(arg0));
    insertion_point->add_instruction(std::make_unique<instruction>("checkcast", std::move(args)));
}

void context::generate_cond_branch(basic_block* then_block, basic_block* else_block)
{
    if(then_block == nullptr)
    {
        throw codegen_error("context::generate_cond_branch: Invalid block.");
    }

    validate_insertion_point();

    auto arg0 = std::make_unique<label_argument>(then_block->get_label());
    auto arg1 = std::make_unique<label_argument>(else_block->get_label());
    std::vector<std::unique_ptr<argument>> args;
    args.emplace_back(std::move(arg0));
    args.emplace_back(std::move(arg1));
    insertion_point->add_instruction(std::make_unique<instruction>("jnz", std::move(args)));
}

void context::generate_const(const value& vt, std::variant<int, float, std::string> v)
{
    validate_insertion_point();
    std::vector<std::unique_ptr<argument>> args;
    if(vt.get_type().get_type_class() == type_class::i32)
    {
        auto arg = std::make_unique<const_argument>(std::get<int>(v));
        args.emplace_back(std::move(arg));
    }
    else if(vt.get_type().get_type_class() == type_class::f32)
    {
        auto arg = std::make_unique<const_argument>(std::get<float>(v));
        args.emplace_back(std::move(arg));
    }
    else if(vt.get_type().get_type_class() == type_class::str)
    {
        auto arg = std::make_unique<const_argument>(std::get<std::string>(v));
        arg->register_const(*this);
        args.emplace_back(std::move(arg));
    }
    else if(vt.get_type().get_type_class() == type_class::fn)
    {
        // Nothing to do.
    }
    else
    {
        throw codegen_error("Invalid value type for constant.");
    }
    insertion_point->add_instruction(std::make_unique<instruction>("const", std::move(args)));
}

void context::generate_const_null()
{
    validate_insertion_point();
    insertion_point->add_instruction(std::make_unique<instruction>("const_null", std::vector<std::unique_ptr<argument>>{}));
}

void context::generate_dup(value vt, std::vector<value> vals)
{
    if(vals.size() >= std::numeric_limits<std::int32_t>::max())
    {
        throw codegen_error(std::format("Depth in dup instruction exceeds maximum value ({} >= {}).", vals.size(), std::numeric_limits<std::int32_t>::max()));
    }

    validate_insertion_point();
    std::vector<std::unique_ptr<argument>> args;
    args.emplace_back(std::make_unique<type_argument>(std::move(vt)));
    for(auto& v: vals)
    {
        args.emplace_back(std::make_unique<type_argument>(std::move(v)));
    }
    insertion_point->add_instruction(std::make_unique<instruction>("dup", std::move(args)));
}

void context::generate_get_field(std::unique_ptr<field_access_argument> arg)
{
    validate_insertion_point();

    std::vector<std::unique_ptr<argument>> args;
    args.emplace_back(std::move(arg));

    insertion_point->add_instruction(std::make_unique<instruction>("get_field", std::move(args)));
}

void context::generate_invoke(std::optional<std::unique_ptr<function_argument>> name)
{
    validate_insertion_point();

    if(name.has_value())
    {
        std::vector<std::unique_ptr<argument>> args;
        args.emplace_back(std::move(*name));
        insertion_point->add_instruction(std::make_unique<instruction>("invoke", std::move(args)));
    }
    else
    {
        insertion_point->add_instruction(std::make_unique<instruction>("invoke_dynamic"));
    }
}

void context::generate_load(std::unique_ptr<argument> arg, bool load_element)
{
    validate_insertion_point();

    std::vector<std::unique_ptr<argument>> args;
    args.emplace_back(std::move(arg));

    if(load_element)
    {
        insertion_point->add_instruction(std::make_unique<instruction>("load_element", std::move(args)));
    }
    else
    {
        insertion_point->add_instruction(std::make_unique<instruction>("load", std::move(args)));
    }
}

void context::generate_new(const value& vt)
{
    validate_insertion_point();
    std::vector<std::unique_ptr<argument>> args;
    args.emplace_back(std::make_unique<type_argument>(vt));
    insertion_point->add_instruction(std::make_unique<instruction>("new", std::move(args)));
}

void context::generate_newarray(const value& vt)
{
    validate_insertion_point();
    std::vector<std::unique_ptr<argument>> args;
    args.emplace_back(std::make_unique<type_argument>(vt));
    insertion_point->add_instruction(std::make_unique<instruction>("newarray", std::move(args)));
}

void context::generate_anewarray(const value& vt)
{
    validate_insertion_point();
    std::vector<std::unique_ptr<argument>> args;
    args.emplace_back(std::make_unique<type_argument>(vt));
    insertion_point->add_instruction(std::make_unique<instruction>("anewarray", std::move(args)));
}

void context::generate_pop(const value& vt)
{
    validate_insertion_point();
    std::vector<std::unique_ptr<argument>> args;

    args.emplace_back(std::make_unique<type_argument>(vt));
    insertion_point->add_instruction(std::make_unique<instruction>("pop", std::move(args)));
}

void context::generate_ret(std::optional<value> arg)
{
    validate_insertion_point();
    std::vector<std::unique_ptr<argument>> args;
    if(!arg)
    {
        args.emplace_back(std::make_unique<type_argument>(value{type{type_class::void_, 0}}));
    }
    else
    {
        args.emplace_back(std::make_unique<type_argument>(*arg));
    }
    insertion_point->add_instruction(std::make_unique<instruction>("ret", std::move(args)));
}

void context::generate_set_field(std::unique_ptr<field_access_argument> arg)
{
    validate_insertion_point();

    std::vector<std::unique_ptr<argument>> args;
    args.emplace_back(std::move(arg));

    insertion_point->add_instruction(std::make_unique<instruction>("set_field", std::move(args)));
}

void context::generate_store(std::unique_ptr<argument> arg, bool store_element)
{
    validate_insertion_point();

    std::vector<std::unique_ptr<argument>> args;
    args.emplace_back(std::move(arg));

    if(store_element)
    {
        insertion_point->add_instruction(std::make_unique<instruction>("store_element", std::move(args)));
    }
    else
    {
        insertion_point->add_instruction(std::make_unique<instruction>("store", std::move(args)));
    }
}

std::string context::generate_label()
{
    ++label_count;
    return std::format("{}", label_count - 1);
}

/**
 * Print strings potentially containing non-alphanumeric characters.
 * These are replaced by their hex values.
 *
 * @param s The string to print.
 * @returns A printable string.
 */
static std::string make_printable(const std::string& s)
{
    std::string str;

    // replace non-printable characters by their character codes.
    for(const auto& c: s)
    {
        if(isalnum(c) == 0 && c != ' ')
        {
            str += std::format("\\x{:02x}", c);
        }
        else
        {
            str += c;
        }
    }

    return str;
};

/**
 * Print a constant including its type.
 *
 * @param index The constant's index in the constant table.
 * @param c The constant table entry.
 * @returns A string containing the constant's information.
 */
static std::string print_constant(std::size_t index, const module_::constant_table_entry& c)
{
    std::string buf;

    if(c.type == module_::constant_type::i32)
    {
        buf += std::format(".i32 @{} {}", index, std::get<std::int32_t>(c.data));
    }
    else if(c.type == module_::constant_type::f32)
    {
        buf += std::format(".f32 @{} {}", index, std::get<float>(c.data));
    }
    else if(c.type == module_::constant_type::str)
    {
        buf += std::format(".string @{} \"{}\"", index, make_printable(std::get<std::string>(c.data)));
    }
    else
    {
        buf += std::format(".<unknown> @{}", index);
    }

    return buf;
};

std::string context::to_string() const
{
    std::string buf;

    // constants.
    if(!constants.empty())
    {
        for(std::size_t i = 0; i < constants.size() - 1; ++i)
        {
            buf += std::format("{}\n", print_constant(i, constants[i]));
        }
        buf += print_constant(constants.size() - 1, constants.back());

        // don't append a newline if the constant table is the only non-empty buffer.
        if(!types.empty() || !funcs.empty())
        {
            buf += "\n";
        }
    }

    // types.
    if(!types.empty())
    {
        for(std::size_t i = 0; i < types.size() - 1; ++i)
        {
            buf += std::format("{}\n", types[i]->to_string());
        }
        buf += std::format("{}", types.back()->to_string());

        // don't append a newline if there are no functions.
        if(!funcs.empty())
        {
            buf += "\n";
        }
    }

    // functions.
    if(!funcs.empty())
    {
        for(std::size_t i = 0; i < funcs.size() - 1; ++i)
        {
            buf += std::format("{}\n", funcs[i]->to_string());
        }
        buf += std::format("{}", funcs.back()->to_string());
    }

    return buf;
}

}    // namespace slang::codegen
