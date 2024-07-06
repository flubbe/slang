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

/*
 * Forward declarations.
 */
namespace slang::typing
{
std::string to_string(const std::pair<std::string, std::optional<std::size_t>>&); /* typing.h */
};

namespace ty = slang::typing;

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
    if(idx < 0 || idx >= strs.size())
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
    if(idx < 0 || idx > strs.size())
    {
        throw codegen_error("Invalid type cast index in to_string(type_cast).");
    }

    return strs[idx];
}

/*
 * Value.
 */

std::string value::to_string() const
{
    if(has_name())
    {
        return fmt::format("{} %{}", ty::to_string({get_resolved_type(), array_size_or_index}), *get_name());
    }
    return fmt::format("{}", ty::to_string({get_resolved_type(), array_size_or_index}));
}

/*
 * Context.
 */

std::size_t context::get_import_index(symbol_type type, std::string import_path, std::string name)
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

    // add the import.
    imports.emplace_back(type, std::move(name), std::move(import_path));
    return imports.size() - 1;
}

type* context::create_type(std::string name, std::vector<std::pair<std::string, value>> members)
{
    if(std::find_if(types.begin(), types.end(),
                    [&name](const std::unique_ptr<type>& t) -> bool
                    {
                        return t->get_name() == name;
                    })
       != types.end())
    {
        throw codegen_error(fmt::format("Type '{}' already defined.", name));
    }

    return types.emplace_back(std::make_unique<type>(std::move(name), std::move(members))).get();
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

const prototype& context::get_prototype(const std::string& name) const
{
    if(resolution_scopes.size() > 0)
    {
        std::string import_path = slang::utils::join(resolution_scopes, "::");
        auto it = std::find_if(prototypes.begin(), prototypes.end(),
                               [&name, &import_path](const std::unique_ptr<prototype>& p) -> bool
                               {
                                   return p->get_import_path().has_value() && *p->get_import_path() == import_path
                                          && p->get_name() == name;
                               });
        if(it == prototypes.end())
        {
            throw codegen_error(fmt::format("Prototype '{}' not found in '{}'.", name, import_path));
        }
        return **it;
    }
    else
    {
        auto it = std::find_if(prototypes.begin(), prototypes.end(),
                               [&name](const std::unique_ptr<prototype>& p) -> bool
                               {
                                   return p->get_name() == name && !p->get_import_path().has_value();
                               });
        if(it == prototypes.end())
        {
            throw codegen_error(fmt::format("Prototype '{}' not found.", name));
        }
        return **it;
    }
}

function* context::create_function(std::string name, value return_type, std::vector<std::unique_ptr<value>> args)
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

    return funcs.emplace_back(std::make_unique<function>(*this, std::move(name), std::move(return_type), std::move(args))).get();
}

void context::create_native_function(std::string lib_name, std::string name, std::string return_type, std::vector<std::unique_ptr<value>> args)
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

    funcs.emplace_back(std::make_unique<function>(*this, std::move(lib_name), std::move(name), std::move(return_type), std::move(args)));
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
 * Name resolution.
 */

void context::push_resolution_scope(std::string name)
{
    resolution_scopes.emplace_back(std::move(name));
}

void context::pop_resolution_scope()
{
    if(resolution_scopes.size() > 0)
    {
        resolution_scopes.pop_back();
    }
    else
    {
        throw codegen_error("Cannot pop from name resolution stack: The stack is empty.");
    }
}

/*
 * Code generation.
 */

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
    auto arg0 = std::make_unique<cast_argument>(codegen::to_string(tc));
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
    if(vt.get_type() == "i32")
    {
        auto arg = std::make_unique<const_argument>(std::get<int>(v));
        args.emplace_back(std::move(arg));
    }
    else if(vt.get_type() == "f32")
    {
        auto arg = std::make_unique<const_argument>(std::get<float>(v));
        args.emplace_back(std::move(arg));
    }
    else if(vt.get_type() == "str")
    {
        auto arg = std::make_unique<const_argument>(std::get<std::string>(v));
        arg->register_const(*this);
        args.emplace_back(std::move(arg));
    }
    else if(vt.get_type() == "fn")
    {
        // Nothing to do.
    }
    else
    {
        throw codegen_error("Invalid value type for constant.");
    }
    insertion_point->add_instruction(std::make_unique<instruction>("const", std::move(args)));
}

void context::generate_dup(value vt)
{
    validate_insertion_point();
    std::vector<std::unique_ptr<argument>> args;
    args.emplace_back(std::make_unique<type_argument>(std::move(vt)));
    insertion_point->add_instruction(std::make_unique<instruction>("dup", std::move(args)));
}

void context::generate_invoke(std::optional<std::unique_ptr<function_argument>> name)
{
    validate_insertion_point();

    if(name.has_value())
    {
        if(resolution_scopes.size() > 0)
        {
            (*name)->set_import_path(slang::utils::join(resolution_scopes, "::"));
        }

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

void context::generate_load_element(std::vector<int> indices)
{
    validate_insertion_point();
    std::vector<std::unique_ptr<argument>> args;
    for(auto i: indices)
    {
        args.emplace_back(std::make_unique<const_argument>(i));
    }
    insertion_point->add_instruction(std::make_unique<instruction>("load_element", std::move(args)));
}

void context::generate_newarray(value vt)
{
    array_type = vt;
    validate_insertion_point();
    std::vector<std::unique_ptr<argument>> args;
    args.emplace_back(std::make_unique<type_argument>(vt.deref()));
    insertion_point->add_instruction(std::make_unique<instruction>("newarray", std::move(args)));
}

void context::generate_pop(value vt)
{
    validate_insertion_point();
    std::vector<std::unique_ptr<argument>> args;

    if(vt.is_aggregate())
    {
        throw codegen_error("Cannot generate pop instruction for aggregate type.");
    }

    args.emplace_back(std::make_unique<type_argument>(vt));
    insertion_point->add_instruction(std::make_unique<instruction>("pop", std::move(args)));
}

void context::generate_ret(std::optional<value> arg)
{
    validate_insertion_point();
    std::vector<std::unique_ptr<argument>> args;
    if(!arg)
    {
        args.emplace_back(std::make_unique<type_argument>(value{"void"}));
    }
    else
    {
        args.emplace_back(std::make_unique<type_argument>(*arg));
    }
    insertion_point->add_instruction(std::make_unique<instruction>("ret", std::move(args)));
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

void context::generate_store_element(std::vector<int> indices)
{
    validate_insertion_point();
    std::vector<std::unique_ptr<argument>> args;
    for(auto i: indices)
    {
        args.emplace_back(std::make_unique<const_argument>(i));
    }
    insertion_point->add_instruction(std::make_unique<instruction>("store_element", std::move(args)));
}

std::string context::generate_label()
{
    ++label_count;
    return fmt::format("{}", label_count - 1);
}

}    // namespace slang::codegen