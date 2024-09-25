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

#include "token.h"
#include "codegen.h"
#include "module.h"
#include "opcodes.h"
#include "utils.h"

namespace slang::codegen
{

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
    std::array<std::string, 18> strs = {
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
    std::array<std::string, 2> strs = {"i32_to_f32", "f32_to_i32"};

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

    is_builtin = (*ty.get_struct_name() == "void")
                 || (*ty.get_struct_name() == "i32")
                 || (*ty.get_struct_name() == "f32")
                 || (*ty.get_struct_name() == "str");
    if(is_builtin)
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
    if(std::find_if(args.begin(), args.end(),
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

    if(std::find_if(locals.begin(), locals.end(),
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
    auto it = std::find_if(structs.begin(), structs.end(),
                           [&name, &import_path](const std::pair<std::string, struct_>& p) -> bool
                           {
                               return p.first == name
                                      && p.second.get_import_path() == import_path;
                           });
    return it != structs.end();
}

value* scope::get_value(const std::string& name)
{
    auto it = std::find_if(args.begin(), args.end(),
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

    it = std::find_if(locals.begin(), locals.end(),
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
    auto it = std::find_if(args.begin(), args.end(),
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
        return it - args.begin();
    }

    it = std::find_if(locals.begin(), locals.end(),
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
        return args.size() + (it - locals.begin());
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

void scope::add_struct(std::string name,
                       std::vector<std::pair<std::string, value>> members,
                       std::optional<std::string> import_path)
{
    auto it = std::find_if(structs.begin(), structs.end(),
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
    structs.insert({name, struct_{std::move(name_copy), std::move(members), import_path}});
}

const std::vector<std::pair<std::string, value>>& scope::get_struct(const std::string& name, std::optional<std::string> import_path) const
{
    auto it = std::find_if(structs.begin(), structs.end(),
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

void context::add_import(symbol_type type, std::string import_path, std::string name)
{
    auto it = std::find_if(imports.begin(), imports.end(),
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
            throw codegen_error(fmt::format("Found different symbol types for import '{}': '{}' and '{}'.", name, slang::to_string(it->type), slang::to_string(type)));
        }
    }
    else
    {
        // add the import.
        imports.emplace_back(type, std::move(name), std::move(import_path));
    }
}

std::size_t context::get_import_index(symbol_type type, std::string import_path, std::string name) const
{
    auto it = std::find_if(imports.begin(), imports.end(),
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
            throw codegen_error(fmt::format("Found different symbol types for import '{}': '{}' and '{}'.", name, slang::to_string(it->type), slang::to_string(type)));
        }

        return std::distance(imports.begin(), it);
    }

    throw codegen_error(fmt::format("Symbol '{}' of type '{}' with path '{}' not found in imports.", name, slang::to_string(type), import_path));
}

struct_* context::add_type(std::string name,
                           std::vector<std::pair<std::string, value>> members,
                           std::optional<std::string> import_path)
{
    if(std::find_if(types.begin(), types.end(),
                    [&name](const std::unique_ptr<struct_>& t) -> bool
                    {
                        return t->get_name() == name;
                    })
       != types.end())
    {
        throw codegen_error(fmt::format("Type '{}' already defined.", name));
    }

    return types.emplace_back(std::make_unique<struct_>(name, std::move(members), std::move(import_path))).get();
}

struct_* context::get_type(const std::string& name, std::optional<std::string> import_path)
{
    auto it = std::find_if(types.begin(), types.end(),
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

std::size_t context::get_string(std::string str)
{
    auto it = std::find(strings.begin(), strings.end(), str);
    if(it != strings.end())
    {
        return it - strings.begin();
    }

    strings.emplace_back(std::move(str));
    return strings.size() - 1;
}

prototype* context::add_prototype(std::string name,
                                  value return_type,
                                  std::vector<value> args, std::optional<std::string> import_path)
{
    if(std::find_if(prototypes.begin(), prototypes.end(),
                    [&name, &import_path](const std::unique_ptr<prototype>& p) -> bool
                    {
                        if(p->get_name() != name)
                        {
                            return false;
                        }
                        if(p->get_import_path().has_value() && import_path.has_value())
                        {
                            return *p->get_import_path() == *import_path;
                        }
                        return !p->get_import_path().has_value() && !import_path.has_value();
                    })
       != prototypes.end())
    {
        throw codegen_error(fmt::format("Prototype '{}' already defined.", name));
    }

    return prototypes.emplace_back(std::make_unique<prototype>(
                                     std::move(name), std::move(return_type), std::move(args), std::move(import_path)))
      .get();
}

const prototype& context::get_prototype(const std::string& name, std::optional<std::string> import_path) const
{
    auto it = std::find_if(prototypes.begin(), prototypes.end(),
                           [&name, &import_path](const std::unique_ptr<prototype>& p) -> bool
                           {
                               if(import_path.has_value())
                               {
                                   return p->get_import_path().has_value()
                                          && *p->get_import_path() == import_path
                                          && p->get_name() == name;
                               }
                               else
                               {
                                   return !p->get_import_path().has_value() && p->get_name() == name;
                               }
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
    if(std::find_if(funcs.begin(), funcs.end(),
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
    if(std::find_if(funcs.begin(), funcs.end(),
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

void context::generate_cmp()
{
    validate_insertion_point();
    insertion_point->add_instruction(std::make_unique<instruction>("cmp"));
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
    array_type = vt;
    validate_insertion_point();
    std::vector<std::unique_ptr<argument>> args;
    args.emplace_back(std::make_unique<type_argument>(vt));
    insertion_point->add_instruction(std::make_unique<instruction>("newarray", std::move(args)));
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

    // strings.
    if(strings.size())
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

        for(std::size_t i = 0; i < strings.size() - 1; ++i)
        {
            buf += fmt::format(".string @{} \"{}\"\n", i, make_printable(strings[i]));
        }
        buf += fmt::format(".string @{} \"{}\"", strings.size() - 1, make_printable(strings.back()));

        // don't append a newline if the string table is the only non-empty buffer.
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