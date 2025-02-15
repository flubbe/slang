/**
 * slang - a simple scripting language.
 *
 * code generation.
 *
 * \author Felix Lubbe
 * \copyright Copyright (c) 2024
 * \license Distributed under the MIT software license (see accompanying LICENSE.txt).
 */

#include <fmt/core.h>

#include "shared/module.h"
#include "shared/opcodes.h"
#include "shared/type_utils.h"
#include "ast.h"
#include "codegen.h"
#include "utils.h"

namespace slang::codegen
{

namespace ty = slang::typing;

/*
 * Exceptions.
 */

codegen_error::codegen_error(const token_location& loc, const std::string& message)
: std::runtime_error{fmt::format("{}: {}", to_string(loc), message)}
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

    std::size_t idx = static_cast<std::size_t>(op);
    if(idx >= strs.size())
    {
        throw codegen_error("Invalid operator index in to_string(binary_op).");
    }

    return strs[idx];
}

/*
 * Type casts.
 */

std::string to_string(type_cast tc)
{
    static const std::array<std::string, 2> strs = {"i32_to_f32", "f32_to_i32"};

    std::size_t idx = static_cast<std::size_t>(tc);
    if(idx > strs.size())
    {
        throw codegen_error("Invalid type cast index in to_string(type_cast).");
    }

    return strs[idx];
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
        throw codegen_error(fmt::format("No type class for type '{}'.", s));
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
            return fmt::format("[{}]", it->second);
        }
        return it->second;
    }

    if(ty == type_class::struct_)
    {
        if(is_array())
        {
            return fmt::format("[{}]", struct_name.value());
        }
        return struct_name.value();
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
        else if(ty.is_void() && ty.is_array())
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
        throw codegen_error(fmt::format("Invalid value type '{}'.", ty.to_string()));
    }

    if(!ty.get_struct_name().has_value() || ty.get_struct_name()->length() == 0)
    {
        throw codegen_error("Empty struct type.");
    }

    if(ty::is_builtin_type(*ty.get_struct_name()))
    {
        throw codegen_error(fmt::format("Aggregate type cannot have the same name '{}' as a built-in type.", *ty.get_struct_name()));
    }
}

std::string value::to_string() const
{
    if(has_name())
    {
        return fmt::format("{} %{}", get_type().to_string(), *get_name());
    }
    return fmt::format("{}", get_type().to_string());
}

/*
 * const_argument.
 */

std::string const_argument::to_string() const
{
    std::string type_name = type->get_type().to_string();

    if(type_name == "i32")
    {
        return fmt::format("i32 {}", static_cast<constant_int*>(type.get())->get_int());
    }
    else if(type_name == "f32")
    {
        return fmt::format("f32 {}", static_cast<constant_float*>(type.get())->get_float());
    }
    else if(type_name == "str")
    {
        return fmt::format("str @{}", static_cast<constant_str*>(type.get())->get_constant_index());
    }

    throw codegen_error(fmt::format("Unrecognized const_argument type."));
}

/*
 * instruction.
 */

std::string instruction::to_string() const
{
    std::string buf;
    if(args.size() > 0)
    {
        buf = " ";
        for(std::size_t i = 0; i < args.size() - 1; ++i)
        {
            buf += args[i]->to_string() + ", ";
        }
        buf += args.back()->to_string();
    }
    return fmt::format("{}{}", name, buf);
}

/*
 * basic_block.
 */

std::string basic_block::to_string() const
{
    if(unreachable)
    {
        return fmt::format("{}:\n unreachable", label);
    }

    if(instrs.size() == 0)
    {
        return fmt::format("{}:", label);
    }

    std::string buf = fmt::format("{}:\n", label);
    for(std::size_t i = 0; i < instrs.size() - 1; ++i)
    {
        buf += fmt::format(" {}\n", instrs[i]->to_string());
    }
    buf += fmt::format(" {}", instrs.back()->to_string());
    return buf;
}

bool basic_block::is_valid() const
{
    std::size_t branch_return_count = 0;
    bool last_instruction_branch_return = false;
    for(auto& it: instrs)
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
    if(std::find_if(
         args.begin(),
         args.end(),
         [&name](const std::unique_ptr<value>& v) -> bool
         {
             if(!v->has_name())
             {
                 throw codegen_error("Scope contains unnamed value.");
             }

             return *v->get_name() == name;
         })
       != args.end())
    {
        return true;
    }

    if(std::find_if(
         locals.begin(),
         locals.end(),
         [&name](const std::unique_ptr<value>& v) -> bool
         {
             if(!v->has_name())
             {
                 throw codegen_error("Scope contains unnamed value.");
             }

             return *v->get_name() == name;
         })
       != locals.end())
    {
        return true;
    }

    return false;
}

bool scope::contains_struct(const std::string& name, const std::optional<std::string>& import_path) const
{
    auto it = std::find_if(
      structs.begin(),
      structs.end(),
      [&name, &import_path](const std::pair<std::string, struct_>& p) -> bool
      {
          return p.first == name
                 && p.second.get_import_path() == import_path;
      });
    return it != structs.end();
}

value* scope::get_value(const std::string& name)
{
    auto it = std::find_if(
      args.begin(),
      args.end(),
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

    it = std::find_if(
      locals.begin(),
      locals.end(),
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
    auto it = std::find_if(
      args.begin(),
      args.end(),
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
        return std::distance(args.begin(), it);
    }

    it = std::find_if(
      locals.begin(),
      locals.end(),
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
        return args.size() + std::distance(locals.begin(), it);
    }

    throw codegen_error(fmt::format("Name '{}' not found in scope.", name));
}

void scope::add_argument(std::unique_ptr<value> arg)
{
    if(!arg->has_name())
    {
        throw codegen_error("Cannot add unnamed argument to scope.");
    }

    if(contains(*arg->get_name()))
    {
        throw codegen_error(fmt::format("Name '{}' already contained in scope.", *arg->get_name()));
    }
    args.emplace_back(std::move(arg));
}

void scope::add_local(std::unique_ptr<value> arg)
{
    if(!arg->has_name())
    {
        throw codegen_error("Cannot add unnamed argument to scope.");
    }

    if(contains(*arg->get_name()))
    {
        throw codegen_error(fmt::format("Name '{}' already contained in scope.", *arg->get_name()));
    }
    locals.emplace_back(std::move(arg));
}

void scope::add_struct(
  std::string name,
  std::vector<std::pair<std::string, value>> members,
  std::uint8_t flags,
  std::optional<std::string> import_path)
{
    auto it = std::find_if(
      structs.begin(),
      structs.end(),
      [&name, &import_path](const std::pair<std::string, struct_>& p) -> bool
      {
          return p.first == name
                 && p.second.get_import_path() == import_path;
      });
    if(it != structs.end())
    {
        if(import_path.has_value())
        {
            throw codegen_error(fmt::format("Type '{}' from '{}' already exists in scope.", name, import_path.value()));
        }
        throw codegen_error(fmt::format("Type '{}' already exists in scope.", name));
    }

    std::string name_copy = name;
    structs.insert({name, struct_{std::move(name_copy), std::move(members), flags, import_path}});
}

const std::vector<std::pair<std::string, value>>& scope::get_struct(const std::string& name, std::optional<std::string> import_path) const
{
    auto it = std::find_if(
      structs.begin(),
      structs.end(),
      [&name, &import_path](const std::pair<std::string, struct_>& p) -> bool
      {
          return p.first == name
                 && p.second.get_import_path() == import_path;
      });
    if(it == structs.end())
    {
        if(import_path.has_value())
        {
            throw codegen_error(fmt::format("Type '{}' from '{}' not found in scope.", name, import_path.value()));
        }
        throw codegen_error(fmt::format("Type '{}' not found in scope.", name));
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
        buf = fmt::format("native ({}) {} @{}(", import_library, return_type->to_string(), name);
    }
    else
    {
        buf = fmt::format("define {} @{}(", return_type->to_string(), name);
    }

    const auto& args = scope.get_args();
    if(args.size() > 0)
    {
        for(std::size_t i = 0; i < args.size() - 1; ++i)
        {
            buf += fmt::format("{}, ", args[i]->to_string());
        }
        buf += fmt::format("{})", args.back()->to_string());
    }
    else
    {
        buf += ")";
    }

    if(!native)
    {
        buf += " {\n";
        for(auto& v: scope.get_locals())
        {
            buf += fmt::format("local {}\n", v->to_string());
        }
        for(auto& b: instr_blocks)
        {
            buf += fmt::format("{}\n", b->to_string());
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
    std::string buf = fmt::format("%{} = type {{\n", name);
    if(members.size() > 0)
    {
        for(std::size_t i = 0; i < members.size() - 1; ++i)
        {
            buf += fmt::format(" {} %{},\n", members[i].second.get_type().to_string(), members[i].first);
        }
        buf += fmt::format(" {} %{},\n", members.back().second.get_type().to_string(), members.back().first);
    }
    buf += "}";
    return buf;
}

/*
 * context.
 */

void context::add_import(module_::symbol_type type, std::string import_path, std::string name)
{
    auto it = std::find_if(
      imports.begin(),
      imports.end(),
      [&name](const imported_symbol& s) -> bool
      {
          return name == s.name;
      });
    if(it != imports.end())
    {
        // check whether the imports match.
        if(import_path != it->import_path)
        {
            throw codegen_error(fmt::format("Found different paths for name '{}': '{}' and '{}'", name, import_path, it->import_path));
        }

        if(it->type != type)
        {
            throw codegen_error(
              fmt::format("Found different symbol types for import '{}': '{}' and '{}'.",
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

std::size_t context::get_import_index(module_::symbol_type type, std::string import_path, std::string name) const
{
    auto it = std::find_if(
      imports.begin(),
      imports.end(),
      [&name](const imported_symbol& s) -> bool
      {
          return name == s.name;
      });
    if(it != imports.end())
    {
        if(it->import_path != import_path)
        {
            throw codegen_error(fmt::format("Found different paths for name '{}': '{}' and '{}'", name, import_path, it->import_path));
        }

        if(it->type != type)
        {
            throw codegen_error(
              fmt::format("Found different symbol types for import '{}': '{}' and '{}'.",
                          name,
                          slang::module_::to_string(it->type),
                          slang::module_::to_string(type)));
        }

        return std::distance(imports.begin(), it);
    }

    throw codegen_error(
      fmt::format("Symbol '{}' of type '{}' with path '{}' not found in imports.",
                  name,
                  slang::module_::to_string(type),
                  import_path));
}

struct_* context::add_struct(std::string name,
                             std::vector<std::pair<std::string, value>> members,
                             std::uint8_t flags,
                             std::optional<std::string> import_path)
{
    if(std::find_if(
         types.begin(),
         types.end(),
         [&name](const std::unique_ptr<struct_>& t) -> bool
         {
             return t->get_name() == name;
         })
       != types.end())
    {
        throw codegen_error(fmt::format("Type '{}' already defined.", name));
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
    auto it = std::find_if(
      types.begin(),
      types.end(),
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
            throw codegen_error(fmt::format("Type '{}' from import '{}' not found.", name, *import_path));
        }

        throw codegen_error(fmt::format("Type '{}' not found.", name));
    }

    return it->get();
}

/** Same as `std::false_type`, but taking a parameter argument. */
template<typename T>
struct false_type : public std::false_type
{
};

/** Helper to map types to `module_::constant_type` values. */
template<typename T>
struct constant_type_mapper
{
    static_assert(false_type<T>::value, "Invalid constant type.");
};

/** Map `std::int32_t` to `module_::constant_type::i32`. */
template<>
struct constant_type_mapper<std::int32_t>
{
    static constexpr module_::constant_type value = module_::constant_type::i32;
};

/** Map `float` to `module_::constant_type::f32`. */
template<>
struct constant_type_mapper<float>
{
    static constexpr module_::constant_type value = module_::constant_type::f32;
};

/** Map `std::string` to `module_::constant_type::str`. */
template<>
struct constant_type_mapper<std::string>
{
    static constexpr module_::constant_type value = module_::constant_type::str;
};

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
  T value,
  std::optional<std::string> import_path)
{
    if(import_path.has_value())
    {
        // add constant to imported constants table.
        auto it = std::find_if(
          imported_constants.begin(),
          imported_constants.end(),
          [&name, &import_path](const constant_table_entry& entry) -> bool
          {
              return entry.name.has_value() && *entry.name == name && entry.import_path == import_path;
          });
        if(it != imported_constants.end())
        {
            throw codegen_error(fmt::format("Imported constant with name '{}' already exists.", name));
        }

        imported_constants.emplace_back(
          constant_type_mapper<T>::value,
          std::move(value),
          std::move(import_path),
          std::move(name));
    }
    else
    {
        // add constant to constants table.
        auto it = std::find_if(
          module_constants.begin(),
          module_constants.end(),
          [&name](const constant_table_entry& entry) -> bool
          {
              return entry.name.has_value() && *entry.name == name;
          });
        if(it != module_constants.end())
        {
            throw codegen_error(fmt::format("Constant with name '{}' already exists.", name));
        }

        module_constants.emplace_back(
          constant_type_mapper<T>::value,
          std::move(value),
          std::move(import_path),
          std::move(name),
          true);
    }
}

void context::add_constant(std::string name, std::int32_t i, std::optional<std::string> import_path)
{
    slang::codegen::add_constant(constants, imported_constants, name, i, import_path);
}

void context::add_constant(std::string name, float f, std::optional<std::string> import_path)
{
    slang::codegen::add_constant(constants, imported_constants, name, f, import_path);
}

void context::add_constant(std::string name, std::string s, std::optional<std::string> import_path)
{
    slang::codegen::add_constant(constants, imported_constants, name, std::move(s), import_path);
}

std::size_t context::get_string(std::string str)
{
    auto it = std::find_if(
      constants.begin(),
      constants.end(),
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
    auto it = std::find_if(
      constants.cbegin(),
      constants.cend(),
      [&name, &import_path](const constant_table_entry& entry) -> bool
      {
          return entry.name == name && entry.import_path == import_path;
      });
    if(it != constants.cend())
    {
        return *it;
    }

    it = std::find_if(
      imported_constants.cbegin(),
      imported_constants.cend(),
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
    if(std::find_if(
         prototypes.begin(),
         prototypes.end(),
         [&name, &import_path](const std::unique_ptr<prototype>& p) -> bool
         {
             return p->get_name() == name
                    && p->get_import_path() == import_path;
         })
       != prototypes.end())
    {
        throw codegen_error(fmt::format("Prototype '{}' already defined.", name));
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
    auto it = std::find_if(
      prototypes.begin(),
      prototypes.end(),
      [&name, &import_path](const std::unique_ptr<prototype>& p) -> bool
      {
          return p->get_name() == name
                 && p->get_import_path() == import_path;
      });
    if(it == prototypes.end())
    {
        if(import_path.has_value())
        {
            throw codegen_error(fmt::format("Prototype '{}' not found in '{}'.", name, *import_path));
        }
        throw codegen_error(fmt::format("Prototype '{}' not found.", name));
    }
    return **it;
}

function* context::create_function(std::string name,
                                   std::unique_ptr<value> return_type,
                                   std::vector<std::unique_ptr<value>> args)
{
    if(std::find_if(
         funcs.begin(),
         funcs.end(),
         [&name](const std::unique_ptr<function>& fn) -> bool
         {
             return fn->get_name() == name;
         })
       != funcs.end())
    {
        throw codegen_error(fmt::format("Function '{}' already defined.", name));
    }

    return funcs.emplace_back(std::make_unique<function>(std::move(name), std::move(return_type), std::move(args))).get();
}

void context::create_native_function(std::string lib_name,
                                     std::string name,
                                     std::unique_ptr<value> return_type,
                                     std::vector<std::unique_ptr<value>> args)
{
    if(std::find_if(
         funcs.begin(),
         funcs.end(),
         [&name](const std::unique_ptr<function>& fn) -> bool
         {
             return fn->get_name() == name;
         })
       != funcs.end())
    {
        throw codegen_error(fmt::format("Function '{}' already defined.", name));
    }

    funcs.emplace_back(std::make_unique<function>(std::move(lib_name), std::move(name), std::move(return_type), std::move(args)));
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
    if(struct_access.size() > 0)
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
    if(struct_access.size() == 0)
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
    auto& members = s->get_struct(struct_name, import_path);

    auto it = std::find_if(
      members.begin(),
      members.end(),
      [&member_name](const std::pair<std::string, value>& v)
      {
          return v.first == member_name;
      });
    if(it == members.end())
    {
        throw codegen_error(
          loc,
          fmt::format("Struct '{}' does not contain a field with name '{}'.",
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
    return constant_expressions.find(&expr) != constant_expressions.end();
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
    return expression_values.find(&expr) != expression_values.end();
}

/*
 * Code generation.
 */

void context::generate_arraylength()
{
    validate_insertion_point();
    insertion_point->add_instruction(std::make_unique<instruction>("arraylength"));
}

void context::generate_binary_op(binary_op op, value op_type)
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

void context::generate_const(value vt, std::variant<int, float, std::string> v)
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
        throw codegen_error(fmt::format("Depth in dup instruction exceeds maximum value ({} >= {}).", vals.size(), std::numeric_limits<std::int32_t>::max()));
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

void context::generate_new(value vt)
{
    validate_insertion_point();
    std::vector<std::unique_ptr<argument>> args;
    args.emplace_back(std::make_unique<type_argument>(vt));
    insertion_point->add_instruction(std::make_unique<instruction>("new", std::move(args)));
}

void context::generate_newarray(value vt)
{
    validate_insertion_point();
    std::vector<std::unique_ptr<argument>> args;
    args.emplace_back(std::make_unique<type_argument>(vt));
    insertion_point->add_instruction(std::make_unique<instruction>("newarray", std::move(args)));
}

void context::generate_anewarray(value vt)
{
    validate_insertion_point();
    std::vector<std::unique_ptr<argument>> args;
    args.emplace_back(std::make_unique<type_argument>(vt));
    insertion_point->add_instruction(std::make_unique<instruction>("anewarray", std::move(args)));
}

void context::generate_pop(value vt)
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
    return fmt::format("{}", label_count - 1);
}

std::string context::to_string() const
{
    std::string buf;

    // constants.
    if(constants.size())
    {
        auto make_printable = [](const std::string& s) -> std::string
        {
            std::string str;

            // replace non-printable characters by their character codes.
            for(auto& c: s)
            {
                if(!isalnum(c) && c != ' ')
                {
                    str += fmt::format("\\x{:02x}", c);
                }
                else
                {
                    str += c;
                }
            }

            return str;
        };

        auto print_constant = [&buf, &make_printable](std::size_t i, const module_::constant_table_entry& c, bool newline = true) -> void
        {
            if(c.type == module_::constant_type::i32)
            {
                buf += fmt::format(".i32 @{} {}", i, std::get<std::int32_t>(c.data));
            }
            else if(c.type == module_::constant_type::f32)
            {
                buf += fmt::format(".f32 @{} {}", i, std::get<float>(c.data));
            }
            else if(c.type == module_::constant_type::str)
            {
                buf += fmt::format(".string @{} \"{}\"", i, make_printable(std::get<std::string>(c.data)));
            }
            else
            {
                buf += fmt::format(".<unknown> @{}", i);
            }

            if(newline)
            {
                buf += "\n";
            }
        };

        for(std::size_t i = 0; i < constants.size() - 1; ++i)
        {
            print_constant(i, constants[i]);
        }
        print_constant(constants.size() - 1, constants.back(), false);

        // don't append a newline if the constant table is the only non-empty buffer.
        if(types.size() != 0 || funcs.size() != 0)
        {
            buf += "\n";
        }
    }

    // types.
    if(types.size() != 0)
    {
        for(std::size_t i = 0; i < types.size() - 1; ++i)
        {
            buf += fmt::format("{}\n", types[i]->to_string());
        }
        buf += fmt::format("{}", types.back()->to_string());

        // don't append a newline if there are no functions.
        if(funcs.size() != 0)
        {
            buf += "\n";
        }
    }

    // functions.
    if(funcs.size() != 0)
    {
        for(std::size_t i = 0; i < funcs.size() - 1; ++i)
        {
            buf += fmt::format("{}\n", funcs[i]->to_string());
        }
        buf += fmt::format("{}", funcs.back()->to_string());
    }

    return buf;
}

}    // namespace slang::codegen
