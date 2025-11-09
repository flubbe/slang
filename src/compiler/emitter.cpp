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
#include "name_utils.h"
#include "utils.h"

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
 * import_table_builder implementation.
 */

std::size_t import_table_builder::intern_package(
  const std::string& qualified_name)
{
    auto it = std::ranges::find_if(
      import_table,
      [&qualified_name](const module_::imported_symbol& entry) -> bool
      {
          return entry.type == module_::symbol_type::package
                 && entry.name == qualified_name;
      });
    if(it != import_table.end())
    {
        return std::distance(import_table.begin(), it);
    }

    import_table.emplace_back(
      module_::symbol_type::package,
      qualified_name);
    return import_table.size() - 1;
}

std::size_t import_table_builder::get_package(
  const std::string& qualified_name) const
{
    auto it = std::ranges::find_if(
      import_table,
      [&qualified_name](const module_::imported_symbol& entry) -> bool
      {
          return entry.type == module_::symbol_type::package
                 && entry.name == qualified_name;
      });
    if(it == import_table.end())
    {
        throw emitter_error(
          std::format(
            "Package '{}' could not be found in import table.",
            qualified_name));
    }

    return std::distance(import_table.begin(), it);
}

std::size_t import_table_builder::intern_function(
  std::size_t package_index,
  const std::string& name)
{
    auto it = std::ranges::find_if(
      import_table,
      [package_index, &name](const module_::imported_symbol& entry) -> bool
      {
          return entry.type == module_::symbol_type::function
                 && entry.package_index == package_index
                 && entry.name == name;
      });
    if(it != import_table.end())
    {
        return std::distance(import_table.begin(), it);
    }

    if(package_index >= import_table.size())
    {
        throw emitter_error(
          std::format(
            "Package index {} out of bounds.",
            package_index));
    }

    if(import_table[package_index].type != module_::symbol_type::package)
    {
        throw emitter_error(
          std::format(
            "Import index {} is not a package.",
            package_index));
    }

    import_table.emplace_back(
      module_::symbol_type::function,
      name,
      package_index);
    return import_table.size() - 1;
}

std::size_t import_table_builder::get_function(
  std::size_t package_index,
  const std::string& name) const
{
    auto it = std::ranges::find_if(
      import_table,
      [package_index, &name](const module_::imported_symbol& entry) -> bool
      {
          return entry.type == module_::symbol_type::function
                 && entry.package_index == package_index
                 && entry.name == name;
      });
    if(it == import_table.end())
    {
        throw emitter_error(
          std::format(
            "Function '{}' with package index '{}' could not be found in import table.",
            name,
            package_index));
    }

    return std::distance(import_table.begin(), it);
}

std::size_t import_table_builder::intern_struct(
  const std::string& qualified_name)
{
    auto package_name = name::module_path(qualified_name);
    std::size_t package_index = intern_package(
      std::string{package_name});

    const std::string struct_name = name::unqualified_name(qualified_name);

    auto it = std::ranges::find_if(
      import_table,
      [&struct_name, package_index](const module_::imported_symbol& entry) -> bool
      {
          return entry.type == module_::symbol_type::type
                 && entry.package_index == package_index
                 && entry.name == struct_name;
      });
    if(it != import_table.end())
    {
        return std::distance(import_table.begin(), it);
    }

    // Add the struct.
    import_table.emplace_back(
      module_::symbol_type::type,
      struct_name,
      package_index);
    return import_table.size() - 1;
}

std::size_t import_table_builder::get_struct(
  std::size_t package_index,
  const std::string& name) const
{
    auto it = std::ranges::find_if(
      import_table,
      [package_index, &name](const module_::imported_symbol& entry) -> bool
      {
          return entry.type == module_::symbol_type::type
                 && entry.package_index == package_index
                 && entry.name == name;
      });
    if(it == import_table.end())
    {
        throw emitter_error(
          std::format(
            "Struct '{}' with package index '{}' could not be found in import table.",
            name,
            package_index));
    }

    return std::distance(import_table.begin(), it);
}

void import_table_builder::write(module_::language_module& mod) const
{
    for(const auto& it: import_table)
    {
        mod.add_import(
          it.type,
          it.name,
          it.package_index);
    }
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

void export_table_builder::add_struct(
  const instruction_emitter& emitter,
  const ty::struct_info& type)
{
    if(std::ranges::find_if(
         export_table,
         [&type](const module_::exported_symbol& entry) -> bool
         { return entry.type == module_::symbol_type::type
                  && entry.name == type.name; })
       != export_table.end())
    {
        throw emitter_error(
          std::format(
            "Cannot add type to export table: '{}' already exists.",
            type.name));
    }

    std::vector<std::pair<std::string, module_::field_descriptor>> transformed_members =
      type.fields
      | std::views::transform(
        [&emitter](
          const ty::field_info& info) -> std::pair<std::string, module_::field_descriptor>
        {
            const ty::type_info& base_type_info = emitter.type_ctx.get_type_info(
              emitter.type_ctx.get_base_type(info.type));
            std::string base_type;
            std::optional<std::size_t> field_type_import_index{std::nullopt};

            if(base_type_info.kind == ty::type_kind::builtin)
            {
                base_type = emitter.type_ctx.to_string(info.type);
            }
            else if(base_type_info.kind == ty::type_kind::struct_)
            {
                const auto& field_type_info_data = std::get<ty::struct_info>(base_type_info.data);
                bool compare_qualified = field_type_info_data.qualified_name.has_value();

                auto it = std::ranges::find_if(
                  emitter.sema_env.symbol_table,
                  [&field_type_info_data, compare_qualified](const std::pair<sema::symbol_id, sema::symbol_info>& s) -> bool
                  {
                      if(s.second.type != sema::symbol_type::type)
                      {
                          return false;
                      }

                      if(compare_qualified)
                      {
                          return s.second.qualified_name == field_type_info_data.qualified_name.value();
                      }

                      return s.second.name == field_type_info_data.name;
                  });
                if(it == emitter.sema_env.symbol_table.end())
                {
                    throw emitter_error(
                      std::format(
                        "Struct '{}' not found in symbol table.",
                        field_type_info_data.name));
                }

                base_type = name::unqualified_name(it->second.name);

                if(it->second.declaring_module != sema::symbol_info::current_module_id)
                {
                    // get the package index.
                    const auto& package_symbol_info = emitter.sema_env.symbol_table.at(
                      it->second.declaring_module);
                    auto package_index = emitter.imports.get_package(
                      package_symbol_info.qualified_name);

                    // get the field type import index.
                    field_type_import_index = emitter.imports.get_struct(
                      package_index,
                      base_type);
                }
            }
            else
            {
                throw emitter_error(
                  std::format(
                    "Invalid base type kind '{}'.",
                    std::to_underlying(base_type_info.kind)));
            }

            return std::make_pair(
              info.name,
              module_::field_descriptor{
                base_type,
                emitter.type_ctx.is_array(info.type),
                field_type_import_index});
        })
      | std::ranges::to<std::vector>();

    std::uint8_t flags =
      (type.allow_cast ? std::to_underlying(module_::struct_flags::allow_cast) : 0)
      | (type.native ? std::to_underlying(module_::struct_flags::native) : 0);

    export_table.emplace_back(
      module_::symbol_type::type,
      type.name,
      module_::struct_descriptor{
        .flags = flags,
        .member_types = std::move(transformed_members)});
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

std::size_t export_table_builder::get_index(
  module_::symbol_type t,
  const std::string& name) const
{
    auto it = std::ranges::find_if(
      std::as_const(export_table),
      [t, &name](const module_::exported_symbol& entry) -> bool
      { return entry.type == t
               && entry.name == name; });
    if(it == export_table.cend())
    {
        throw emitter_error(
          std::format(
            "Symbol '{}' of type '{}' not found in export table.",
            name,
            to_string(t)));
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
    // walk types.
    for(const auto& [id, symbol_info]: sema_env.symbol_table)
    {
        if(symbol_info.type != sema::symbol_type::type)
        {
            continue;
        }

        if(symbol_info.declaring_module != sema::symbol_info::current_module_id)
        {
            continue;
        }

        const auto& type_info = type_ctx.get_type_info(
          type_ctx.get_type(symbol_info.name));

        const auto& struct_info = std::get<ty::struct_info>(type_info.data);
        for(const auto& field_info: struct_info.fields)
        {
            const auto& field_type_info = type_ctx.get_type_info(field_info.type);
            if(field_type_info.kind != ty::type_kind::struct_)
            {
                continue;
            }

            if(!std::get<ty::struct_info>(field_type_info.data).qualified_name.has_value())
            {
                continue;
            }

            // the linter does not seem to detect the above check.
            // NOLINTNEXTLINE(bugprone-unchecked-optional-access)
            imports.intern_struct(std::get<ty::struct_info>(field_type_info.data).qualified_name.value());
        }
    }

    // walk functions.
    for(auto& f: codegen_ctx.funcs)
    {
        const auto& func_args = f->get_args();
        const auto& func_locals = f->get_locals();

        for(const auto& arg: func_args)
        {
            std::optional<ty::type_id> id = arg.second.get_type_id();
            if(!id.has_value())
            {
                throw emitter_error(
                  std::format(
                    "'{}': Argument has no type id.",
                    f->get_name()));
            }

            ty::type_info info = type_ctx.get_type_info(id.value());
            if(info.kind == ty::type_kind::struct_)
            {
                const auto& struct_info = std::get<ty::struct_info>(info.data);
                if(struct_info.qualified_name.has_value())
                {
                    imports.intern_struct(struct_info.qualified_name.value());
                }
            }
        }

        for(const auto& local: func_locals)
        {
            std::optional<ty::type_id> id = local.second.get_type_id();
            if(!id.has_value())
            {
                throw emitter_error(
                  std::format(
                    "'{}': Local has no type id.",
                    f->get_name()));
            }

            ty::type_info info = type_ctx.get_type_info(id.value());
            if(info.kind == ty::type_kind::struct_)
            {
                const auto& struct_info = std::get<ty::struct_info>(info.data);
                if(struct_info.qualified_name.has_value())
                {
                    imports.intern_struct(struct_info.qualified_name.value());
                }
            }
        }

        std::pair<cg::type, std::vector<cg::type>> sig = f->get_signature();
        std::optional<ty::type_id> return_type_id = sig.first.get_type_id();
        if(!return_type_id.has_value())
        {
            throw emitter_error(
              std::format(
                "'{}': Return type has no type id.",
                f->get_name()));
        }

        ty::type_info return_type_info = type_ctx.get_type_info(return_type_id.value());
        if(return_type_info.kind == ty::type_kind::struct_)
        {
            const auto& struct_info = std::get<ty::struct_info>(return_type_info.data);
            if(struct_info.qualified_name.has_value())
            {
                imports.intern_struct(struct_info.qualified_name.value());
            }
        }

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

                        std::string function_name = name::unqualified_name(
                          symbol_info_it->second.name);
                        std::size_t package_index = imports.intern_package(
                          declaring_module_symbol_it->second.qualified_name);
                        imports.intern_function(
                          package_index,
                          function_name);
                    }
                }
                else if(instr->get_name() == "new"
                        || instr->get_name() == "anewarray"
                        || instr->get_name() == "checkcast")
                {
                    if(instr->get_args().size() != 1)
                    {
                        throw emitter_error(
                          std::format(
                            "Expected 1 argument for '{}', got {}.",
                            instr->get_name(),
                            instr->get_args().size()));
                    }

                    const auto* arg = static_cast<const cg::type_argument*>(instr->get_args()[0].get());    // NOLINT(cppcoreguidelines-pro-type-static-cast-downcast)
                    auto lowered_type = arg->get_lowered_type();
                    if(!lowered_type.get_type_id().has_value())
                    {
                        throw emitter_error(
                          std::format(
                            "Lowered type '{}' carries no type id.",
                            ::cg::to_string(lowered_type.get_type_kind())));
                    }

                    auto info = type_ctx.get_type_info(lowered_type.get_type_id().value());
                    if(info.kind == ty::type_kind::struct_)
                    {
                        const auto& struct_info = std::get<ty::struct_info>(info.data);
                        if(struct_info.qualified_name.has_value())
                        {
                            // FIXME Change this, similar to function imports.
                            imports.intern_struct(struct_info.qualified_name.value());
                        }
                    }
                }
            }
        }
    }
}

/**
 * Get the import index of a type, or `std::nullopt` if it is not an import.
 *
 * @param type_ctx Type context.
 * @param imports Import table.
 * @param t Type to get the import index for.
 * @returns Returns an index into the context's import table, or `std::nullopt`.
 */
static std::optional<std::size_t> get_import_index_or_nullopt(
  const ty::context& type_ctx,
  const sema::env& sema_env,
  const import_table_builder& imports,
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

    const auto& struct_info = std::get<ty::struct_info>(info.data);
    if(!struct_info.qualified_name.has_value())
    {
        // not an import.
        return std::nullopt;
    }

    const auto it = std::ranges::find_if(
      sema_env.symbol_table,
      [&struct_info](const std::pair<sema::symbol_id, sema::symbol_info>& s) -> bool
      {
          return s.second.type == sema::symbol_type::type
                 && s.second.qualified_name == struct_info.qualified_name;
      });
    if(it == sema_env.symbol_table.cend())
    {
        throw emitter_error(
          std::format(
            "Struct '{}' not found in symbol table.",
            struct_info.qualified_name.value()));
    }

    if(it->second.declaring_module == sema::symbol_info::current_module_id)
    {
        return std::nullopt;
    }

    // get index into import table.
    auto package_symbol_info = sema_env.symbol_table.at(
      it->second.declaring_module);

    auto package_index = imports.get_package(package_symbol_info.name);

    // strip package name from import.
    std::string import_name = name::unqualified_name(struct_info.name);
    return imports.get_struct(package_index, import_name);
}

/** Argument count for a given instruction. */    // FIXME ?
static const std::unordered_map<
  std::string,
  std::size_t>
  instruction_info = {
    {"add", 1},
    {"sub", 1},
    {"mul", 1},
    {"div", 1},
    {"mod", 1},
    {"const_null", 0},
    {"const", 1},
    {"load", 1},
    {"store", 1},
    {"load_element", 1},
    {"store_element", 1},
    {"dup", 1},
    {"dup_x1", 2},
    {"dup_x2", 3},
    {"pop", 1},
    {"cast", 1},
    {"invoke", 1},
    {"ret", 1},
    {"set_field", 1},
    {"get_field", 1},
    {"and", 1},
    {"land", 1},
    {"or", 1},
    {"lor", 1},
    {"xor", 1},
    {"shl", 1},
    {"shr", 1},
    {"cmpl", 1},
    {"cmple", 1},
    {"cmpg", 1},
    {"cmpge", 1},
    {"cmpeq", 1},
    {"cmpne", 1},
    {"jnz", 2},
    {"jmp", 1},
    {"new", 1},
    {"anewarray", 1},
    {"checkcast", 1},
    {"newarray", 1},
    {"arraylength", 0}};

void instruction_emitter::emit_instruction(
  const std::unique_ptr<cg::function>& func,
  const std::unique_ptr<cg::instruction>& instr)
{
    const auto& name = instr->get_name();
    const auto& args = instr->get_args();

    /** Validate instruction and argument count. */
    auto it = instruction_info.find(name);
    if(it == instruction_info.end())
    {
        throw emitter_error(
          std::format(
            "Unknown instruction '{}'.",
            name));
    }

    if(it->second != args.size())
    {
        throw emitter_error(
          std::format(
            "Expected {} argument(s) for '{}', got {}.",
            it->second,
            name,
            args.size()));
    }

    auto emit_typed =
      [this, &name, &args](
        opcode i32_opcode,
        std::optional<opcode> f32_opcode = std::nullopt,
        std::optional<opcode> str_opcode = std::nullopt,
        std::optional<opcode> ref_opcode = std::nullopt)
    {
        const auto* arg = static_cast<const cg::type_argument*>(args[0].get());    // NOLINT(cppcoreguidelines-pro-type-static-cast-downcast)
        const auto type_kind = arg->get_lowered_type().get_type_kind();

        if(type_kind == cg::type_kind::i32)
        {
            emit(instruction_buffer, i32_opcode);
        }
        else if(type_kind == cg::type_kind::f32)
        {
            if(!f32_opcode.has_value())
            {
                throw std::runtime_error(std::format("Invalid type 'f32' for instruction '{}'.", name));
            }

            emit(instruction_buffer, *f32_opcode);
        }
        else if(type_kind == cg::type_kind::str)
        {
            if(!str_opcode.has_value())
            {
                throw std::runtime_error(std::format("Invalid type 'str' for instruction '{}'.", name));
            }

            emit(instruction_buffer, *str_opcode);
        }
        else if(type_kind == cg::type_kind::ref
                || type_kind == cg::type_kind::null)
        {
            if(!ref_opcode.has_value())
            {
                throw std::runtime_error(
                  std::format(
                    "Invalid reference type for instruction '{}'.",
                    name));
            }

            emit(instruction_buffer, *ref_opcode);
        }
        else
        {
            throw std::runtime_error(
              std::format(
                "Invalid type '{}' for instruction '{}'.",
                cg::to_string(arg->get_lowered_type().get_type_kind()),
                name));
        }
    };

    auto emit_load_store =
      [this, &name, &args, &func](
        opcode i8_opcode,
        opcode i16_opcode,
        opcode i32_opcode,
        opcode i64_opcode,
        opcode f32_opcode,
        opcode f64_opcode,
        opcode ref_opcode)
    {
        const auto* arg = static_cast<const cg::variable_argument*>(args[0].get());    // NOLINT(cppcoreguidelines-pro-type-static-cast-downcast)
        const cg::value* v = arg->get_value();

        if(!v->has_symbol_id())
        {
            throw std::runtime_error(
              std::format(
                "Cannot emit instruction '{}': Argument value has no symbol id.",
                name));
        }
        vle_int index{
          utils::numeric_cast<std::int64_t>(
            func->get_index(v->get_symbol_id().value()))};

        // check array types first.
        if(type_ctx.is_array(v->get_type().get_type_id().value()))
        {
            emit(instruction_buffer, ref_opcode);
        }
        else if(v->get_type().get_type_kind() == cg::type_kind::i8)
        {
            emit(instruction_buffer, i8_opcode);
        }
        else if(v->get_type().get_type_kind() == cg::type_kind::i16)
        {
            emit(instruction_buffer, i16_opcode);
        }
        else if(v->get_type().get_type_kind() == cg::type_kind::i32)
        {
            emit(instruction_buffer, i32_opcode);
        }
        else if(v->get_type().get_type_kind() == cg::type_kind::i64)
        {
            emit(instruction_buffer, i64_opcode);
        }
        else if(v->get_type().get_type_kind() == cg::type_kind::f32)
        {
            emit(instruction_buffer, f32_opcode);
        }
        else if(v->get_type().get_type_kind() == cg::type_kind::f64)
        {
            emit(instruction_buffer, f64_opcode);
        }
        else if(v->get_type().get_type_kind() == cg::type_kind::str)
        {
            emit(instruction_buffer, ref_opcode);
        }
        else if(type_ctx.is_reference(v->get_type().get_type_id().value()))
        {
            emit(instruction_buffer, ref_opcode);
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
        const auto* arg = static_cast<const cg::const_argument*>(args[0].get());    // NOLINT(cppcoreguidelines-pro-type-static-cast-downcast)
        const auto type_kind = arg->get_value()->get_type().get_type_kind();

        if(type_kind == cg::type_kind::i32)
        {
            emit(
              instruction_buffer,
              opcode::iconst,
              static_cast<std::int32_t>(
                static_cast<const cg::constant_i32*>(    // NOLINT(cppcoreguidelines-pro-type-static-cast-downcast)
                  arg->get_value())
                  ->get_int()));
        }
        else if(type_kind == cg::type_kind::f32)
        {
            emit(
              instruction_buffer,
              opcode::fconst,
              static_cast<const cg::constant_f32*>(    // NOLINT(cppcoreguidelines-pro-type-static-cast-downcast)
                arg->get_value())
                ->get_float());
        }
        else if(type_kind == cg::type_kind::str)
        {
            const_::constant_id id = static_cast<const cg::constant_str*>(    // NOLINT(cppcoreguidelines-pro-type-static-cast-downcast)
                                       arg->get_value())
                                       ->get_constant_id();
            auto it = constant_map.find(id);
            if(it == constant_map.end())
            {
                throw emitter_error(
                  std::format(
                    "Constant with id '{}' not in constant map.",
                    id));
            }

            emit(
              instruction_buffer,
              opcode::sconst,
              vle_int{static_cast<std::int64_t>(it->second)});    // NOLINT(cppcoreguidelines-pro-type-static-cast-downcast)
        }
        else
        {
            throw std::runtime_error(
              std::format(
                "Invalid type '{}' for instruction '{}'.",
                ::cg::to_string(type_kind),
                name));
        }
    }
    else if(name == "load")
    {
        emit_load_store(
          opcode::cload,
          opcode::sload,
          opcode::iload,
          opcode::lload,
          opcode::fload,
          opcode::dload,
          opcode::aload);
    }
    else if(name == "store")
    {
        emit_load_store(
          opcode::cstore,
          opcode::sstore,
          opcode::istore,
          opcode::lstore,
          opcode::fstore,
          opcode::dstore,
          opcode::astore);
    }
    else if(name == "load_element")
    {
        emit_typed(opcode::iaload, opcode::faload, opcode::aaload, opcode::aaload);
    }
    else if(name == "store_element")
    {
        emit_typed(opcode::iastore, opcode::fastore, opcode::aastore, opcode::aastore);
    }
    else if(name == "dup")
    {
        const auto* arg = static_cast<const cg::type_class_argument*>(args[0].get());    // NOLINT(cppcoreguidelines-pro-type-static-cast-downcast)
        const auto v = arg->get_class();

        if(v == type_class::cat1)
        {
            emit(instruction_buffer, opcode::dup);
        }
        else if(v == type_class::cat2)
        {
            emit(instruction_buffer, opcode::dup2);
        }
        else if(v == type_class::ref)
        {
            emit(instruction_buffer, opcode::adup);
        }
        else
        {
            throw std::runtime_error(
              std::format(
                "Invalid stack value type '{}' for instruction '{}'.",
                ::slang::to_string(v),
                name));
        }
    }
    else if(name == "dup_x1")
    {
        // get the duplicated value.
        const auto* v_arg = static_cast<const cg::type_class_argument*>(args[0].get());    // NOLINT(cppcoreguidelines-pro-type-static-cast-downcast)
        auto v = v_arg->get_class();

        // get the stack argument.
        const auto* stack_arg = static_cast<const cg::type_class_argument*>(args[1].get());    // NOLINT(cppcoreguidelines-pro-type-static-cast-downcast)
        auto s = stack_arg->get_class();

        // emit instruction.
        emit(instruction_buffer, opcode::dup_x1);
        instruction_buffer & v & s;
    }
    else if(name == "dup_x2")
    {
        // get the duplicated value.
        const auto* v_arg = static_cast<const cg::type_class_argument*>(args[0].get());    // NOLINT(cppcoreguidelines-pro-type-static-cast-downcast)
        auto v = v_arg->get_class();

        // get the stack argument.
        std::array<const cg::type_class_argument*, 2> stack_args = {
          static_cast<const cg::type_class_argument*>(args[1].get()),    // NOLINT(cppcoreguidelines-pro-type-static-cast-downcast)
          static_cast<const cg::type_class_argument*>(args[2].get())     // NOLINT(cppcoreguidelines-pro-type-static-cast-downcast)
        };
        std::array<type_class, 2> s = {
          stack_args[0]->get_class(),
          stack_args[1]->get_class()};

        // emit instruction.
        emit(instruction_buffer, opcode::dup_x2);
        instruction_buffer & v & s[0] & s[1];
    }
    else if(name == "pop")
    {
        emit_typed(opcode::pop, opcode::pop, opcode::apop, opcode::apop);    // same instruction for i32 and f32.
    }
    else if(name == "cast")
    {
        const auto* arg = static_cast<const cg::cast_argument*>(args[0].get());    // NOLINT(cppcoreguidelines-pro-type-static-cast-downcast)
        switch(arg->get_cast())
        {
        case cg::type_cast::i32_to_i8:
            emit(instruction_buffer, opcode::i2c);
            break;
        case cg::type_cast::i32_to_i16:
            emit(instruction_buffer, opcode::i2s);
            break;
        case cg::type_cast::i32_to_i64:
            emit(instruction_buffer, opcode::i2l);
            break;
        case cg::type_cast::i32_to_f32:
            emit(instruction_buffer, opcode::i2f);
            break;
        case cg::type_cast::i32_to_f64:
            emit(instruction_buffer, opcode::i2d);
            break;
        case cg::type_cast::i64_to_i32:
            emit(instruction_buffer, opcode::l2i);
            break;
        case cg::type_cast::i64_to_f32:
            emit(instruction_buffer, opcode::l2f);
            break;
        case cg::type_cast::i64_to_f64:
            emit(instruction_buffer, opcode::l2d);
            break;
        case cg::type_cast::f32_to_i32:
            emit(instruction_buffer, opcode::f2i);
            break;
        case cg::type_cast::f32_to_i64:
            emit(instruction_buffer, opcode::f2l);
            break;
        case cg::type_cast::f32_to_f64:
            emit(instruction_buffer, opcode::f2d);
            break;
        case cg::type_cast::f64_to_i32:
            emit(instruction_buffer, opcode::d2i);
            break;
        case cg::type_cast::f64_to_i64:
            emit(instruction_buffer, opcode::d2l);
            break;
        case cg::type_cast::f64_to_f32:
            emit(instruction_buffer, opcode::d2f);
            break;
        default:
            throw emitter_error(std::format("Invalid cast type '{}'.", ::cg::to_string(arg->get_cast())));
        }
    }
    else if(name == "invoke")
    {
        const auto* arg = static_cast<const cg::function_argument*>(args[0].get());    // NOLINT(cppcoreguidelines-pro-type-static-cast-downcast)
        auto symbol_id = arg->get_symbol_id();

        auto it = sema_env.symbol_table.find(symbol_id);
        if(it == sema_env.symbol_table.end())
        {
            throw emitter_error(
              std::format(
                "Could not find function with symbol id '{}' in symbol table.",
                symbol_id.value));
        }

        const auto& symbol_info = it->second;

        const auto info = get_import_info(symbol_id, sema_env);
        if(!info.has_value())
        {
            // resolve module-local functions.
            vle_int index{
              utils::numeric_cast<std::int64_t>(
                exports.get_index(
                  module_::symbol_type::function,
                  symbol_info.name))};
            emit(instruction_buffer, opcode::invoke);
            instruction_buffer & index;
            return;
        }

        std::string function_name = name::unqualified_name(symbol_info.name);
        std::size_t package_index = imports.get_package(info->qualified_module_name);
        std::size_t function_index = imports.get_function(package_index, function_name);

        vle_int index{-utils::numeric_cast<std::int64_t>(function_index) - 1};
        emit(instruction_buffer, opcode::invoke);
        instruction_buffer & index;
    }
    else if(name == "ret")
    {
        const auto* arg = static_cast<const cg::type_argument*>(args[0].get());    // NOLINT(cppcoreguidelines-pro-type-static-cast-downcast)

        if(arg->get_lowered_type().get_type_kind() == cg::type_kind::void_)
        {
            // return void from a function.
            emit(instruction_buffer, opcode::ret);
        }
        else
        {
            emit_typed(opcode::iret, opcode::fret, opcode::sret, opcode::aret);
        }
    }
    else if(name == "set_field"
            || name == "get_field")
    {
        const auto* arg = static_cast<const cg::field_access_argument*>(args[0].get());    // NOLINT(cppcoreguidelines-pro-type-static-cast-downcast)

        // resolve references to struct and field name.
        vle_int struct_index{0};
        vle_int field_index{utils::numeric_cast<std::int64_t>(arg->get_field_index())};

        auto struct_type_info = type_ctx.get_type_info(
          arg->get_struct_type_id());
        if(struct_type_info.kind != ty::type_kind::struct_)
        {
            throw emitter_error(
              std::format(
                "Expected struct type for type id '{}', got '{}'.",
                arg->get_struct_type_id(),
                ty::to_string(struct_type_info.kind)));
        }

        const auto& struct_info = std::get<ty::struct_info>(struct_type_info.data);
        if(struct_info.qualified_name.has_value())
        {
            // FIXME Use a type id, not a string.

            auto module_path = name::module_path(
              struct_info.qualified_name.value());
            std::size_t package_index = imports.get_package(
              std::string{module_path});

            // strip package name from import.
            std::string struct_import_name = name::unqualified_name(
              struct_info.name);

            struct_index = vle_int{-utils::numeric_cast<std::int64_t>(
                                     imports.get_struct(package_index, struct_import_name))
                                   - 1};
        }
        else
        {
            auto info = type_ctx.get_type_info(
              arg->get_struct_type_id());
            if(info.kind != ty::type_kind::struct_)
            {
                throw emitter_error(
                  std::format(
                    "Expected struct type for type id '{}', got '{}'.",
                    arg->get_struct_type_id(),
                    ty::to_string(info.kind)));
            }

            // find struct in export table.
            struct_index = vle_int{utils::numeric_cast<std::int64_t>(
              exports.get_index(
                module_::symbol_type::type,
                std::get<ty::struct_info>(info.data).name))};
        }

        if(name == "set_field")
        {
            emit(instruction_buffer, opcode::setfield);
        }
        else if(name == "get_field")
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
        emit_typed(opcode::icmpeq, opcode::fcmpeq, opcode::acmpeq, opcode::acmpeq);
    }
    else if(name == "cmpne")
    {
        emit_typed(opcode::icmpne, opcode::fcmpne, opcode::acmpne, opcode::acmpne);
    }
    else if(name == "jnz")
    {
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
        const auto* arg = static_cast<const cg::type_argument*>(args[0].get());    // NOLINT(cppcoreguidelines-pro-type-static-cast-downcast)
        const cg::type type = arg->get_lowered_type();                             // NOLINT(cppcoreguidelines-pro-type-static-cast-downcast)

        if(!type.get_type_id().has_value())
        {
            throw std::runtime_error(
              std::format(
                "Cannot emit instruction '{}': No type specified.",
                name));
        }

        // resolve type to index.
        vle_int struct_index{0};

        auto struct_type_info = type_ctx.get_type_info(
          type.get_type_id().value());
        if(struct_type_info.kind != ty::type_kind::struct_)
        {
            throw emitter_error(
              std::format(
                "Expected struct type for type id '{}', got '{}'.",
                type.get_type_id().value(),
                ty::to_string(struct_type_info.kind)));
        }

        const auto& struct_info = std::get<ty::struct_info>(struct_type_info.data);
        if(struct_info.qualified_name.has_value())
        {
            auto import_index = get_import_index_or_nullopt(
              type_ctx,
              sema_env,
              imports,
              type);

            if(!import_index.has_value())
            {
                throw emitter_error(
                  std::format(
                    "Cannot find type '{}' in import table.",
                    struct_info.qualified_name.value()));
            }
            struct_index = vle_int{
              -utils::numeric_cast<std::int64_t>(import_index.value()) - 1};
        }
        else
        {
            // find struct in export table.
            struct_index = vle_int{utils::numeric_cast<std::int64_t>(
              exports.get_index(
                module_::symbol_type::type,
                struct_info.name))};
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
        const auto& args = instr->get_args();
        const auto* arg = static_cast<cg::type_argument*>(args[0].get());    // NOLINT(cppcoreguidelines-pro-type-static-cast-downcast)
        auto type_kind = arg->get_lowered_type().get_type_kind();

        module_::array_type type = [&type_kind]() -> module_::array_type
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
              ::cg::to_string(type_kind)));
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

/**
 * Convert `const_::constant_type` to `module_::constant_type`.
 *
 * @param type The type to convert.
 * @returns Returns a `module_::constant_type`.
 * @throws Throws an `emitter_error` for unknown types.
 */
static module_::constant_type to_module_constant(const_::constant_type type)
{
    if(type == const_::constant_type::i32)
    {
        return module_::constant_type::i32;
    }

    if(type == const_::constant_type::f32)
    {
        return module_::constant_type::f32;
    }

    if(type == const_::constant_type::str)
    {
        return module_::constant_type::str;
    }

    throw emitter_error(
      std::format(
        "Cannot convert constant to '{}'.",
        std::to_underlying(type)));
}

/**
 * Convert a constant value.
 *
 * @param data The constant value.
 * @param type The type to pick.
 * @returns Returns the value.
 * @throws Throws an `emitter_error` for unknown types.
 */
static module_::constant_data_type
  to_module_const_value(
    std::variant<
      std::monostate,
      int,
      float,
      std::string>
      data,
    const_::constant_type type)
{
    if(type == const_::constant_type::i32)
    {
        return std::get<int>(data);
    }

    if(type == const_::constant_type::f32)
    {
        return std::get<float>(data);
    }

    if(type == const_::constant_type::str)
    {
        return std::get<std::string>(data);
    }

    throw emitter_error(
      std::format(
        "Cannot convert constant to '{}'.",
        std::to_underlying(type)));
}

void instruction_emitter::run()
{
    // clear buffers and tables.
    instruction_buffer.clear();
    exports.clear();
    imports.clear();
    constant_table.clear();
    constant_map.clear();

    // collect jump targets.
    jump_targets = collect_jump_targets();

    // collect imports.
    collect_imports();

    // the import count is not allowed to change, so store it here and check later.
    std::size_t import_count = imports.size();

    // exported constants.
    constant_table.reserve(const_env.const_literal_map.size());

    for(const auto& [id, info]: const_env.const_literal_map)
    {
        constant_map[id] = constant_table.size();
        constant_table.emplace_back(
          to_module_constant(info.type),
          to_module_const_value(info.value, info.type));
    }

    for(const auto& [symbol_id, info]: const_env.const_info_map)
    {
        // only export constants declared in the current module.
        if(info.origin_module_id != sema::symbol_info::current_module_id)
        {
            continue;
        }

        auto symbol_info = sema_env.symbol_table.at(symbol_id);

        exports.add_constant(
          symbol_info.name,
          constant_table.size());
        constant_table.emplace_back(
          to_module_constant(info.type),
          to_module_const_value(info.value, info.type));
    }

    // TODO Walk the exported types earlier and add imports to import table.
    //      (after collect_imports) ?

    // exported types.
    for(const auto& [id, symbol_info]: sema_env.symbol_table)
    {
        if(symbol_info.type != sema::symbol_type::type)
        {
            continue;
        }

        if(symbol_info.declaring_module != sema::symbol_info::current_module_id)
        {
            continue;
        }

        const auto& type_info = type_ctx.get_type_info(
          type_ctx.get_type(symbol_info.name));

        const auto& struct_info = std::get<ty::struct_info>(type_info.data);
        exports.add_struct(*this, struct_info);
    }

    // exported functions.
    for(const auto& f: codegen_ctx.funcs)
    {
        auto [signature_ret_type, signature_arg_types] = f->get_signature();

        if(!signature_ret_type.get_type_id().has_value())
        {
            throw emitter_error(
              std::format(
                "{}: No return type.",
                f->get_name()));
        }

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
            sema_env,
            imports,
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
                        sema_env,
                        imports, 
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
            continue;
        }

        exports.add_macro(m->get_name(), m->get_desc());
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
        const auto& func_args = f->get_args();
        const auto& func_locals = f->get_locals();
        const std::size_t local_count = func_args.size() + func_locals.size();

        std::vector<module_::variable_descriptor> locals{local_count};

        std::set<std::size_t> unset_indices;
        for(std::size_t i = 0; i < local_count; ++i)
        {
            unset_indices.insert(unset_indices.end(), i);
        }

        for(const auto& [sym_id, ty]: func_args)
        {
            std::size_t index = f->get_index(sym_id);
            if(!unset_indices.contains(index))
            {
                auto info_it = sema_env.symbol_table.find(sym_id);
                if(info_it != sema_env.symbol_table.end())
                {
                    throw emitter_error(
                      std::format(
                        "{}: Tried to map local '{}' with index '{}' multiple times.",
                        f->get_name(),
                        info_it->second.name,
                        index));
                }

                throw emitter_error(
                  std::format(
                    "{}: Tried to map local with index '{}' multiple times.",
                    f->get_name(),
                    index));
            }
            unset_indices.erase(index);

            if(!ty.get_type_id().has_value())
            {
                throw emitter_error(
                  std::format(
                    "{}: Argument {} has no symbol id.",
                    f->get_name(),
                    index));
            }

            auto type_id = ty.get_type_id().value();
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
                  sema_env,
                  imports,
                  ty)}};
        }

        for(const auto& [sym_id, ty]: func_locals)
        {
            std::size_t index = f->get_index(sym_id);
            if(!unset_indices.contains(index))
            {
                auto info_it = sema_env.symbol_table.find(sym_id);
                if(info_it != sema_env.symbol_table.end())
                {
                    throw emitter_error(
                      std::format(
                        "{}: Tried to map local '{}' with index '{}' multiple times.",
                        f->get_name(),
                        info_it->second.name,
                        index));
                }

                throw emitter_error(
                  std::format(
                    "{}: Tried to map local with index '{}' multiple times.",
                    f->get_name(),
                    index));
            }
            unset_indices.erase(index);

            if(!ty.get_type_id().has_value())
            {
                throw emitter_error(
                  std::format(
                    "{}: Local {} has no symbol id.",
                    f->get_name(),
                    index));
            }

            auto type_id = ty.get_type_id().value();
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
                  sema_env,
                  imports,
                  ty)}};
        }

        if(!unset_indices.empty())
        {
            throw emitter_error(
              std::format(
                "Inconsistent local count for function '{}'.",
                f->get_name()));
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
    if(import_count != imports.size())
    {
        throw emitter_error(
          std::format(
            "Import count changed during instruction emission ({} -> {}).",
            import_count, imports.size()));
    }
    if(export_count != exports.size())
    {
        throw emitter_error(
          std::format(
            "Export count changed during instruction emission ({} -> {}).",
            export_count, exports.size()));
    }
}

module_::language_module instruction_emitter::to_module() const
{
    module_::language_module mod;

    /*
     * Imports.
     */
    imports.write(mod);

    /*
     * Constants.
     */
    mod.set_constant_table(constant_table);

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
