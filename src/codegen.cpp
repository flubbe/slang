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

#include "codegen.h"

namespace slang::codegen
{

/*
 * Binary operators.
 */

std::string to_string(binary_op op)
{
    std::array<std::string, 9> strs = {
      "add", "sub", "mul", "div", "mod", "and", "or", "shl", "shr"};

    std::size_t idx = static_cast<std::size_t>(op);
    if(idx < 0 || idx >= strs.size())
    {
        throw codegen_error("Invalid operator index in to_string(binary_op).");
    }

    return strs[idx];
}

/*
 * Context.
 */

type* context::create_type(std::string name, std::vector<std::pair<std::string, std::string>> members)
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

function* context::create_function(std::string name, std::string return_type, std::vector<std::unique_ptr<variable>> args)
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
 * Code generation.
 */

void context::generate_binary_op(binary_op op, value op_type)
{
    validate_insertion_point();
    std::vector<std::unique_ptr<argument>> args;
    args.emplace_back(std::make_unique<type_argument>(op_type));
    insertion_point->add_instruction(std::make_unique<instruction>(codegen::to_string(op), std::move(args)));
}

void context::generate_branch(std::unique_ptr<label_argument> label)
{
    validate_insertion_point();
    std::vector<std::unique_ptr<argument>> args;
    args.emplace_back(std::move(label));
    insertion_point->add_instruction(std::make_unique<instruction>("jmp", std::move(args)));
}

void context::generate_cmp()
{
    validate_insertion_point();
    insertion_point->add_instruction(std::make_unique<instruction>("cmp"));
}

void context::generate_cond_branch(basic_block* then_block, basic_block* else_block)
{
    validate_insertion_point();
    auto arg0 = std::make_unique<label_argument>(then_block->get_label());
    auto arg1 = std::make_unique<label_argument>(else_block->get_label());
    std::vector<std::unique_ptr<argument>> args;
    args.emplace_back(std::move(arg0));
    args.emplace_back(std::move(arg1));
    insertion_point->add_instruction(std::make_unique<instruction>("ifeq", std::move(args)));
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
        generate_load(std::make_unique<const_argument>(std::get<std::string>(v)));
        return;
    }
    else
    {
        throw codegen_error("Invalid value type for constant.");
    }
    insertion_point->add_instruction(std::make_unique<instruction>("const", std::move(args)));
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

void context::generate_load(std::unique_ptr<argument> arg)
{
    validate_insertion_point();
    arg->register_const(this);
    std::vector<std::unique_ptr<argument>> args;
    args.emplace_back(std::move(arg));
    insertion_point->add_instruction(std::make_unique<instruction>("load", std::move(args)));
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

void context::generate_ret(std::optional<value> arg)
{
    validate_insertion_point();
    if(!arg)
    {
        insertion_point->add_instruction(std::make_unique<instruction>("ret"));
    }
    else
    {
        std::unique_ptr<type_argument> ret_arg = std::make_unique<type_argument>(*arg);
        std::vector<std::unique_ptr<argument>> args;
        args.emplace_back(std::move(ret_arg));
        insertion_point->add_instruction(std::make_unique<instruction>("ret", std::move(args)));
    }
}

void context::generate_store(std::unique_ptr<variable_argument> arg)
{
    validate_insertion_point();
    std::vector<std::unique_ptr<argument>> args;
    args.emplace_back(std::move(arg));
    insertion_point->add_instruction(std::make_unique<instruction>("store", std::move(args)));
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

bool context::ends_with_return() const
{
    validate_insertion_point();
    return insertion_point->ends_with_return();
}

}    // namespace slang::codegen