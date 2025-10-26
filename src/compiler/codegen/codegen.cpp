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

#include "compiler/ast/ast.h"
#include "compiler/typing.h"
#include "shared/module.h"
#include "shared/opcodes.h"
#include "shared/type_utils.h"
#include "codegen.h"
#include "loader.h"
#include "utils.h"

namespace slang::codegen
{

namespace ty = slang::typing;
namespace ld = slang::loader;

/*
 * Exceptions.
 */

codegen_error::codegen_error(const source_location& loc, const std::string& message)
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
    case type_kind::void_: return "void";
    case type_kind::null: return "null";
    case type_kind::i32: return "i32";
    case type_kind::f32: return "f32";
    case type_kind::str: return "str";
    case type_kind::ref: return "ref";
    default:
        return "<unknown>";
    }
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

std::string type::to_string([[maybe_unused]] const name_resolver* resolver) const
{
    static const std::unordered_map<type_kind, std::string> map = {
      {type_kind::void_, "void"},
      {type_kind::null, "null"},
      {type_kind::i32, "i32"},
      {type_kind::f32, "f32"},
      {type_kind::str, "str"},
      {type_kind::ref, "ref"}};

    auto it = map.find(back_end_type);
    if(it != map.end())
    {
        if(type_id.has_value()
           && resolver != nullptr)
        {
            return std::format(
              "{}",
              it->second,
              resolver->type_name(type_id.value()));
        }

        return it->second;
    }

    throw std::runtime_error("Invalid type kind.");
}

/*
 * value.
 */

std::string value::to_string([[maybe_unused]] const name_resolver* resolver) const
{
    // named values.
    if(name.has_value())
    {
        // resolve front-end type, if possible.
        if(ty.get_type_id().has_value()
           && resolver != nullptr)
        {
            return std::format(
              "{} %{}    ; {}",
              ty.to_string(),
              name.value(),
              resolver->type_name(ty.get_type_id().value()));
        }

        return std::format("{} %{}", ty.to_string(), name.value());
    }

    // resolve front-end type, if possible.
    if(ty.get_type_id().has_value()
       && resolver != nullptr)
    {
        return std::format(
          "{}    ; {}",
          ty.to_string(),
          resolver->type_name(ty.get_type_id().value()));
    }

    return std::format("{}", ty.to_string());
}

/*
 * const_argument.
 */

std::string const_argument::to_string(const name_resolver* resolver) const
{
    auto kind = v->get_type().get_type_kind();

    if(kind == type_kind::i32)
    {
        if(resolver)
        {
            return std::format(
              "{}",
              static_cast<constant_i32*>(v.get())->get_int());
        }

        return std::format(
          "i32 {}",
          static_cast<constant_i32*>(v.get())->get_int());    // NOLINT(cppcoreguidelines-pro-type-static-cast-downcast)
    }

    if(kind == type_kind::f32)
    {
        if(resolver)
        {
            return std::format(
              "{}",
              static_cast<constant_f32*>(v.get())->get_float());
        }

        return std::format(
          "f32 {}",
          static_cast<constant_f32*>(v.get())->get_float());    // NOLINT(cppcoreguidelines-pro-type-static-cast-downcast)
    }

    if(kind == type_kind::str)
    {
        const auto* v_ptr = static_cast<const constant_str*>(v.get());    // NOLINT(cppcoreguidelines-pro-type-static-cast-downcast)
        std::string s = v_ptr->to_string();

        if(resolver == nullptr)
        {
            return std::format(
              "str {}",
              s);
        }

        // resolve the string to its value and truncate after 10 characters.
        std::string resolved_s = resolver->constant(v_ptr->get_id());

        // TODO Handle escape sequences in the string.

        // We get a possibly longer string from the resolver.
        if(resolved_s.length() > 10)    // NOLINT(readability-magic-numbers)
        {
            resolved_s = std::format(
              "{}...",
              resolved_s.substr(0, 10));    // NOLINT(readability-magic-numbers)
        }

        return std::format(
          "{}",
          resolved_s);
    }

    throw codegen_error(std::format("Unrecognized const_argument type."));
}

/*
 * instruction.
 */

std::string instruction::to_string(const name_resolver* resolver) const
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

        if(resolver != nullptr)
        {
            std::string resolved_args;

            for(std::size_t i = 0; i < args.size() - 1; ++i)
            {
                resolved_args += args[i]->to_string(resolver) + ", ";
            }
            resolved_args += args.back()->to_string(resolver);

            if(!resolved_args.empty())
            {
                buf += std::format("    ; {}", resolved_args);
            }
        }
    }
    return std::format("{}{}", name, buf);
}

/*
 * basic_block.
 */

std::string basic_block::to_string(const name_resolver* resolver) const
{
    if(instrs.empty())
    {
        return std::format("{}:", label);
    }

    std::string buf = std::format("{}:\n", label);
    for(std::size_t i = 0; i < instrs.size() - 1; ++i)
    {
        buf += std::format(" {}\n", instrs[i]->to_string(resolver));
    }
    buf += std::format(" {}", instrs.back()->to_string(resolver));
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
         [&name](const std::pair<source_location, std::unique_ptr<value>>& v) -> bool
         {
             if(!v.second->has_name())
             {
                 throw codegen_error("Scope contains unnamed value.");
             }

             return *v.second->get_name() == name;
         })
       != args.cend())
    {
        return true;
    }

    if(std::ranges::find_if(
         locals,
         [&name](const std::pair<source_location, std::unique_ptr<value>>& v) -> bool
         {
             if(!v.second->has_name())
             {
                 throw codegen_error("Scope contains unnamed value.");
             }

             return *v.second->get_name() == name;
         })
       != locals.cend())
    {
        return true;
    }

    return false;
}

value* scope::get_value(const std::string& name)
{
    auto it = std::ranges::find_if(
      args,
      [&name](const std::pair<source_location, std::unique_ptr<value>>& v) -> bool
      {
          if(!v.second->has_name())
          {
              throw codegen_error("Scope contains unnamed value.");
          }

          return *v.second->get_name() == name;
      });
    if(it != args.end())
    {
        return it->second.get();
    }

    it = std::ranges::find_if(
      locals,
      [&name](const std::pair<source_location, std::unique_ptr<value>>& v) -> bool
      {
          if(!v.second->has_name())
          {
              throw codegen_error("Scope contains unnamed value.");
          }

          return *v.second->get_name() == name;
      });

    if(it != locals.end())
    {
        return it->second.get();
    }

    return nullptr;
}

std::size_t scope::get_index(const std::string& name) const
{
    auto it = std::ranges::find_if(
      args,
      [&name](const std::pair<source_location, std::unique_ptr<value>>& v) -> bool
      {
          if(!v.second->has_name())
          {
              throw codegen_error("Scope contains unnamed value.");
          }

          return *v.second->get_name() == name;
      });
    if(it != args.cend())
    {
        return std::distance(args.cbegin(), it);
    }

    it = std::ranges::find_if(
      locals,
      [&name](const std::pair<source_location, std::unique_ptr<value>>& v) -> bool
      {
          if(!v.second->has_name())
          {
              throw codegen_error("Scope contains unnamed value.");
          }

          return *v.second->get_name() == name;
      });

    if(it != locals.cend())
    {
        return args.size() + std::distance(locals.cbegin(), it);
    }

    throw codegen_error(std::format("Name '{}' not found in scope.", name));
}

void scope::add_local(
  source_location loc,
  std::unique_ptr<value> arg)
{
    if(!arg->has_name())
    {
        throw codegen_error("Cannot add unnamed argument to scope.");
    }

    if(contains(arg->get_name().value()))    // NOLINT(bugprone-unchecked-optional-access)
    {
        throw codegen_error(
          std::format(
            "{}: Name '{}' already contained in scope.",
            slang::to_string(loc),
            arg->get_name().value()));    // NOLINT(bugprone-unchecked-optional-access)
    }
    locals.emplace_back(
      std::make_pair(
        loc,
        std::move(arg)));
}

/*
 * function.
 */

basic_block* function::remove_basic_block(const std::string& label)
{
    auto it = std::ranges::find_if(
      instr_blocks,
      [&label](const basic_block* bb) -> bool
      {
          return bb->get_label() == label;
      });

    if(it == instr_blocks.end())
    {
        throw codegen_error(
          std::format(
            "Function '{}': Cannot remove basic block with label '{}': Label not found.",
            get_name(),
            label));
    }

    basic_block* bb = *it;
    instr_blocks.erase(it);

    return bb;
}

std::string function::to_string(const name_resolver* resolver) const
{
    std::string buf;
    if(native)
    {
        buf = std::format(
          "native ({}) {} @{}(",
          import_library,
          return_type->to_string(),
          name);
    }
    else
    {
        buf = std::format(
          "define {} @{}(",
          return_type->to_string(),
          name);
    }

    const auto& args = scope.get_args();
    if(!args.empty())
    {
        for(std::size_t i = 0; i < args.size() - 1; ++i)
        {
            buf += std::format(
              "{}, ",
              args[i].second->to_string());    // no resolver; only print lowered type.
        }
        buf += std::format(
          "{})",
          args.back().second->to_string());    // no resolver; only print lowered type.
    }
    else
    {
        buf += ")";
    }

    if(!native)
    {
        bool all_type_ids_exist = return_type->get_type().get_type_id().has_value();
        for(const auto& arg: args)
        {
            all_type_ids_exist &= arg.second->get_type().get_type_id().has_value();
        }

        if(resolver != nullptr
           && all_type_ids_exist)
        {
            buf += std::format(
              " {{    ; {} (",
              resolver->type_name(return_type->get_type().get_type_id().value()));

            if(!args.empty())
            {
                for(std::size_t i = 0; i < args.size() - 1; ++i)
                {
                    buf += std::format(
                      "{}, ",
                      resolver->type_name(args[i].second->get_type().get_type_id().value()));
                }
                buf += std::format(
                  "{}",
                  resolver->type_name(args.back().second->get_type().get_type_id().value()));
            }

            buf += ")\n";
        }
        else
        {
            buf += " {\n";
        }

        for(const auto& [_, v]: scope.get_locals())
        {
            buf += std::format(
              "local {}\n",
              v->to_string(resolver));
        }
        for(const auto& b: instr_blocks)
        {
            buf += std::format(
              "{}\n",
              b->to_string(resolver));
        }
        buf += "}";
    }

    return buf;
}

/*
 * context.
 */

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

bool context::has_registered_constant_name(const std::string& name)
{
    // TODO
    throw std::runtime_error("context::has_registered_constant_name");
}

std::size_t context::get_string(std::string str)
{
    // TODO
    throw std::runtime_error("constext::get_string");

    // auto it = std::ranges::find_if(
    //   constants,
    //   [&str](const module_::constant_table_entry& t) -> bool
    //   {
    //       return t.type == module_::constant_type::str && std::get<std::string>(t.data) == str;
    //   });
    // if(it != constants.end())
    // {
    //     it->import_path = std::nullopt;
    //     return std::distance(constants.begin(), it);
    // }

    // constants.emplace_back(module_::constant_type::str, std::move(str));
    // return constants.size() - 1;
}

std::optional<constant_table_entry> context::get_constant(
  const std::string& name,
  const std::optional<std::string>& import_path)
{
    throw std::runtime_error("context::get_constant");

    // /*
    //  * First try to find the constant in the module's constant table.
    //  * If not found, search the import table and copy the constant into
    //  * the import table.
    //  */
    // auto it = std::ranges::find_if(
    //   constants,
    //   [&name, &import_path](const constant_table_entry& entry) -> bool
    //   {
    //       return entry.name == name && entry.import_path == import_path;
    //   });
    // if(it != constants.cend())
    // {
    //     return *it;
    // }

    // it = std::ranges::find_if(
    //   imported_constants,
    //   [&name, &import_path](const constant_table_entry& entry) -> bool
    //   {
    //       return entry.name == name && entry.import_path == import_path;
    //   });
    // if(it != imported_constants.end())
    // {
    //     // copy string constants to constant table.
    //     if(it->type == module_::constant_type::str)
    //     {
    //         return constants.emplace_back(*it);
    //     }

    //     // return primitive constant.
    //     return *it;
    // }

    // return std::nullopt;
}

function* context::create_function(
  std::string name,
  std::unique_ptr<value> return_type,
  std::vector<
    std::pair<
      source_location,
      std::unique_ptr<value>>>
    args)
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

    return funcs.emplace_back(
                  std::make_unique<function>(
                    std::move(name),
                    std::move(return_type),
                    std::move(args)))
      .get();
}

void context::create_native_function(
  std::string lib_name,
  std::string name,
  std::unique_ptr<value> return_type,
  std::vector<
    std::pair<
      source_location,
      std::unique_ptr<value>>>
    args)
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
 * Code generation.
 */

void context::generate_arraylength()
{
    validate_insertion_point();
    insertion_point->add_instruction(std::make_unique<instruction>("arraylength"));
}

void context::generate_binary_op(binary_op op, const type& op_type)
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
    auto arg0 = std::make_unique<type_argument>(target_type);
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

void context::generate_const(
  const type& vt,
  std::variant<int, float, const_::constant_id> v)
{
    validate_insertion_point();
    std::vector<std::unique_ptr<argument>> args;
    if(vt.get_type_kind() == type_kind::i32)
    {
        auto arg = std::make_unique<const_argument>(
          std::get<int>(v),
          std::nullopt);
        args.emplace_back(std::move(arg));
    }
    else if(vt.get_type_kind() == type_kind::f32)
    {
        auto arg = std::make_unique<const_argument>(
          std::get<float>(v),
          std::nullopt);
        args.emplace_back(std::move(arg));
    }
    else if(vt.get_type_kind() == type_kind::str)
    {
        auto arg = std::make_unique<const_argument>(
          std::get<const_::constant_id>(v),
          std::nullopt);
        args.emplace_back(std::move(arg));
    }
    else
    {
        throw codegen_error(
          std::format(
            "Invalid type kind '{}' for constant.",
            ::slang::codegen::to_string(
              vt.get_type_kind())));
    }
    insertion_point->add_instruction(
      std::make_unique<instruction>(
        "const",
        std::move(args)));
}

void context::generate_const_null()
{
    validate_insertion_point();
    insertion_point->add_instruction(std::make_unique<instruction>("const_null", std::vector<std::unique_ptr<argument>>{}));
}

void context::generate_dup(type vt)
{
    validate_insertion_point();
    std::vector<std::unique_ptr<argument>> args;
    args.emplace_back(
      std::make_unique<stack_value_argument>(
        lowering_ctx.get_stack_value(vt)));
    insertion_point->add_instruction(
      std::make_unique<instruction>(
        "dup",
        std::move(args)));
}

void context::generate_dup_x1(type vt, type skip_type)
{
    validate_insertion_point();
    std::vector<std::unique_ptr<argument>> args;
    args.emplace_back(
      std::make_unique<stack_value_argument>(
        lowering_ctx.get_stack_value(vt)));
    args.emplace_back(
      std::make_unique<stack_value_argument>(
        lowering_ctx.get_stack_value(skip_type)));
    insertion_point->add_instruction(
      std::make_unique<instruction>(
        "dup_x1",
        std::move(args)));
}

void context::generate_dup_x2(type vt, type skip_type1, type skip_type2)
{
    validate_insertion_point();
    std::vector<std::unique_ptr<argument>> args;
    args.emplace_back(
      std::make_unique<stack_value_argument>(
        lowering_ctx.get_stack_value(vt)));
    args.emplace_back(
      std::make_unique<stack_value_argument>(
        lowering_ctx.get_stack_value(skip_type1)));
    args.emplace_back(
      std::make_unique<stack_value_argument>(
        lowering_ctx.get_stack_value(skip_type2)));
    insertion_point->add_instruction(
      std::make_unique<instruction>(
        "dup_x2",
        std::move(args)));
}

void context::generate_get_field(std::unique_ptr<field_access_argument> arg)
{
    validate_insertion_point();

    std::vector<std::unique_ptr<argument>> args;
    args.emplace_back(std::move(arg));

    insertion_point->add_instruction(
      std::make_unique<instruction>(
        "get_field",
        std::move(args)));
}

void context::generate_invoke(function_argument f)
{
    validate_insertion_point();

    std::vector<std::unique_ptr<argument>> args;
    args.emplace_back(std::make_unique<function_argument>(f));
    insertion_point->add_instruction(
      std::make_unique<instruction>(
        "invoke",
        std::move(args)));
}

void context::generate_invoke_dynamic()
{
    validate_insertion_point();

    insertion_point->add_instruction(
      std::make_unique<instruction>(
        "invoke_dynamic"));
}

void context::generate_load(variable_argument v)
{
    validate_insertion_point();
    std::vector<std::unique_ptr<argument>> args;
    args.emplace_back(
      std::make_unique<variable_argument>(
        std::move(v)));
    insertion_point->add_instruction(
      std::make_unique<instruction>(
        "load",
        std::move(args)));
}

void context::generate_load_element(type_argument t)
{
    validate_insertion_point();
    std::vector<std::unique_ptr<argument>> args;
    args.emplace_back(std::make_unique<type_argument>(t));
    insertion_point->add_instruction(
      std::make_unique<instruction>(
        "load_element",
        std::move(args)));
}

void context::generate_new(const type& t)
{
    validate_insertion_point();
    std::vector<std::unique_ptr<argument>> args;
    args.emplace_back(std::make_unique<type_argument>(t));
    insertion_point->add_instruction(std::make_unique<instruction>("new", std::move(args)));
}

void context::generate_newarray(const type& t)
{
    validate_insertion_point();
    std::vector<std::unique_ptr<argument>> args;
    args.emplace_back(std::make_unique<type_argument>(t));
    insertion_point->add_instruction(std::make_unique<instruction>("newarray", std::move(args)));
}

void context::generate_anewarray(const type& t)
{
    validate_insertion_point();
    std::vector<std::unique_ptr<argument>> args;
    args.emplace_back(std::make_unique<type_argument>(t));
    insertion_point->add_instruction(std::make_unique<instruction>("anewarray", std::move(args)));
}

void context::generate_pop(const type& t)
{
    validate_insertion_point();
    std::vector<std::unique_ptr<argument>> args;

    args.emplace_back(std::make_unique<type_argument>(t));
    insertion_point->add_instruction(std::make_unique<instruction>("pop", std::move(args)));
}

void context::generate_ret(std::optional<type> arg)
{
    validate_insertion_point();
    std::vector<std::unique_ptr<argument>> args;
    if(!arg)
    {
        args.emplace_back(std::make_unique<type_argument>(type{type_kind::void_}));
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

void context::generate_store(variable_argument v)
{
    validate_insertion_point();
    std::vector<std::unique_ptr<argument>> args;
    args.emplace_back(
      std::make_unique<variable_argument>(
        std::move(v)));
    insertion_point->add_instruction(
      std::make_unique<instruction>(
        "store",
        std::move(args)));
}

void context::generate_store_element(type_argument t)
{
    validate_insertion_point();
    std::vector<std::unique_ptr<argument>> args;
    args.emplace_back(
      std::make_unique<type_argument>(
        std::move(t)));
    insertion_point->add_instruction(
      std::make_unique<instruction>(
        "store_element",
        std::move(args)));
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
 * @param id The constant id.
 * @param info The constant info.
 * @returns A string containing the constant's information.
 */
static std::string print_constant(
  const_::constant_id id,
  const const_::const_info& info)
{
    std::string buf;

    if(info.type == const_::constant_type::i32)
    {
        buf += std::format(".i32 @{} {}", id, std::get<int>(info.value));
    }
    else if(info.type == const_::constant_type::f32)
    {
        buf += std::format(".f32 @{} {}", id, std::get<float>(info.value));
    }
    else if(info.type == const_::constant_type::str)
    {
        buf += std::format(".string @{} \"{}\"", id, make_printable(std::get<std::string>(info.value)));
    }
    else
    {
        buf += std::format(".<unknown> @{}", id);
    }

    return buf;
};

std::string context::to_string(const name_resolver* resolver) const
{
    std::string buf;

    // constant literals.
    if(!const_env.const_literal_map.empty())
    {
        for(const auto& [id, info]: const_env.const_literal_map)
        {
            buf += std::format("{}\n", print_constant(id, info));
        }

        // don't append a newline if the constant table is the only non-empty buffer.
        if(funcs.empty())
        {
            buf.pop_back();
        }
    }

    // FIXME This prints the type definitions, but we likely
    //       don't want to go through the type lowering context
    //       (but maybe instead have a small facade to the type context
    //       or print the type definitions somewhere else entirely)
    buf += lowering_ctx.to_string();

    // functions.
    if(!funcs.empty())
    {
        for(std::size_t i = 0; i < funcs.size() - 1; ++i)
        {
            buf += std::format("{}\n", funcs[i]->to_string(resolver));
        }
        buf += std::format("{}", funcs.back()->to_string(resolver));
    }

    return buf;
}

}    // namespace slang::codegen
