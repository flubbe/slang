/**
 * slang - a simple scripting language.
 *
 * instruction emitter.
 *
 * \author Felix Lubbe
 * \copyright Copyright (c) 2024
 * \license Distributed under the MIT software license (see accompanying LICENSE.txt).
 */

#include <set>
#include <stdexcept>
#include <unordered_map>

#include "codegen.h"
#include "emitter.h"
#include "module.h"
#include "opcodes.h"

namespace cg = slang::codegen;

namespace slang
{

/** Helper function for opcode emission. */
inline void emit(archive& ar, opcode op)
{
    ar & op;
}

/** Helper function for opcode emission. */
template<typename T>
void emit(archive& ar, opcode op, T arg)
{
    ar & op;
    ar & arg;
}

/*
 * export_table_builder implementation.
 */

void export_table_builder::add_function(
  const std::string& name,
  std::pair<std::string, bool> return_type,
  std::vector<std::pair<std::string, bool>> arg_types)
{
    if(std::find_if(export_table.begin(), export_table.end(),
                    [&name](const module_::exported_symbol& entry) -> bool
                    { return entry.type == module_::symbol_type::function
                             && entry.name == name; })
       != export_table.end())
    {
        throw emitter_error(fmt::format("Cannot add function to export table: '{}' already exists.", name));
    }

    export_table.emplace_back(
      module_::symbol_type::function,
      name,
      module_::function_descriptor{
        module_::function_signature{
          std::move(return_type),
          std::move(arg_types)},
        false,
        module_::function_details{} /* dummy details */
      });
}

void export_table_builder::update_function(
  const std::string& name,
  std::size_t size,
  std::size_t offset,
  std::vector<module_::variable_descriptor> locals)
{
    auto it = std::find_if(export_table.begin(), export_table.end(),
                           [&name](const module_::exported_symbol& entry) -> bool
                           { return entry.type == module_::symbol_type::function
                                    && entry.name == name; });
    if(it == export_table.end())
    {
        throw emitter_error(fmt::format("Cannot update function to export table: '{}' not found.", name));
    }

    auto& desc = std::get<module_::function_descriptor>(it->desc);
    auto& details = std::get<module_::function_details>(desc.details);

    details.size = size;
    details.offset = offset;
    details.locals = std::move(locals);
}

void export_table_builder::add_native_function(
  const std::string& name,
  std::pair<std::string, bool> return_type,
  std::vector<std::pair<std::string, bool>> arg_types,
  std::string import_library)
{
    if(std::find_if(export_table.begin(), export_table.end(),
                    [&name](const module_::exported_symbol& entry) -> bool
                    { return entry.type == module_::symbol_type::function
                             && entry.name == name; })
       != export_table.end())
    {
        throw emitter_error(fmt::format("Cannot add function to export table: '{}' already exists.", name));
    }

    export_table.emplace_back(
      module_::symbol_type::function,
      name,
      module_::function_descriptor{
        module_::function_signature{
          std::move(return_type),
          std::move(arg_types)},
        true,
        module_::native_function_details{
          std::move(import_library)}});
}

void export_table_builder::add_type(const cg::context& ctx, const std::unique_ptr<cg::struct_>& type)
{
    if(std::find_if(export_table.begin(), export_table.end(),
                    [&type](const module_::exported_symbol& entry) -> bool
                    { return entry.type == module_::symbol_type::type
                             && entry.name == type->get_name(); })
       != export_table.end())
    {
        throw emitter_error(fmt::format("Cannot add function to export table: '{}' already exists.", type->get_name()));
    }

    auto members = type->get_members();
    std::vector<std::pair<std::string, module_::field_descriptor>> transformed_members;

    std::transform(members.cbegin(), members.cend(), std::back_inserter(transformed_members),
                   [&ctx](const std::pair<std::string, cg::value>& m) -> std::pair<std::string, module_::field_descriptor>
                   {
                       const auto& t = std::get<1>(m);

                       std::optional<std::size_t> import_index = std::nullopt;
                       if(t.get_type().is_import())
                       {
                           // find the import index.
                           auto import_it = std::find_if(ctx.imports.begin(), ctx.imports.end(),
                                                         [&t](const cg::imported_symbol& s) -> bool
                                                         {
                                                             return s.type == module_::symbol_type::type
                                                                    && s.name == t.get_type().base_type().to_string()
                                                                    && s.import_path == t.get_type().base_type().get_import_path();
                                                         });
                           if(import_it == ctx.imports.end())
                           {
                               throw emitter_error(fmt::format(
                                 "Type '{}' from package '{}' not found in import table.",
                                 t.get_type().base_type().to_string(), *t.get_type().base_type().get_import_path()));
                           }

                           import_index = std::distance(ctx.imports.begin(), import_it);
                       }

                       return std::make_pair(
                         std::get<0>(m),
                         module_::field_descriptor{
                           t.get_type().base_type().to_string(),
                           t.get_type().is_array(),
                           import_index});
                   });

    export_table.emplace_back(
      module_::symbol_type::type,
      type->get_name(),
      module_::struct_descriptor{std::move(transformed_members)});
}

std::size_t export_table_builder::get_index(module_::symbol_type t, const std::string& name) const
{
    auto it = std::find_if(export_table.begin(), export_table.end(),
                           [t, &name](const module_::exported_symbol& entry) -> bool
                           { return entry.type == t
                                    && entry.name == name; });
    if(it == export_table.end())
    {
        throw emitter_error(fmt::format("Symbol '{}' of type '{}' not found in export table.", name, to_string(t)));
    }

    return std::distance(export_table.begin(), it);
}

void export_table_builder::write(module_::language_module& mod) const
{
    for(const auto& entry: export_table)
    {
        if(entry.type == module_::symbol_type::function)
        {
            const module_::function_descriptor& desc = std::get<module_::function_descriptor>(entry.desc);

            std::pair<std::string, bool> return_type = {
              std::get<0>(desc.signature.return_type).base_type(),
              std::get<0>(desc.signature.return_type).is_array()};
            std::vector<std::pair<std::string, bool>> arg_types;

            std::transform(desc.signature.arg_types.cbegin(), desc.signature.arg_types.cend(), std::back_inserter(arg_types),
                           [](const std::pair<module_::variable_type, bool>& t) -> std::pair<std::string, bool>
                           {
                               return std::make_pair(std::get<0>(t).base_type(), std::get<0>(t).is_array());
                           });

            if(desc.native)
            {
                const module_::native_function_details& details = std::get<module_::native_function_details>(desc.details);

                mod.add_native_function(
                  entry.name,
                  std::move(return_type),
                  std::move(arg_types),
                  details.library_name);
            }
            else
            {
                const module_::function_details& details = std::get<module_::function_details>(desc.details);

                mod.add_function(
                  entry.name,
                  std::move(return_type),
                  std::move(arg_types),
                  details.size,
                  details.offset,
                  details.locals);
            }
        }
        else if(entry.type == module_::symbol_type::type)
        {
            const module_::struct_descriptor& desc = std::get<module_::struct_descriptor>(entry.desc);
            mod.add_struct(entry.name, desc.member_types);
        }
        else
        {
            throw emitter_error(fmt::format("Unexpected symbol type '{}' during export table write.", to_string(entry.type)));
        }
    }
}

/*
 * instruction_emitter implementation.
 */

std::set<std::string> instruction_emitter::collect_jump_targets() const
{
    std::set<std::string> targets;

    for(auto& f: ctx.funcs)
    {
        for(auto& it: f->get_basic_blocks())
        {
            for(auto& instr: it->get_instructions())
            {
                if(instr->get_name() == "jnz")
                {
                    auto& args = instr->get_args();
                    targets.emplace(static_cast<cg::label_argument*>(args.at(0).get())->get_label());
                    targets.emplace(static_cast<cg::label_argument*>(args.at(1).get())->get_label());
                }
                else if(instr->get_name() == "jmp")
                {
                    auto& args = instr->get_args();
                    targets.emplace(static_cast<cg::label_argument*>(args.at(0).get())->get_label());
                }
            }
        }
    }

    return targets;
}

void instruction_emitter::collect_imports()
{
    for(auto& f: ctx.funcs)
    {
        for(auto& it: f->get_basic_blocks())
        {
            for(auto& instr: it->get_instructions())
            {
                if(instr->get_name() == "invoke")
                {
                    if(instr->get_args().size() != 1)
                    {
                        throw emitter_error(fmt::format("Expected 1 argument for 'invoke', got {}.", instr->get_args().size()));
                    }

                    cg::function_argument* arg = static_cast<cg::function_argument*>(instr->get_args()[0].get());
                    auto& import_path = arg->get_import_path();
                    if(import_path.has_value())
                    {
                        auto import_it = std::find_if(ctx.prototypes.begin(), ctx.prototypes.end(),
                                                      [arg, import_path](const std::unique_ptr<cg::prototype>& p) -> bool
                                                      {
                                                          return p->is_import() && *p->get_import_path() == import_path
                                                                 && p->get_name() == *arg->get_value()->get_name();
                                                      });
                        if(import_it == ctx.prototypes.end())
                        {
                            throw emitter_error(fmt::format("Could not resolve imported function '{}'.", *arg->get_value()->get_name()));
                        }

                        ctx.add_import(module_::symbol_type::function, *(*import_it)->get_import_path(), (*import_it)->get_name());
                    }
                }
                else if(instr->get_name() == "new")
                {
                    if(instr->get_args().size() != 1)
                    {
                        throw emitter_error(fmt::format("Expected 1 argument for 'new', got {}.", instr->get_args().size()));
                    }

                    cg::type_argument* arg = static_cast<cg::type_argument*>(instr->get_args()[0].get());
                    auto& import_path = arg->get_import_path();
                    if(import_path.has_value())
                    {
                        auto import_it = std::find_if(ctx.types.begin(), ctx.types.end(),
                                                      [arg, import_path](const std::unique_ptr<cg::struct_>& p) -> bool
                                                      {
                                                          return p->is_import() && *p->get_import_path() == import_path
                                                                 && p->get_name() == *arg->get_value()->get_name();
                                                      });
                        if(import_it == ctx.types.end())
                        {
                            throw emitter_error(fmt::format("Could not resolve imported type '{}'.", *arg->get_value()->get_name()));
                        }

                        ctx.add_import(module_::symbol_type::type, *(*import_it)->get_import_path(), (*import_it)->get_name());
                    }
                }
            }
        }
    }
}

void instruction_emitter::emit_instruction(const std::unique_ptr<cg::function>& func, const std::unique_ptr<cg::instruction>& instr)
{
    auto& name = instr->get_name();
    auto& args = instr->get_args();

    auto expect_arg_size = [&name, &args](std::size_t s1, std::optional<std::size_t> s2 = std::nullopt)
    {
        if(args.size() != s1)
        {
            if(!s2.has_value())
            {
                throw emitter_error(fmt::format("Expected {} argument(s) for '{}', got {}.", s1, name, args.size()));
            }

            if(args.size() != *s2)
            {
                throw emitter_error(fmt::format("Expected {} or {} argument(s) for '{}', got {}.", s1, *s2, name, args.size()));
            }
        }
    };

    auto emit_typed = [this, &name, &args, expect_arg_size](
                        opcode i32_opcode,
                        std::optional<opcode> f32_opcode = std::nullopt,
                        std::optional<opcode> str_opcode = std::nullopt,
                        std::optional<opcode> array_opcode = std::nullopt,
                        std::optional<opcode> ref_opcode = std::nullopt)
    {
        expect_arg_size(1);

        cg::const_argument* arg = static_cast<cg::const_argument*>(args[0].get());
        const cg::value* v = arg->get_value();

        if(array_opcode.has_value() && v->get_type().is_array())
        {
            emit(instruction_buffer, *array_opcode);
            return;
        }

        if(v->get_type().get_type_class() == cg::type_class::i32)
        {
            emit(instruction_buffer, i32_opcode);
        }
        else if(v->get_type().get_type_class() == cg::type_class::f32)
        {
            if(!f32_opcode.has_value())
            {
                throw std::runtime_error(fmt::format("Invalid type 'f32' for instruction '{}'.", name));
            }

            emit(instruction_buffer, *f32_opcode);
        }
        else if(v->get_type().get_type_class() == cg::type_class::str)
        {
            if(!str_opcode.has_value())
            {
                throw std::runtime_error(fmt::format("Invalid type 'str' for instruction '{}'.", name));
            }

            emit(instruction_buffer, *str_opcode);
        }
        else if(v->get_type().is_reference())
        {
            if(!ref_opcode.has_value())
            {
                throw std::runtime_error(fmt::format("Invalid reference type for instruction '{}'.", name));
            }

            emit(instruction_buffer, *ref_opcode);
        }
        else
        {
            throw std::runtime_error(fmt::format("Invalid type '{}' for instruction '{}'.", v->get_type().to_string(), name));
        }
    };

    auto emit_typed_one_arg = [this, &name, &args, expect_arg_size](opcode i32_opcode, opcode f32_opcode, std::optional<opcode> str_opcode = std::nullopt)
    {
        expect_arg_size(1);

        cg::const_argument* arg = static_cast<cg::const_argument*>(args[0].get());
        const cg::value* v = arg->get_value();

        if(v->get_type().get_type_class() == cg::type_class::i32)
        {
            emit(instruction_buffer, i32_opcode, static_cast<std::int32_t>(static_cast<const cg::constant_int*>(v)->get_int()));
        }
        else if(v->get_type().get_type_class() == cg::type_class::f32)
        {
            emit(instruction_buffer, f32_opcode, static_cast<const cg::constant_float*>(v)->get_float());
        }
        else if(v->get_type().get_type_class() == cg::type_class::str)
        {
            if(!str_opcode.has_value())
            {
                throw std::runtime_error(fmt::format("Invalid type 'str' for instruction '{}'.", name));
            }

            emit(instruction_buffer, *str_opcode, vle_int{static_cast<const cg::constant_str*>(v)->get_constant_index()});
        }
        else
        {
            throw std::runtime_error(fmt::format("Invalid type '{}' for instruction '{}'.", v->get_type().to_string(), name));
        }
    };

    auto emit_typed_one_var_arg =
      [this, &name, &args, &func, expect_arg_size](
        opcode i32_opcode,
        opcode f32_opcode,
        std::optional<opcode> str_array_opcode = std::nullopt,
        std::optional<opcode> ref_opcode = std::nullopt)
    {
        expect_arg_size(1);

        cg::variable_argument* arg = static_cast<cg::variable_argument*>(args[0].get());
        const cg::value* v = arg->get_value();

        if(!v->has_name())
        {
            throw std::runtime_error(fmt::format("Cannot emit instruction '{}': Argument value has no name.", name));
        }
        vle_int index = func->get_scope()->get_index(*v->get_name());

        if(v->get_type().is_array())
        {
            if(!str_array_opcode.has_value())
            {
                throw std::runtime_error(fmt::format("Invalid type '{}' for instruction '{}'.", v->get_type().to_string(), name));
            }

            emit(instruction_buffer, *str_array_opcode);
        }
        else if(v->get_type().get_type_class() == cg::type_class::i32)
        {
            emit(instruction_buffer, i32_opcode);
        }
        else if(v->get_type().get_type_class() == cg::type_class::f32)
        {
            emit(instruction_buffer, f32_opcode);
        }
        else if(v->get_type().get_type_class() == cg::type_class::str)
        {
            if(!str_array_opcode.has_value())
            {
                throw std::runtime_error(fmt::format("Invalid type 'str' for instruction '{}'.", name));
            }

            emit(instruction_buffer, *str_array_opcode);
        }
        else if(v->get_type().is_reference())
        {
            if(!ref_opcode.has_value())
            {
                throw std::runtime_error(fmt::format("Invalid reference type for instruction '{}'.", name));
            }

            emit(instruction_buffer, *ref_opcode);
        }
        else
        {
            throw std::runtime_error(fmt::format("Invalid type '{}' for instruction '{}'.", v->get_type().to_string(), name));
        }

        instruction_buffer & index;
    };

    if(name == "add")
    {
        emit_typed(opcode::iadd, opcode::fadd);
    }
    else if(name == "sub")
    {
        emit_typed(opcode::isub, opcode::fsub);
    }
    else if(name == "mul")
    {
        emit_typed(opcode::imul, opcode::fmul);
    }
    else if(name == "div")
    {
        emit_typed(opcode::idiv, opcode::fdiv);
    }
    else if(name == "mod")
    {
        emit_typed(opcode::imod);
    }
    else if(name == "const_null")
    {
        emit(instruction_buffer, opcode::aconst_null);
    }
    else if(name == "const")
    {
        emit_typed_one_arg(opcode::iconst, opcode::fconst, opcode::sconst);
    }
    else if(name == "load")
    {
        emit_typed_one_var_arg(opcode::iload, opcode::fload, opcode::aload, opcode::aload);
    }
    else if(name == "store")
    {
        emit_typed_one_var_arg(opcode::istore, opcode::fstore, opcode::astore, opcode::astore);
    }
    else if(name == "load_element")
    {
        emit_typed(opcode::iaload, opcode::faload, opcode::saload);
    }
    else if(name == "store_element")
    {
        emit_typed(opcode::iastore, opcode::fastore, opcode::sastore);
    }
    else if(name == "dup")
    {
        if(args.size() == 1)
        {
            emit_typed(opcode::idup, opcode::fdup, std::nullopt, opcode::adup, opcode::adup);
        }
        else if(args.size() == 2)
        {
            // We intentionally do not resolve the types for the instruction, since
            // we only need to know if it is a built-in type or an address.

            // get the duplicated value.
            cg::type_argument* v_arg = static_cast<cg::type_argument*>(args[0].get());
            const cg::value* v = v_arg->get_value();
            module_::variable_type v_type = v->get_type().to_string();

            // get the stack arguments.
            cg::type_argument* stack_arg = static_cast<cg::type_argument*>(args[1].get());
            module_::variable_type s_type = stack_arg->get_value()->get_type().to_string();

            // emit instruction.
            emit(instruction_buffer, opcode::dup_x1);
            instruction_buffer & v_type & s_type;
        }
        else
        {
            throw emitter_error(fmt::format("Unexpected argument count ({}) for 'dup'.", args.size()));
        }
    }
    else if(name == "pop")
    {
        emit_typed(opcode::pop, opcode::pop, opcode::apop, opcode::apop, opcode::apop);    // same instruction for i32 and f32.
    }
    else if(name == "cast")
    {
        expect_arg_size(1);
        cg::cast_argument* arg = static_cast<cg::cast_argument*>(args[0].get());
        if(arg->get_cast() == cg::type_cast::i32_to_f32)
        {
            emit(instruction_buffer, opcode::i2f);
        }
        else if(arg->get_cast() == cg::type_cast::f32_to_i32)
        {
            emit(instruction_buffer, opcode::f2i);
        }
        else
        {
            throw emitter_error(fmt::format("Invalid cast type '{}'.", ::cg::to_string(arg->get_cast())));
        }
    }
    else if(name == "invoke")
    {
        expect_arg_size(1);
        cg::function_argument* arg = static_cast<cg::function_argument*>(args[0].get());
        const cg::value* v = arg->get_value();

        auto& import_path = arg->get_import_path();
        if(!import_path.has_value())
        {
            // resolve module-local functions.
            vle_int index = exports.get_index(module_::symbol_type::function, v->get_name().value());
            emit(instruction_buffer, opcode::invoke);
            instruction_buffer & index;
            return;
        }

        // resolve imports.
        auto import_it = std::find_if(ctx.prototypes.begin(), ctx.prototypes.end(),
                                      [arg, import_path](const std::unique_ptr<cg::prototype>& p) -> bool
                                      {
                                          return p->is_import() && *p->get_import_path() == import_path
                                                 && p->get_name() == *arg->get_value()->get_name();
                                      });
        if(import_it == ctx.prototypes.end())
        {
            throw emitter_error(fmt::format("Could not resolve imported function '{}'.", *arg->get_value()->get_name()));
        }

        vle_int index = -ctx.get_import_index(module_::symbol_type::function, *(*import_it)->get_import_path(), (*import_it)->get_name()) - 1;
        emit(instruction_buffer, opcode::invoke);
        instruction_buffer & index;
    }
    else if(name == "ret")
    {
        expect_arg_size(1);

        if(args[0]->get_value()->get_type().get_type_class() == cg::type_class::void_)
        {
            // return void from a function.
            emit(instruction_buffer, opcode::ret);
        }
        else
        {
            emit_typed(opcode::iret, opcode::fret, opcode::sret, opcode::aret, opcode::aret);
        }
    }
    else if(name == "set_field")
    {
        expect_arg_size(1);

        cg::field_access_argument* arg = static_cast<cg::field_access_argument*>(args[0].get());

        // resolve references to struct and field name.
        vle_int struct_index = 0;
        vle_int field_index = 0;

        auto struct_it = std::find_if(ctx.types.begin(), ctx.types.end(),
                                      [arg](const std::unique_ptr<cg::struct_>& t) -> bool
                                      {
                                          return t->get_name() == arg->get_struct_name()
                                                 && t->get_import_path() == arg->get_import_path();
                                      });
        if(struct_it != ctx.types.end())
        {
            if((*struct_it)->get_import_path().has_value())
            {
                // find struct in import table.
                auto import_it = std::find_if(ctx.imports.begin(), ctx.imports.end(),
                                              [&struct_it](const cg::imported_symbol& s) -> bool
                                              {
                                                  return s.type == module_::symbol_type::type
                                                         && s.name == (*struct_it)->get_name()
                                                         && s.import_path == (*struct_it)->get_import_path();
                                              });
                if(import_it == ctx.imports.end())
                {
                    throw emitter_error(fmt::format(
                      "Cannot find type '{}' from package '{}' in import table.",
                      (*struct_it)->get_name(), *(*struct_it)->get_import_path()));
                }
                struct_index = -std::distance(ctx.imports.begin(), import_it) - 1;
            }
            else
            {
                // find struct in export table.
                struct_index = exports.get_index(module_::symbol_type::type, (*struct_it)->get_name());
            }
        }
        else
        {
            throw emitter_error(fmt::format("Type '{}' not found.", arg->get_struct_name()));
        }

        auto& members = struct_it->get()->get_members();
        auto field_it = std::find_if(members.begin(), members.end(),
                                     [arg](const std::pair<std::string, cg::value>& m) -> bool
                                     {
                                         return m.first == *arg->get_member().get_name();
                                     });
        if(field_it == members.end())
        {
            throw emitter_error(fmt::format("Could not resolve field '{}' in struct '{}'.", *arg->get_member().get_name(), arg->get_struct_name()));
        }
        field_index = std::distance(members.begin(), field_it);

        emit(instruction_buffer, opcode::setfield);
        instruction_buffer & struct_index;
        instruction_buffer & field_index;
    }
    else if(name == "get_field")
    {
        expect_arg_size(1);

        cg::field_access_argument* arg = static_cast<cg::field_access_argument*>(args[0].get());

        // resolve references to struct and field name.
        vle_int struct_index = 0;
        vle_int field_index = 0;

        auto struct_it = std::find_if(ctx.types.begin(), ctx.types.end(),
                                      [arg](const std::unique_ptr<cg::struct_>& t) -> bool
                                      {
                                          return t->get_name() == arg->get_struct_name()
                                                 && t->get_import_path() == arg->get_import_path();
                                      });
        if(struct_it != ctx.types.end())
        {
            if((*struct_it)->get_import_path().has_value())
            {
                // find struct in import table.
                auto import_it = std::find_if(ctx.imports.begin(), ctx.imports.end(),
                                              [&struct_it](const cg::imported_symbol& s) -> bool
                                              {
                                                  return s.type == module_::symbol_type::type
                                                         && s.name == (*struct_it)->get_name()
                                                         && s.import_path == (*struct_it)->get_import_path();
                                              });
                if(import_it == ctx.imports.end())
                {
                    throw emitter_error(fmt::format(
                      "Cannot find type '{}' from package '{}' in import table.",
                      (*struct_it)->get_name(), *(*struct_it)->get_import_path()));
                }
                struct_index = -std::distance(ctx.imports.begin(), import_it) - 1;
            }
            else
            {
                // find struct in export table.
                struct_index = exports.get_index(module_::symbol_type::type, (*struct_it)->get_name());
            }
        }
        else
        {
            throw emitter_error(fmt::format("Type '{}' not found.", arg->to_string()));
        }

        auto& members = struct_it->get()->get_members();
        auto field_it = std::find_if(members.begin(), members.end(),
                                     [arg](const std::pair<std::string, cg::value>& m) -> bool
                                     {
                                         return m.first == *arg->get_member().get_name();
                                     });
        if(field_it == members.end())
        {
            throw emitter_error(fmt::format("Could not resolve field '{}' in struct '{}'.", *arg->get_member().get_name(), arg->get_struct_name()));
        }
        field_index = std::distance(members.begin(), field_it);

        emit(instruction_buffer, opcode::getfield);
        instruction_buffer & struct_index;
        instruction_buffer & field_index;
    }
    else if(name == "and")
    {
        emit_typed(opcode::iand);
    }
    else if(name == "land")
    {
        emit_typed(opcode::land);
    }
    else if(name == "or")
    {
        emit_typed(opcode::ior);
    }
    else if(name == "lor")
    {
        emit_typed(opcode::lor);
    }
    else if(name == "xor")
    {
        emit_typed(opcode::ixor);
    }
    else if(name == "shl")
    {
        emit_typed(opcode::ishl);
    }
    else if(name == "shr")
    {
        emit_typed(opcode::ishr);
    }
    else if(name == "cmpl")
    {
        emit_typed(opcode::icmpl, opcode::fcmpl);
    }
    else if(name == "cmple")
    {
        emit_typed(opcode::icmple, opcode::fcmple);
    }
    else if(name == "cmpg")
    {
        emit_typed(opcode::icmpg, opcode::fcmpg);
    }
    else if(name == "cmpge")
    {
        emit_typed(opcode::icmpge, opcode::fcmpge);
    }
    else if(name == "cmpeq")
    {
        emit_typed(opcode::icmpeq, opcode::fcmpeq, opcode::acmpeq, opcode::acmpeq, opcode::acmpeq);
    }
    else if(name == "cmpne")
    {
        emit_typed(opcode::icmpne, opcode::fcmpne, opcode::acmpne, opcode::acmpne, opcode::acmpne);
    }
    else if(name == "jnz")
    {
        expect_arg_size(2);

        auto& args = instr->get_args();

        auto then_label = static_cast<cg::label_argument*>(args[0].get())->get_label();
        auto else_label = static_cast<cg::label_argument*>(args[1].get())->get_label();

        auto then_it = jump_targets.find(then_label);
        if(then_it == jump_targets.end())
        {
            throw emitter_error(fmt::format("Cannot find label '{}'.", then_label));
        }

        auto else_it = jump_targets.find(else_label);
        if(else_it == jump_targets.end())
        {
            throw emitter_error(fmt::format("Cannot find label '{}'.", else_label));
        }

        vle_int then_index = std::distance(jump_targets.begin(), then_it);
        vle_int else_index = std::distance(jump_targets.begin(), else_it);

        emit(instruction_buffer, opcode::jnz);
        instruction_buffer & then_index;
        instruction_buffer & else_index;
    }
    else if(name == "jmp")
    {
        expect_arg_size(1);

        auto& args = instr->get_args();
        auto label = static_cast<cg::label_argument*>(args[0].get())->get_label();

        auto it = jump_targets.find(label);
        if(it == jump_targets.end())
        {
            throw emitter_error(fmt::format("Cannot find label '{}'.", label));
        }

        vle_int index = std::distance(jump_targets.begin(), it);
        emit(instruction_buffer, opcode::jmp);
        instruction_buffer & index;
    }
    else if(name == "new")
    {
        expect_arg_size(1);

        auto& args = instr->get_args();
        auto type = static_cast<cg::type_argument*>(args[0].get())->get_value()->get_type();

        // resolve type to index.
        vle_int struct_index = 0;

        auto struct_it = std::find_if(ctx.types.begin(), ctx.types.end(),
                                      [&type](const std::unique_ptr<cg::struct_>& t) -> bool
                                      {
                                          return t->get_name() == type.get_struct_name()
                                                 && t->get_import_path() == type.get_import_path();
                                      });
        if(struct_it != ctx.types.end())
        {
            if((*struct_it)->get_import_path().has_value())
            {
                // find type in import table.
                auto import_it = std::find_if(ctx.imports.begin(), ctx.imports.end(),
                                              [&struct_it](const cg::imported_symbol& s) -> bool
                                              {
                                                  return s.type == module_::symbol_type::type
                                                         && s.name == (*struct_it)->get_name()
                                                         && s.import_path == (*struct_it)->get_import_path();
                                              });
                if(import_it == ctx.imports.end())
                {
                    throw emitter_error(fmt::format(
                      "Cannot find type '{}' from package '{}' in import table.",
                      (*struct_it)->get_name(), *(*struct_it)->get_import_path()));
                }
                struct_index = -std::distance(ctx.imports.begin(), import_it) - 1;
            }
            else
            {
                // find struct in export table.
                struct_index = exports.get_index(module_::symbol_type::type, (*struct_it)->get_name());
            }
        }
        else
        {
            throw emitter_error(fmt::format("Type '{}' not found.", type.to_string()));
        }

        emit(instruction_buffer, opcode::new_);
        instruction_buffer & struct_index;
    }
    else if(name == "newarray")
    {
        expect_arg_size(1);

        auto& args = instr->get_args();
        auto type_class = static_cast<cg::type_argument*>(args[0].get())->get_value()->get_type().get_type_class();

        module_::array_type type;
        if(type_class == cg::type_class::i32)
        {
            type = module_::array_type::i32;
        }
        else if(type_class == cg::type_class::f32)
        {
            type = module_::array_type::f32;
        }
        else if(type_class == cg::type_class::str)
        {
            type = module_::array_type::str;
        }
        else if(type_class == cg::type_class::struct_)
        {
            type = module_::array_type::ref;
        }
        else
        {
            throw std::runtime_error(fmt::format(
              "Unknown array type '{}' for newarray.",
              static_cast<cg::type_argument*>(args[0].get())->get_value()->get_type().to_string()));
        }

        emit(instruction_buffer, opcode::newarray);
        instruction_buffer & type;
    }
    else if(name == "arraylength")
    {
        emit(instruction_buffer, opcode::arraylength);
    }
    else
    {
        // TODO
        throw std::runtime_error(fmt::format("instruction_emitter::emit_instruction: instruction generation for '{}' not implemented.", name));
    }
}

void instruction_emitter::run()
{
    // clear buffers and tables.
    instruction_buffer.clear();
    exports.clear();

    // collect jump targets.
    jump_targets = collect_jump_targets();

    // collect imports.
    collect_imports();

    // the import count is not allowed to change, so store it here and check later.
    std::size_t import_count = ctx.imports.size();

    // exported types.
    for(auto& it: ctx.types)
    {
        if(it->is_import())
        {
            // Verify that the type is in the import table.
            auto import_it = std::find_if(ctx.imports.begin(), ctx.imports.end(),
                                          [&it](const cg::imported_symbol& s) -> bool
                                          {
                                              return s.type == module_::symbol_type::type
                                                     && s.name == it->get_name()
                                                     && s.import_path == it->get_import_path();
                                          });
            if(import_it == ctx.imports.end())
            {
                throw std::runtime_error(fmt::format(
                  "Type '{}' from package '{}' not found in import table.",
                  it->get_name(), *it->get_import_path()));
            }
        }
        else
        {
            exports.add_type(ctx, it);
        }
    }

    // exported functions.
    for(auto& f: ctx.funcs)
    {
        auto [signature_ret_type, signature_arg_types] = f->get_signature();
        std::pair<std::string, bool> return_type = {
          signature_ret_type.base_type().to_string(),    // FIXME Store type.
          signature_ret_type.is_array()};
        std::vector<std::pair<std::string, bool>> arg_types;

        std::transform(signature_arg_types.cbegin(), signature_arg_types.cend(), std::back_inserter(arg_types),
                       [](const cg::type& t) -> std::pair<std::string, bool>
                       {
                           return std::make_pair(t.base_type().to_string(), t.is_array());    // FIXME Store type.
                       });

        if(f->is_native())
        {
            exports.add_native_function(
              f->get_name(),
              std::move(return_type),
              std::move(arg_types),
              f->get_import_library());
        }
        else
        {
            exports.add_function(
              f->get_name(),
              std::move(return_type),
              std::move(arg_types));
        }
    }

    // the export count is not allowed to change after this point, so store it here and check later.
    std::size_t export_count = exports.size();

    /*
     * generate bytecode.
     */
    for(auto& f: ctx.funcs)
    {
        // skip native functions.
        if(f->is_native())
        {
            continue;
        }

        /*
         * allocate and map locals.
         */
        auto* s = f->get_scope();
        auto& func_args = s->get_args();
        auto& func_locals = f->get_scope()->get_locals();
        const std::size_t local_count = func_args.size() + func_locals.size();

        std::vector<module_::variable_descriptor> locals{local_count};

        std::set<std::size_t> unset_indices;
        for(std::size_t i = 0; i < local_count; ++i)
        {
            unset_indices.insert(unset_indices.end(), i);
        }

        for(auto& it: func_args)
        {
            auto name = it->get_name();
            if(!name.has_value())
            {
                throw emitter_error(fmt::format("Unnamed variable in function '{}'.", f->get_name()));
            }

            std::size_t index = s->get_index(*it->get_name());
            if(unset_indices.find(index) == unset_indices.end())
            {
                throw emitter_error(fmt::format("Tried to map local '{}' with index '{}' multiple times.", *name, index));
            }
            unset_indices.erase(index);

            locals[index] = module_::variable_descriptor{
              module_::variable_type{it->get_type().base_type().to_string(),
                                     it->get_type().is_array() ? std::make_optional(1) : std::nullopt}};
        }

        for(auto& it: func_locals)
        {
            auto name = it->get_name();
            if(!name.has_value())
            {
                throw emitter_error(fmt::format("Unnamed variable in function '{}'.", f->get_name()));
            }

            std::size_t index = s->get_index(*it->get_name());
            if(unset_indices.find(index) == unset_indices.end())
            {
                throw emitter_error(fmt::format("Tried to map local '{}' with index '{}' multiple times.", *name, index));
            }
            unset_indices.erase(index);

            locals[index] = module_::variable_descriptor{
              module_::variable_type{it->get_type().base_type().to_string(),
                                     it->get_type().is_array() ? std::make_optional(1) : std::nullopt}};
        }

        if(unset_indices.size() != 0)
        {
            throw emitter_error(fmt::format("Inconsistent local count for function '{}'.", f->get_name()));
        }

        /*
         * instruction generation.
         */
        std::size_t entry_point = instruction_buffer.tell();

        for(auto& it: f->get_basic_blocks())
        {
            auto jump_it = jump_targets.find(it->get_label());
            if(jump_it != jump_targets.end())
            {
                vle_int index = std::distance(jump_targets.begin(), jump_it);
                emit(instruction_buffer, opcode::label);
                instruction_buffer & index;
            }

            for(auto& instr: it->get_instructions())
            {
                emit_instruction(f, instr);
            }
        }

        /*
         * store function details.
         */
        std::size_t size = instruction_buffer.tell() - entry_point;
        exports.update_function(f->get_name(), size, entry_point, locals);
    }

    // check that the import and export counts did not change.
    if(import_count != ctx.imports.size())
    {
        throw emitter_error(
          fmt::format("Import count changed during instruction emission ({} -> {}).",
                      import_count, ctx.imports.size()));
    }
    if(export_count != exports.size())
    {
        throw emitter_error(
          fmt::format("Export count changed during instruction emission ({} -> {}).",
                      export_count, exports.size()));
    }
}

module_::language_module instruction_emitter::to_module() const
{
    module_::language_module mod;

    /*
     * imports.
     */

    // Find packages which are not already in the import table.
    std::vector<std::string> packages;
    for(const auto& it: ctx.imports)
    {
        if(it.type == module_::symbol_type::package)
        {
            auto p_it = std::find(packages.begin(), packages.end(), it.name);
            if(p_it != packages.end())
            {
                packages.erase(p_it);
            }
        }
        else
        {
            // ensure we import the symbol's package.
            auto p_it = std::find_if(ctx.imports.begin(), ctx.imports.end(),
                                     [&it](auto& s) -> bool
                                     {
                                         return s.type == module_::symbol_type::package && s.name == it.import_path;
                                     });
            if(p_it == ctx.imports.end())
            {
                auto p_it = std::find(packages.begin(), packages.end(), it.name);
                if(p_it == packages.end())
                {
                    packages.push_back(it.import_path);
                }
            }
        }
    }

    // Write import table. Additional packages from the search above are appended.
    for(const auto& it: ctx.imports)
    {
        std::uint32_t package_index = 0;

        auto import_it = std::find_if(ctx.imports.begin(), ctx.imports.end(),
                                      [&it](auto& s) -> bool
                                      {
                                          return s.type == module_::symbol_type::package && s.name == it.import_path;
                                      });
        if(import_it != ctx.imports.end())
        {
            package_index = std::distance(ctx.imports.begin(), import_it);
        }
        else
        {
            auto package_it = std::find(packages.begin(), packages.end(), it.import_path);
            if(package_it == packages.end())
            {
                throw std::runtime_error(fmt::format("Package '{}' not found in package table.", it.import_path));
            }
            package_index = ctx.imports.size() + std::distance(packages.begin(), package_it);
        }

        mod.add_import(it.type, it.name, package_index);
    }
    for(const auto& it: packages)
    {
        mod.add_import(module_::symbol_type::package, it);
    }

    // strings.
    mod.set_string_table(ctx.strings);

    // export table.
    exports.write(mod);

    // instructions.
    mod.set_binary(instruction_buffer.get_buffer());

    return mod;
}

}    // namespace slang