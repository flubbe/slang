/**
 * slang - a simple scripting language.
 *
 * instruction emitter.
 *
 * \author Felix Lubbe
 * \copyright Copyright (c) 2025
 * \license Distributed under the MIT software license (see accompanying LICENSE.txt).
 */

#include "archives/memory.h"
#include "ast/ast.h"
#include "ast/node_registry.h"
#include "codegen/codegen.h"
#include "shared/module.h"
#include "shared/opcodes.h"
#include "emitter.h"
#include "macro.h"
#include "utils.h"

#include <print>    // FIXME

namespace cg = slang::codegen;
namespace ast = slang::ast;

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
  module_::variable_type return_type,
  std::vector<module_::variable_type> arg_types)
{
    if(std::ranges::find_if(
         export_table,
         [&name](const module_::exported_symbol& entry) -> bool
         { return entry.type == module_::symbol_type::function
                  && entry.name == name; })
       != export_table.end())
    {
        throw emitter_error(std::format("Cannot add function to export table: '{}' already exists.", name));
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
  std::size_t size,    // NOLINT(bugprone-easily-swappable-parameters)
  std::size_t offset,
  std::vector<module_::variable_descriptor> locals)
{
    auto it = std::ranges::find_if(
      export_table,
      [&name](const module_::exported_symbol& entry) -> bool
      { return entry.type == module_::symbol_type::function
               && entry.name == name; });
    if(it == export_table.end())
    {
        throw emitter_error(std::format("Cannot update function to export table: '{}' not found.", name));
    }

    auto& desc = std::get<module_::function_descriptor>(it->desc);
    auto& details = std::get<module_::function_details>(desc.details);

    details.size = size;
    details.offset = offset;
    details.locals = std::move(locals);
}

void export_table_builder::add_native_function(
  const std::string& name,
  module_::variable_type return_type,
  std::vector<module_::variable_type> arg_types,
  std::string import_library)
{
    if(std::ranges::find_if(
         export_table,
         [&name](const module_::exported_symbol& entry) -> bool
         { return entry.type == module_::symbol_type::function
                  && entry.name == name; })
       != export_table.end())
    {
        throw emitter_error(std::format("Cannot add function to export table: '{}' already exists.", name));
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

/** Information about an imported symbol. */
struct import_info
{
    /** The symbol name. */
    std::string name;

    /** The qualified name of the importing module. */
    std::string qualified_module_name;
};

/**
 * Return the import info or `std::nullopt` for a symbol id.
 *
 * @param symbol_id The symbol id.
 * @param sema_env The semantic environment.
 * @returns For imports, returns the import info, and `std::nullopt` for locally defined symbols.
 */
static std::optional<import_info> get_import_info(
  sema::symbol_id symbol_id,
  const sema::env& sema_env)
{
    const auto symbol_info_it = sema_env.symbol_table.find(symbol_id);
    if(symbol_info_it == sema_env.symbol_table.end())
    {
        throw emitter_error(
          std::format(
            "Could not find symbol info for symbol with id {}.",
            symbol_id.value));
    }

    if(symbol_info_it->second.declaring_module == sema::symbol_info::current_module_id)
    {
        return std::nullopt;
    }

    auto declaring_module_symbol_it = sema_env.symbol_table.find(symbol_info_it->second.declaring_module);
    if(declaring_module_symbol_it == sema_env.symbol_table.end())
    {
        throw emitter_error(
          std::format(
            "Could not find symbol for module with id {} for symbol '{}'.",
            symbol_info_it->second.declaring_module.value,
            symbol_info_it->second.qualified_name));
    }

    return std::make_optional<import_info>(
      {.name = symbol_info_it->second.name,
       .qualified_module_name = declaring_module_symbol_it->second.qualified_name});
}

/**
 * Return the import info or `std::nullopt` for a type id.
 *
 * @param type_id The type id.
 * @param sema_env The semantic environment.
 * @returns For imports, returns the import info, and `std::nullopt` for locally defined types.
 */
static std::optional<import_info> get_import_info(
  ty::type_id type_id,
  const sema::env& sema_env)
{
    const auto symbol_it = std::ranges::find_if(
      sema_env.type_map,
      [type_id](const auto& p) -> bool
      {
          return p.second == type_id;
      });
    if(symbol_it == sema_env.type_map.end())
    {
        throw emitter_error(
          std::format(
            "Could not find symbol id for type with id {}.",
            type_id));
    }

    return get_import_info(symbol_it->first.value, sema_env);
}

void export_table_builder::add_struct(const ty::struct_info& type)
{
    throw std::runtime_error("export_table_builder::add_struct");

    // if(std::ranges::find_if(
    //      export_table,
    //      [&type](const module_::exported_symbol& entry) -> bool
    //      { return entry.type == module_::symbol_type::type
    //               && entry.name == type->get_name(); })
    //    != export_table.end())
    // {
    //     throw emitter_error(
    //       std::format(
    //         "Cannot add type to export table: '{}' already exists.",
    //         type->get_name()));
    // }

    // std::vector<std::pair<std::string, module_::field_descriptor>> transformed_members =
    //   type->get_members()
    //   | std::views::transform(
    //     [this](const std::pair<std::string, cg::value>& m) -> std::pair<std::string, module_::field_descriptor>
    //     {
    //         const auto& t = std::get<1>(m);
    //         auto type_id = t.get_type().get_type_id();
    //         if(!type_id.has_value())
    //         {
    //             throw emitter_error("Type has no id.");
    //         }

    //         std::optional<import_info> info = get_import_info(type_id.value(), emitter.sema_env);
    //         std::optional<std::size_t> import_index = std::nullopt;

    //         if(info.has_value())
    //         {
    //             // find the import index.
    //             auto import_it = std::ranges::find_if(
    //               emitter.codegen_ctx.imports,
    //               [&info](const cg::imported_symbol& s) -> bool
    //               {
    //                   return s.type == module_::symbol_type::type
    //                          && s.name == info.value().name
    //                          && s.import_path == info.value().qualified_module_name;
    //               });
    //             if(import_it == emitter.codegen_ctx.imports.end())
    //             {
    //                 throw emitter_error(
    //                   std::format(
    //                     "Type '{}' from package '{}' not found in import table.",
    //                     info.value().name,
    //                     info.value().qualified_module_name));
    //             }

    //             import_index = std::distance(emitter.codegen_ctx.imports.begin(), import_it);
    //         }

    //         return std::make_pair(
    //           std::get<0>(m),
    //           module_::field_descriptor{
    //             emitter.type_ctx.to_string(
    //               emitter.type_ctx.get_base_type(
    //                 type_id.value())),
    //             emitter.type_ctx.is_array(type_id.value()),
    //             import_index});
    //     })
    //   | std::ranges::to<std::vector>();

    // export_table.emplace_back(
    //   module_::symbol_type::type,
    //   type->get_name(),
    //   module_::struct_descriptor{
    //     .flags = type->get_flags(),
    //     .member_types = std::move(transformed_members)});
}

void export_table_builder::add_constant(std::string name, std::size_t i)
{
    if(std::ranges::find_if(
         export_table,
         [&name](const module_::exported_symbol& entry) -> bool
         { return entry.type == module_::symbol_type::constant
                  && entry.name == name; })
       != export_table.end())
    {
        throw emitter_error(std::format("Cannot add constant to export table: '{}' already exists.", name));
    }

    export_table.emplace_back(module_::symbol_type::constant, std::move(name), i);
}

void export_table_builder::add_macro(std::string name, module_::macro_descriptor desc)
{
    if(std::ranges::find_if(
         export_table,
         [&name](const module_::exported_symbol& entry) -> bool
         { return entry.type == module_::symbol_type::macro
                  && entry.name == name; })
       != export_table.end())
    {
        throw emitter_error(std::format("Cannot add macro to export table: '{}' already exists.", name));
    }

    export_table.emplace_back(
      module_::symbol_type::macro,
      name,
      std::move(desc));
}

std::size_t export_table_builder::get_index(module_::symbol_type t, const std::string& name) const
{
    auto it = std::ranges::find_if(
      std::as_const(export_table),
      [t, &name](const module_::exported_symbol& entry) -> bool
      { return entry.type == t
               && entry.name == name; });
    if(it == export_table.cend())
    {
        throw emitter_error(std::format("Symbol '{}' of type '{}' not found in export table.", name, to_string(t)));
    }

    return std::distance(export_table.cbegin(), it);
}

void export_table_builder::write(module_::language_module& mod) const
{
    for(const auto& entry: export_table)
    {
        if(entry.type == module_::symbol_type::function)
        {
            const auto& desc = std::get<module_::function_descriptor>(entry.desc);

            if(desc.native)
            {
                const auto& details = std::get<module_::native_function_details>(desc.details);

                mod.add_native_function(
                  entry.name,
                  desc.signature.return_type,
                  desc.signature.arg_types,
                  details.library_name);
            }
            else
            {
                const auto& details = std::get<module_::function_details>(desc.details);

                mod.add_function(
                  entry.name,
                  desc.signature.return_type,
                  desc.signature.arg_types,
                  details.size,
                  details.offset,
                  details.locals);
            }
        }
        else if(entry.type == module_::symbol_type::type)
        {
            const auto& desc = std::get<module_::struct_descriptor>(entry.desc);
            mod.add_struct(entry.name, desc.member_types, desc.flags);
        }
        else if(entry.type == module_::symbol_type::constant)
        {
            mod.add_constant(entry.name, std::get<std::size_t>(entry.desc));
        }
        else if(entry.type == module_::symbol_type::macro)
        {
            mod.add_macro(entry.name, std::get<module_::macro_descriptor>(entry.desc));
        }
        else
        {
            throw emitter_error(std::format("Unexpected symbol type '{}' during export table write.", to_string(entry.type)));
        }
    }
}

/*
 * instruction_emitter implementation.
 */

std::set<std::string> instruction_emitter::collect_jump_targets() const
{
    std::set<std::string> targets;

    for(auto& f: codegen_ctx.funcs)
    {
        for(const auto& it: f->get_basic_blocks())
        {
            for(auto& instr: it->get_instructions())
            {
                if(instr->get_name() == "jnz")
                {
                    const auto& args = instr->get_args();
                    targets.emplace(static_cast<cg::label_argument*>(args.at(0).get())->get_label());    // NOLINT(cppcoreguidelines-pro-type-static-cast-downcast)
                    targets.emplace(static_cast<cg::label_argument*>(args.at(1).get())->get_label());    // NOLINT(cppcoreguidelines-pro-type-static-cast-downcast)
                }
                else if(instr->get_name() == "jmp")
                {
                    const auto& args = instr->get_args();
                    targets.emplace(static_cast<cg::label_argument*>(args.at(0).get())->get_label());    // NOLINT(cppcoreguidelines-pro-type-static-cast-downcast)
                }
            }
        }
    }

    return targets;
}

void instruction_emitter::collect_imports()
{
    for(auto& f: codegen_ctx.funcs)
    {
        for(const auto& it: f->get_basic_blocks())
        {
            for(auto& instr: it->get_instructions())
            {
                if(instr->get_name() == "invoke")
                {
                    if(instr->get_args().size() != 1)
                    {
                        throw emitter_error(
                          std::format(
                            "Expected 1 argument for 'invoke', got {}.",
                            instr->get_args().size()));
                    }

                    const auto* arg = static_cast<const cg::function_argument*>(instr->get_args()[0].get());    // NOLINT(cppcoreguidelines-pro-type-static-cast-downcast)

                    const auto symbol_info_it = sema_env.symbol_table.find(arg->get_symbol_id());
                    if(symbol_info_it == sema_env.symbol_table.end())
                    {
                        throw emitter_error(
                          std::format(
                            "Symbol with id {} not found in symbol table.",
                            arg->get_symbol_id().value));
                    }

                    if(symbol_info_it->second.declaring_module != sema::symbol_info::current_module_id)
                    {
                        auto declaring_module_symbol_it = sema_env.symbol_table.find(symbol_info_it->second.declaring_module);
                        if(declaring_module_symbol_it == sema_env.symbol_table.end())
                        {
                            throw emitter_error(
                              std::format(
                                "Could not find symbol for module with id {} for symbol '{}'.",
                                symbol_info_it->second.declaring_module.value,
                                symbol_info_it->second.qualified_name));
                        }

                        // TODO
                        throw std::runtime_error("instruction_emitter::collect_imports");

                        // auto import_it = std::ranges::find_if(
                        //   codegen_ctx.prototypes,
                        //   [arg, &declaring_module_symbol_it](const std::unique_ptr<cg::prototype>& p) -> bool
                        //   {
                        //       return p->is_import()
                        //              && *p->get_import_path() == declaring_module_symbol_it->second.qualified_name
                        //              && p->get_name() == *arg->get_value()->get_name();
                        //   });
                        // if(import_it == codegen_ctx.prototypes.end())
                        // {
                        //     throw emitter_error(
                        //       std::format(
                        //         "Could not resolve imported function '{}'.",
                        //         arg->get_value()->get_name().value_or("<unknown>")));
                        // }

                        // codegen_ctx.add_import(
                        //   module_::symbol_type::function,
                        //   (*import_it)->get_import_path().value(),    // NOLINT(bugprone-unchecked-optional-access)
                        //   (*import_it)->get_name());
                    }
                }
                else if(instr->get_name() == "new")
                {
                    if(instr->get_args().size() != 1)
                    {
                        throw emitter_error(std::format("Expected 1 argument for 'new', got {}.", instr->get_args().size()));
                    }

                    const auto* arg = static_cast<const cg::type_argument*>(instr->get_args()[0].get());    // NOLINT(cppcoreguidelines-pro-type-static-cast-downcast)

                    const auto& import_path = arg->get_import_path();
                    if(import_path.has_value())
                    {
                        auto import_it = std::ranges::find_if(
                          sema_env.symbol_table,
                          [arg, import_path, this](const auto& p) -> bool
                          {
                              auto symbol_info_it = sema_env.symbol_table.find(p.first);
                              if(symbol_info_it == sema_env.symbol_table.end())
                              {
                                  throw emitter_error(
                                    std::format(
                                      "Could not find symbol for module with id {} for symbol '{}'.",
                                      symbol_info_it->second.declaring_module.value,
                                      symbol_info_it->second.qualified_name));
                              }

                              if(symbol_info_it->second.declaring_module == sema::symbol_info::current_module_id)
                              {
                                  return false;
                              }

                              auto declaring_module_symbol_it = sema_env.symbol_table.find(symbol_info_it->second.declaring_module);
                              if(declaring_module_symbol_it == sema_env.symbol_table.end())
                              {
                                  throw emitter_error(
                                    std::format(
                                      "Could not find symbol for module with id {} for symbol '{}'.",
                                      symbol_info_it->second.declaring_module.value,
                                      symbol_info_it->second.qualified_name));
                              }

                              return import_path.value() == declaring_module_symbol_it->second.qualified_name
                                     && symbol_info_it->second.name == arg->get_value()->get_name().value();
                          });

                        if(import_it == sema_env.symbol_table.end())
                        {
                            throw emitter_error(
                              std::format(
                                "Could not resolve imported type '{}'.",
                                arg->get_value()->get_name().value_or("<unknown>")));
                        }

                        codegen_ctx.add_import(
                          module_::symbol_type::type,
                          import_path.value(),
                          arg->get_value()->get_name().value());
                    }
                }
            }
        }
    }

    for(const auto& m: macro_env.macros)
    {
        const auto& desc = m->get_desc();
        if(!desc.serialized_ast.has_value())
        {
            throw emitter_error(
              std::format(
                "Macro '{}' has empty AST.",
                m->get_name()));
        }

        memory_read_archive ar{
          desc.serialized_ast.value(),
          true,
          std::endian::little};

        std::unique_ptr<ast::expression> macro_ast;
        ar& ast::expression_serializer{macro_ast};

        macro_ast->visit_nodes(
          [this](const ast::expression& e) -> void
          {
              if(e.is_macro_invocation()
                 && e.get_namespace_path().has_value())
              {
                  codegen_ctx.add_import(
                    module_::symbol_type::macro,
                    e.get_namespace_path().value(),    // NOLINT(bugprone-unchecked-optional-access)
                    e.as_named_expression()->get_name().s);
              }
          },
          true,
          false);
    }
}

/**
 * Get the import index of a type, or `std::nullopt` if it is not an import.
 *
 * @param ctx The codegen context holding the imports.
 * @param t The type.
 * @returns Returns an index into the context's import table, or `std::nullopt`.
 */
static std::optional<std::size_t> get_import_index_or_nullopt(
  ty::context& type_ctx,
  cg::context& codegen_ctx,
  const cg::type& t)
{
    if(!t.get_type_id().has_value())
    {
        throw emitter_error(
          "Type had no id during emission.");
    }

    auto info = type_ctx.get_type_info(
      type_ctx.get_base_type(t.get_type_id().value()));

    if(info.kind != ty::type_kind::struct_)
    {
        return std::nullopt;
    }

    std::print("type id: {},  type kind: {}\n",
               t.get_type_id().value(),
               ty::to_string(info.kind));

    throw std::runtime_error("get_import_index_or_nullopt");

    // if(t.is_import())
    // {
    //     return std::make_optional(codegen_ctx.get_import_index(
    //       module_::symbol_type::type,
    //       t.get_import_path().value(),    // NOLINT(bugprone-unchecked-optional-access)
    //       t.base_type().to_string()));
    // }
    // return std::nullopt;
}

void instruction_emitter::emit_instruction(const std::unique_ptr<cg::function>& func, const std::unique_ptr<cg::instruction>& instr)
{
    const auto& name = instr->get_name();
    const auto& args = instr->get_args();

    auto expect_arg_size = [&name, &args](std::size_t s1, std::optional<std::size_t> s2 = std::nullopt)
    {
        if(args.size() != s1)
        {
            if(!s2.has_value())
            {
                throw emitter_error(std::format("Expected {} argument(s) for '{}', got {}.", s1, name, args.size()));
            }

            if(args.size() != *s2)
            {
                throw emitter_error(std::format("Expected {} or {} argument(s) for '{}', got {}.", s1, *s2, name, args.size()));
            }
        }
    };

    auto emit_typed =
      [this, &name, &args, expect_arg_size](
        opcode i32_opcode,
        std::optional<opcode> f32_opcode = std::nullopt,
        std::optional<opcode> str_opcode = std::nullopt,
        std::optional<opcode> array_opcode = std::nullopt,
        std::optional<opcode> ref_opcode = std::nullopt)
    {
        expect_arg_size(1);

        const auto* arg = static_cast<const cg::const_argument*>(args[0].get());    // NOLINT(cppcoreguidelines-pro-type-static-cast-downcast)
        const cg::value* v = arg->get_value();

        if(array_opcode.has_value()
           && type_ctx.is_array(v->get_type().get_type_id().value()))
        {
            emit(instruction_buffer, *array_opcode);
            return;
        }

        if(v->get_type().get_type_kind() == cg::type_kind::i32)
        {
            emit(instruction_buffer, i32_opcode);
        }
        else if(v->get_type().get_type_kind() == cg::type_kind::f32)
        {
            if(!f32_opcode.has_value())
            {
                throw std::runtime_error(std::format("Invalid type 'f32' for instruction '{}'.", name));
            }

            emit(instruction_buffer, *f32_opcode);
        }
        else if(v->get_type().get_type_kind() == cg::type_kind::str)
        {
            if(!str_opcode.has_value())
            {
                throw std::runtime_error(std::format("Invalid type 'str' for instruction '{}'.", name));
            }

            emit(instruction_buffer, *str_opcode);
        }
        else if(type_ctx.is_nullable(v->get_type().get_type_id().value()))
        {
            if(!ref_opcode.has_value())
            {
                throw std::runtime_error(std::format("Invalid reference type for instruction '{}'.", name));
            }

            emit(instruction_buffer, *ref_opcode);
        }
        else
        {
            throw std::runtime_error(std::format("Invalid type '{}' for instruction '{}'.", v->get_type().to_string(), name));
        }
    };

    auto emit_typed_one_arg = [this, &name, &args, expect_arg_size](opcode i32_opcode, opcode f32_opcode, std::optional<opcode> str_opcode = std::nullopt)
    {
        expect_arg_size(1);

        const auto* arg = static_cast<const cg::const_argument*>(args[0].get());    // NOLINT(cppcoreguidelines-pro-type-static-cast-downcast)
        const cg::value* v = arg->get_value();

        if(v->get_type().get_type_kind() == cg::type_kind::i32)
        {
            emit(
              instruction_buffer,
              i32_opcode,
              static_cast<std::int32_t>(static_cast<const cg::constant_i32*>(v)->get_int()));    // NOLINT(cppcoreguidelines-pro-type-static-cast-downcast)
        }
        else if(v->get_type().get_type_kind() == cg::type_kind::f32)
        {
            emit(
              instruction_buffer,
              f32_opcode,
              static_cast<const cg::constant_f32*>(v)->get_float());    // NOLINT(cppcoreguidelines-pro-type-static-cast-downcast)
        }
        else if(v->get_type().get_type_kind() == cg::type_kind::str)
        {
            if(!str_opcode.has_value())
            {
                throw std::runtime_error(std::format("Invalid type 'str' for instruction '{}'.", name));
            }

            // FIXME Explicitly construct the constant table first and get the id's from there.
            const_::constant_id id = static_cast<const cg::constant_str*>(v)->get_id();
            emit(
              instruction_buffer,
              *str_opcode,
              vle_int{static_cast<std::int64_t>(id)});    // NOLINT(cppcoreguidelines-pro-type-static-cast-downcast)
        }
        else
        {
            throw std::runtime_error(std::format("Invalid type '{}' for instruction '{}'.", v->get_type().to_string(), name));
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

        const auto* arg = static_cast<const cg::variable_argument*>(args[0].get());    // NOLINT(cppcoreguidelines-pro-type-static-cast-downcast)
        const cg::value* v = arg->get_value();

        if(!v->has_name())
        {
            throw std::runtime_error(std::format("Cannot emit instruction '{}': Argument value has no name.", name));
        }
        vle_int index{
          utils::numeric_cast<std::int64_t>(
            func->get_scope()->get_index(v->get_name().value()))};

        if(type_ctx.is_array(v->get_type().get_type_id().value()))
        {
            if(!str_array_opcode.has_value())
            {
                throw std::runtime_error(std::format("Invalid type '{}' for instruction '{}'.", v->get_type().to_string(), name));
            }

            emit(instruction_buffer, *str_array_opcode);
        }
        else if(v->get_type().get_type_kind() == cg::type_kind::i32)
        {
            emit(instruction_buffer, i32_opcode);
        }
        else if(v->get_type().get_type_kind() == cg::type_kind::f32)
        {
            emit(instruction_buffer, f32_opcode);
        }
        else if(v->get_type().get_type_kind() == cg::type_kind::str)
        {
            if(!str_array_opcode.has_value())
            {
                throw std::runtime_error(std::format("Invalid type 'str' for instruction '{}'.", name));
            }

            emit(instruction_buffer, *str_array_opcode);
        }
        else if(type_ctx.is_reference(v->get_type().get_type_id().value()))
        {
            if(!ref_opcode.has_value())
            {
                throw std::runtime_error(std::format("Invalid reference type for instruction '{}'.", name));
            }

            emit(instruction_buffer, *ref_opcode);
        }
        else
        {
            throw std::runtime_error(std::format("Invalid type '{}' for instruction '{}'.", v->get_type().to_string(), name));
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
        emit_typed(opcode::iaload, opcode::faload, opcode::aaload, std::nullopt, opcode::aaload);
    }
    else if(name == "store_element")
    {
        emit_typed(opcode::iastore, opcode::fastore, opcode::aastore, std::nullopt, opcode::aastore);
    }
    else if(name == "dup")
    {
        if(args.size() == 1)
        {
            emit_typed(opcode::idup, opcode::fdup, std::nullopt, opcode::adup, opcode::adup);
        }
        else if(args.size() == 2)
        {
            // get the duplicated value.
            const auto* v_arg = static_cast<const cg::type_argument*>(args[0].get());    // NOLINT(cppcoreguidelines-pro-type-static-cast-downcast)
            const cg::value* v = v_arg->get_value();

            module_::variable_type v_type = {
              v->get_type().to_string(),
              std::nullopt,
              std::nullopt,
              get_import_index_or_nullopt(
                type_ctx,
                codegen_ctx,
                v->get_type())};

            // get the stack arguments.
            const auto* stack_arg = static_cast<const cg::type_argument*>(args[1].get());    // NOLINT(cppcoreguidelines-pro-type-static-cast-downcast)
            module_::variable_type s_type = stack_arg->get_value()->get_type().to_string();

            // emit instruction.
            emit(instruction_buffer, opcode::dup_x1);
            instruction_buffer & v_type & s_type;
        }
        else if(args.size() == 3)
        {
            // get the duplicated value.
            const auto* v_arg = static_cast<const cg::type_argument*>(args[0].get());    // NOLINT(cppcoreguidelines-pro-type-static-cast-downcast)
            const cg::value* v = v_arg->get_value();

            module_::variable_type v_type = {
              v->get_type().to_string(),
              std::nullopt,
              std::nullopt,
              get_import_index_or_nullopt(
                type_ctx,
                codegen_ctx,
                v->get_type())};

            // get the stack arguments.
            const auto* stack_arg = static_cast<const cg::type_argument*>(args[1].get());    // NOLINT(cppcoreguidelines-pro-type-static-cast-downcast)
            const cg::value* s1v = stack_arg->get_value();

            module_::variable_type s_type1 = {
              s1v->to_string(),
              std::nullopt,
              std::nullopt,
              get_import_index_or_nullopt(
                type_ctx,
                codegen_ctx,
                s1v->get_type())};

            stack_arg = static_cast<const cg::type_argument*>(args[2].get());    // NOLINT(cppcoreguidelines-pro-type-static-cast-downcast)
            const cg::value* s2v = stack_arg->get_value();

            module_::variable_type s_type2 = {
              s2v->to_string(),
              std::nullopt,
              std::nullopt,
              get_import_index_or_nullopt(
                type_ctx,
                codegen_ctx,
                s2v->get_type())};

            // emit instruction.
            emit(instruction_buffer, opcode::dup_x2);
            instruction_buffer & v_type & s_type1 & s_type2;
        }
        else
        {
            throw emitter_error(std::format("Unexpected argument count ({}) for 'dup'.", args.size()));
        }
    }
    else if(name == "pop")
    {
        emit_typed(opcode::pop, opcode::pop, opcode::apop, opcode::apop, opcode::apop);    // same instruction for i32 and f32.
    }
    else if(name == "cast")
    {
        expect_arg_size(1);
        const auto* arg = static_cast<const cg::cast_argument*>(args[0].get());    // NOLINT(cppcoreguidelines-pro-type-static-cast-downcast)
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
            throw emitter_error(std::format("Invalid cast type '{}'.", ::cg::to_string(arg->get_cast())));
        }
    }
    else if(name == "invoke")
    {
        expect_arg_size(1);
        const auto* arg = static_cast<const cg::function_argument*>(args[0].get());    // NOLINT(cppcoreguidelines-pro-type-static-cast-downcast)
        const cg::value* v = arg->get_value();

        const auto info = get_import_info(arg->get_symbol_id(), sema_env);
        if(!info.has_value())
        {
            // resolve module-local functions.
            vle_int index{
              utils::numeric_cast<std::int64_t>(
                exports.get_index(
                  module_::symbol_type::function,
                  v->get_name().value_or("<invalid-name>")))};
            emit(instruction_buffer, opcode::invoke);
            instruction_buffer & index;
            return;
        }

        // TODO
        throw std::runtime_error("instruction_emitter::emit_instruction");

        // resolve imports.
        // auto import_it = std::ranges::find_if(
        //   codegen_ctx.prototypes,
        //   [arg, &info](const std::unique_ptr<cg::prototype>& p) -> bool
        //   {
        //       return p->is_import()
        //              && *p->get_import_path() == info.value().qualified_module_name
        //              && p->get_name() == *arg->get_value()->get_name();
        //   });
        // if(import_it == codegen_ctx.prototypes.end())
        // {
        //     throw emitter_error(
        //       std::format(
        //         "Could not resolve imported function '{}'.",
        //         arg->get_value()->get_name().value_or("<invalid-name>")));
        // }

        // vle_int index{
        //   -utils::numeric_cast<std::int64_t>(
        //     codegen_ctx.get_import_index(
        //       module_::symbol_type::function,
        //       (*import_it)->get_import_path().value_or("<invalid-import-path>"),
        //       (*import_it)->get_name()))
        //   - 1};
        // emit(instruction_buffer, opcode::invoke);
        // instruction_buffer & index;
    }
    else if(name == "ret")
    {
        expect_arg_size(1);

        if(args[0]->get_value()->get_type().get_type_kind() == cg::type_kind::void_)
        {
            // return void from a function.
            emit(instruction_buffer, opcode::ret);
        }
        else
        {
            emit_typed(opcode::iret, opcode::fret, opcode::sret, opcode::aret, opcode::aret);
        }
    }
    else if(name == "set_field"
            || name == "get_field")
    {
        expect_arg_size(1);

        const auto* arg = static_cast<const cg::field_access_argument*>(args[0].get());    // NOLINT(cppcoreguidelines-pro-type-static-cast-downcast)

        // resolve references to struct and field name.
        vle_int struct_index{0};
        vle_int field_index{utils::numeric_cast<std::int64_t>(arg->get_field_index())};

        const auto struct_import_info = get_import_info(arg->get_struct_type_id(), sema_env);

        if(struct_import_info.has_value())
        {
            // find struct in import table.
            auto import_it = std::ranges::find_if(
              codegen_ctx.imports,
              [&struct_import_info](const cg::imported_symbol& s) -> bool
              {
                  return s.type == module_::symbol_type::type
                         && s.name == struct_import_info.value().name
                         && s.import_path == struct_import_info.value().qualified_module_name;
              });
            if(import_it == codegen_ctx.imports.end())
            {
                throw emitter_error(std::format(
                  "Cannot find type '{}' from package '{}' in import table.",
                  struct_import_info.value().name,
                  struct_import_info.value().qualified_module_name));
            }
            struct_index = vle_int{-std::distance(codegen_ctx.imports.begin(), import_it) - 1};
        }
        else
        {
            // FIXME Use symbol id instead of name.
            const auto symbol_it = std::ranges::find_if(
              sema_env.type_map,
              [arg](const auto& p) -> bool
              {
                  return p.second == arg->get_struct_type_id();
              });
            if(symbol_it == sema_env.type_map.end())
            {
                throw emitter_error(
                  std::format(
                    "Could not find symbol id for type with id {}.",
                    arg->get_struct_type_id()));
            }

            const auto symbol_info_it = sema_env.symbol_table.find(symbol_it->first);
            if(symbol_info_it == sema_env.symbol_table.end())
            {
                throw emitter_error(
                  std::format(
                    "Could not find symbol info for symbol with id {}.",
                    symbol_it->first.value));
            }

            if(symbol_info_it->second.declaring_module != sema::symbol_info::current_module_id)
            {
                throw emitter_error(
                  std::format(
                    "Symbol with id {} is not module-local.",
                    symbol_it->first.value));
            }

            // find struct in export table.
            struct_index = vle_int{utils::numeric_cast<std::int64_t>(
              exports.get_index(
                module_::symbol_type::type,
                symbol_info_it->second.name))};
        }

        if(name == "setfield")
        {
            emit(instruction_buffer, opcode::setfield);
        }
        else if(name == "getfield")
        {
            emit(instruction_buffer, opcode::getfield);
        }
        else
        {
            throw std::runtime_error(std::format("Unknown instruction '{}'.", name));
        }
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

        const auto& args = instr->get_args();

        const auto& then_label = static_cast<const cg::label_argument*>(args[0].get())->get_label();    // NOLINT(cppcoreguidelines-pro-type-static-cast-downcast)
        const auto& else_label = static_cast<const cg::label_argument*>(args[1].get())->get_label();    // NOLINT(cppcoreguidelines-pro-type-static-cast-downcast)

        auto then_it = jump_targets.find(then_label);
        if(then_it == jump_targets.end())
        {
            throw emitter_error(std::format("Cannot find label '{}'.", then_label));
        }

        auto else_it = jump_targets.find(else_label);
        if(else_it == jump_targets.end())
        {
            throw emitter_error(std::format("Cannot find label '{}'.", else_label));
        }

        vle_int then_index{std::distance(jump_targets.begin(), then_it)};
        vle_int else_index{std::distance(jump_targets.begin(), else_it)};

        emit(instruction_buffer, opcode::jnz);
        instruction_buffer & then_index;
        instruction_buffer & else_index;
    }
    else if(name == "jmp")
    {
        expect_arg_size(1);

        const auto& args = instr->get_args();
        const auto& label = static_cast<cg::label_argument*>(args[0].get())->get_label();    // NOLINT(cppcoreguidelines-pro-type-static-cast-downcast)

        auto it = jump_targets.find(label);
        if(it == jump_targets.end())
        {
            throw emitter_error(std::format("Cannot find label '{}'.", label));
        }

        vle_int index{std::distance(jump_targets.begin(), it)};
        emit(instruction_buffer, opcode::jmp);
        instruction_buffer & index;
    }
    else if(name == "new"
            || name == "anewarray"
            || name == "checkcast")
    {
        expect_arg_size(1);

        const auto* arg = static_cast<const cg::type_argument*>(args[0].get());    // NOLINT(cppcoreguidelines-pro-type-static-cast-downcast)
        const cg::type type = arg->get_value()->get_type();                        // NOLINT(cppcoreguidelines-pro-type-static-cast-downcast)

        // resolve type to index.
        vle_int struct_index{0};

        const auto struct_import_info = get_import_info(type.get_type_id().value(), sema_env);

        if(struct_import_info.has_value())
        {
            // find struct in import table.
            auto import_it = std::ranges::find_if(
              codegen_ctx.imports,
              [&struct_import_info](const cg::imported_symbol& s) -> bool
              {
                  return s.type == module_::symbol_type::type
                         && s.name == struct_import_info.value().name
                         && s.import_path == struct_import_info.value().qualified_module_name;
              });
            if(import_it == codegen_ctx.imports.end())
            {
                throw emitter_error(std::format(
                  "Cannot find type '{}' from package '{}' in import table.",
                  struct_import_info.value().name,
                  struct_import_info.value().qualified_module_name));
            }
            struct_index = vle_int{-std::distance(codegen_ctx.imports.begin(), import_it) - 1};
        }
        else
        {
            // FIXME Use symbol id instead of name.
            const auto symbol_it = std::ranges::find_if(
              sema_env.type_map,
              [type](const auto& p) -> bool
              {
                  return p.second == type.get_type_id().value();
              });
            if(symbol_it == sema_env.type_map.end())
            {
                throw emitter_error(
                  std::format(
                    "Could not find symbol id for type with id {}.",
                    type.get_type_id().value()));
            }

            const auto symbol_info_it = sema_env.symbol_table.find(symbol_it->first);
            if(symbol_info_it == sema_env.symbol_table.end())
            {
                throw emitter_error(
                  std::format(
                    "Could not find symbol info for symbol with id {}.",
                    symbol_it->first.value));
            }

            if(symbol_info_it->second.declaring_module != sema::symbol_info::current_module_id)
            {
                throw emitter_error(
                  std::format(
                    "Symbol with id {} is not module-local.",
                    symbol_it->first.value));
            }

            // find struct in export table.
            struct_index = vle_int{utils::numeric_cast<std::int64_t>(
              exports.get_index(
                module_::symbol_type::type,
                symbol_info_it->second.name))};
        }

        if(name == "new")
        {
            emit(instruction_buffer, opcode::new_);
        }
        else if(name == "anewarray")
        {
            emit(instruction_buffer, opcode::anewarray);
        }
        else if(name == "checkcast")
        {
            emit(instruction_buffer, opcode::checkcast);
        }
        else
        {
            throw std::runtime_error(std::format("Unknown instruction '{}'.", name));
        }

        instruction_buffer & struct_index;
    }
    else if(name == "newarray")
    {
        expect_arg_size(1);

        const auto& args = instr->get_args();
        auto type_kind = static_cast<cg::type_argument*>(args[0].get())->get_value()->get_type().get_type_kind();    // NOLINT(cppcoreguidelines-pro-type-static-cast-downcast)

        module_::array_type type = [&type_kind, &args]() -> module_::array_type
        {
            if(type_kind == cg::type_kind::i32)
            {
                return module_::array_type::i32;
            }

            if(type_kind == cg::type_kind::f32)
            {
                return module_::array_type::f32;
            }

            if(type_kind == cg::type_kind::str)
            {
                return module_::array_type::str;
            }

            throw std::runtime_error(std::format(
              "Unknown array type '{}' for newarray.",
              static_cast<cg::type_argument*>(args[0].get())->get_value()->get_type().to_string()));    // NOLINT(cppcoreguidelines-pro-type-static-cast-downcast)
        }();

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
        throw std::runtime_error(std::format("instruction_emitter::emit_instruction: instruction generation for '{}' not implemented.", name));
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
    std::size_t import_count = codegen_ctx.imports.size();

    // exported constants.
    constant_table.clear();
    constant_table.reserve(const_env.const_literal_map.size());

    for(const auto& [id, info]: const_env.const_info_map)
    {
        // FIXME Export constants.
        std::print("FIXME constant table: {}\n", sema_env.symbol_table[id].name);

        // const auto& c = codegen_ctx.constants[i];
        // if(!c.add_to_exports)
        // {
        //     continue;
        // }
        // if(!c.name.has_value())
        // {
        //     throw emitter_error("Cannot export a constant without a name.");
        // }

        // exports.add_constant(c.name.value(), i);
    }

    // exported types.
    for(const auto& [id, info]: type_ctx.get_type_info_map())
    {
        if(info.kind != ty::type_kind::struct_)
        {
            continue;
        }

        // TODO Filter imported struct definitions.

        const auto& struct_info = std::get<ty::struct_info>(info.data);
        exports.add_struct(struct_info);
    }

    // for(const auto& it: codegen_ctx.types)
    // {
    //     if(it->is_import())
    //     {
    //         // Verify that the type is in the import table.
    //         auto import_it = std::ranges::find_if(
    //           std::as_const(codegen_ctx.imports),
    //           [&it](const cg::imported_symbol& s) -> bool
    //           {
    //               return s.type == module_::symbol_type::type
    //                      && s.name == it->get_name()
    //                      && s.import_path == it->get_import_path();
    //           });
    //         if(import_it == codegen_ctx.imports.cend())
    //         {
    //             throw std::runtime_error(
    //               std::format(
    //                 "Type '{}' from package '{}' not found in import table.",
    //                 it->get_name(),
    //                 it->get_import_path().value_or("<invalid-import-path>")));
    //         }
    //     }
    //     else
    //     {
    //         exports.add_type(it);
    //     }
    // }

    // exported functions.
    for(const auto& f: codegen_ctx.funcs)
    {
        auto [signature_ret_type, signature_arg_types] = f->get_signature();

        auto type_id = signature_ret_type.get_type_id().value();
        auto base_type_id = type_ctx.get_base_type(type_id);
        bool is_array = type_ctx.is_array(type_id);
        auto array_rank = type_ctx.get_array_rank(type_id);

        module_::variable_type return_type = {
          type_ctx.to_string(base_type_id),
          is_array
            ? std::make_optional(array_rank)
            : std::nullopt,
          std::nullopt,
          get_import_index_or_nullopt(
            type_ctx,
            codegen_ctx,
            signature_ret_type)};

        std::vector<module_::variable_type> arg_types =
          signature_arg_types
          | std::views::transform(
            [this](const cg::type& t) -> module_::variable_type
            { 
                auto type_id = t.get_type_id().value();
                auto base_type_id = type_ctx.get_base_type(type_id);
                bool is_array = type_ctx.is_array(type_id);
                auto array_rank = type_ctx.get_array_rank(type_id);
                
                return {
                    type_ctx.to_string(base_type_id),
                    is_array 
                      ? std::make_optional(array_rank) 
                      : std::nullopt,
                    std::nullopt,
                    get_import_index_or_nullopt(
                        type_ctx,
                        codegen_ctx, 
                        t)}; })
          | std::ranges::to<std::vector>();

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

    // exported macros.
    for(const auto& m: macro_env.macros)
    {
        if(m->is_import())
        {
            // Macros are only valid at compile-time, so
            // they should not appear in the import table.
            auto import_it = std::ranges::find_if(
              std::as_const(codegen_ctx.imports),
              [&m](const cg::imported_symbol& s) -> bool
              {
                  return s.type == module_::symbol_type::macro
                         && s.name == m->get_name()
                         && s.import_path == m->get_import_path();
              });
            if(import_it != codegen_ctx.imports.cend())
            {
                throw std::runtime_error(
                  std::format(
                    "Macro '{}' from package '{}' should not appear in import table.",
                    m->get_name(),
                    m->get_import_path().value_or("<invalid-import-path>")));
            }
        }
        else
        {
            exports.add_macro(m->get_name(), m->get_desc());
        }
    }

    // the export count is not allowed to change after this point, so store it here and check later.
    std::size_t export_count = exports.size();

    /*
     * generate bytecode.
     */
    for(auto& f: codegen_ctx.funcs)
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
        const auto& func_args = s->get_args();
        const auto& func_locals = f->get_scope()->get_locals();
        const std::size_t local_count = func_args.size() + func_locals.size();

        std::vector<module_::variable_descriptor> locals{local_count};

        std::set<std::size_t> unset_indices;
        for(std::size_t i = 0; i < local_count; ++i)
        {
            unset_indices.insert(unset_indices.end(), i);
        }

        for(const auto& it: func_args)
        {
            auto name = it->get_name();
            if(!name.has_value())
            {
                throw emitter_error(std::format("Unnamed variable in function '{}'.", f->get_name()));
            }

            std::size_t index = s->get_index(it->get_name().value_or("<invalid-name>"));
            if(!unset_indices.contains(index))
            {
                throw emitter_error(std::format("Tried to map local '{}' with index '{}' multiple times.", *name, index));
            }
            unset_indices.erase(index);

            auto t = it->get_type();

            auto type_id = t.get_type_id().value();
            auto base_type_id = type_ctx.get_base_type(type_id);
            bool is_array = type_ctx.is_array(type_id);
            auto array_rank = type_ctx.get_array_rank(type_id);

            locals[index] = module_::variable_descriptor{
              module_::variable_type{
                type_ctx.to_string(base_type_id),
                is_array
                  ? std::make_optional(array_rank)
                  : std::nullopt,
                std::nullopt,
                get_import_index_or_nullopt(
                  type_ctx,
                  codegen_ctx,
                  t)}};
        }

        for(const auto& it: func_locals)
        {
            auto name = it->get_name();
            if(!name.has_value())
            {
                throw emitter_error(std::format("Unnamed variable in function '{}'.", f->get_name()));
            }

            std::size_t index = s->get_index(it->get_name().value_or("<invalid-name>"));
            if(!unset_indices.contains(index))
            {
                throw emitter_error(std::format("Tried to map local '{}' with index '{}' multiple times.", *name, index));
            }
            unset_indices.erase(index);

            auto t = it->get_type();

            auto type_id = t.get_type_id().value();
            auto base_type_id = type_ctx.get_base_type(type_id);
            bool is_array = type_ctx.is_array(type_id);
            auto array_rank = type_ctx.get_array_rank(type_id);

            locals[index] = module_::variable_descriptor{
              module_::variable_type{
                type_ctx.to_string(base_type_id),
                is_array
                  ? std::make_optional(array_rank)
                  : std::nullopt,
                std::nullopt,
                get_import_index_or_nullopt(
                  type_ctx,
                  codegen_ctx,
                  t)}};
        }

        if(!unset_indices.empty())
        {
            throw emitter_error(std::format("Inconsistent local count for function '{}'.", f->get_name()));
        }

        /*
         * instruction generation.
         */
        std::size_t entry_point = instruction_buffer.tell();

        for(const auto& it: f->get_basic_blocks())
        {
            auto jump_it = jump_targets.find(it->get_label());
            if(jump_it != jump_targets.end())
            {
                emit(instruction_buffer, opcode::label);

                vle_int index{std::distance(jump_targets.begin(), jump_it)};
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
    if(import_count != codegen_ctx.imports.size())
    {
        throw emitter_error(
          std::format("Import count changed during instruction emission ({} -> {}).",
                      import_count, codegen_ctx.imports.size()));
    }
    if(export_count != exports.size())
    {
        throw emitter_error(
          std::format("Export count changed during instruction emission ({} -> {}).",
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
    for(const auto& it: codegen_ctx.imports)
    {
        if(it.type == module_::symbol_type::package)
        {
            auto p_it = std::ranges::find(std::as_const(packages), it.name);
            if(p_it != packages.cend())
            {
                packages.erase(p_it);
            }
        }
        else
        {
            // ensure we import the symbol's package.
            auto p_it = std::ranges::find_if(
              std::as_const(codegen_ctx.imports),
              [&it](const auto& s) -> bool
              {
                  return s.type == module_::symbol_type::package && s.name == it.import_path;
              });
            if(p_it == codegen_ctx.imports.cend())
            {
                auto p_it = std::ranges::find(std::as_const(packages), it.name);
                if(p_it == packages.cend())
                {
                    packages.push_back(it.import_path);
                }
            }
        }
    }

    // Write import table. Additional packages from the search above are appended.
    std::vector<cg::imported_symbol> template_header;
    auto add_symbol_to_template = [&template_header](const cg::imported_symbol& s)
    {
        if(std::ranges::find_if(
             std::as_const(template_header),
             [&s](const cg::imported_symbol& it)
             {
                 return s.type == it.type
                        && s.name == it.name
                        && s.import_path == it.import_path;
             })
           == template_header.cend())
        {
            template_header.emplace_back(s);
        }
    };

    for(const auto& it: codegen_ctx.imports)
    {
        add_symbol_to_template(it);
    }
    for(const auto& it: packages)
    {
        add_symbol_to_template({module_::symbol_type::package, it, std::string{}});
    }

    for(const auto& it: template_header)
    {
        if(it.type == module_::symbol_type::package)
        {
            mod.add_import(it.type, it.name);
        }
        else
        {
            auto import_it = std::ranges::find_if(
              std::as_const(template_header),
              [&it](const auto& s) -> bool
              {
                  return s.type == module_::symbol_type::package && s.name == it.import_path;
              });

            if(import_it == template_header.cend())
            {
                throw std::runtime_error(std::format("Package '{}' not found in package table.", it.import_path));
            }

            mod.add_import(it.type, it.name, std::distance(template_header.cbegin(), import_it));
        }
    }

    /*
     * Constants.
     */
    std::vector<cg::constant_table_entry> exported_constants;
    std::ranges::copy_if(
      std::as_const(constant_table),
      std::back_inserter(exported_constants),
      [](const cg::constant_table_entry& entry) -> bool
      {
          return !entry.import_path.has_value();
      });

    std::vector<module_::constant_table_entry> constants =
      exported_constants
      | std::views::transform(
        [](const cg::constant_table_entry& entry) -> module_::constant_table_entry
        {
            return {entry.type, entry.data};
        })
      | std::ranges::to<std::vector>();

    mod.set_constant_table(constants);

    /*
     * Exports.
     */
    exports.write(mod);

    /*
     * Instructions.
     */
    mod.set_binary(instruction_buffer.get_buffer());

    return mod;
}

}    // namespace slang
