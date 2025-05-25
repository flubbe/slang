/**
 * slang - a simple scripting language.
 *
 * name resolution.
 *
 * \author Felix Lubbe
 * \copyright Copyright (c) 2025
 * \license Distributed under the MIT software license (see accompanying LICENSE.txt).
 */

#include <fmt/core.h>

#include "compiler/ast/ast.h"
#include "compiler/codegen.h"
#include "compiler/typing.h"
#include "shared/type_utils.h"
#include "package.h"
#include "resolve.h"
#include "utils.h"

namespace cg = slang::codegen;
namespace ty = slang::typing;
namespace si = slang::interpreter;

namespace slang::resolve
{

/*
 * Helpers.
 */

/**
 * Return the type class of a `variable_type`.
 *
 * @param vt A `variable_type`.
 * @return A `type_class`.
 */
static ty::type_class to_type_class(const module_::variable_type& vt)
{
    if(vt.is_array())
    {
        return ty::type_class::tc_array;
    }

    if(ty::is_builtin_type(vt.base_type()))
    {
        return ty::type_class::tc_plain;
    }

    return ty::type_class::tc_struct;
}

/**
 * Convert a field given by a field descriptor to a `cg::value`.
 *
 * @param desc The field's variable type.
 * @param resolver The module resolver.
 * @param import_path The module's import path.
 * @param name The field's name.
 * @return A `cg::value` with the field information.
 */
static cg::value to_value(
  const module_::variable_type& vt,
  const module_::module_resolver& resolver,
  const std::string& import_path,
  const std::optional<std::string>& name = std::nullopt)
{
    // Built-in types.
    if(ty::is_builtin_type(vt.base_type()))
    {
        return {cg::type{
                  cg::to_type_class(vt.base_type()),
                  vt.is_array()
                    ? static_cast<std::size_t>(1)
                    : static_cast<std::size_t>(0)},
                name};
    }

    // Custom types.
    std::string type_import_package = import_path;
    if(vt.get_import_index().has_value())
    {
        // This is an imported type.
        // NOLINTNEXTLINE(bugprone-unchecked-optional-access)
        const module_::imported_symbol& sym = resolver.get_module().get_header().imports.at(vt.get_import_index().value());
        if(sym.type != module_::symbol_type::package)
        {
            throw resolve_error(
              fmt::format(
                "Cannot resolve imported type: Import table entry '{}' ('{}') is not a package.",
                vt.get_import_index().value(),
                sym.name));
        }
        type_import_package = sym.name;
    }

    return cg::value{
      cg::type{
        cg::type_class::struct_,
        vt.is_array()
          ? static_cast<std::size_t>(1)
          : static_cast<std::size_t>(0),
        vt.base_type(),
        type_import_package},
      name};
}

/**
 * Convert a field given by a field descriptor to a `ty::type_info`.
 *
 * @param type_ctx The type context for type lookups.
 * @param desc The field descriptor.
 * @param resolver The module resolver.
 * @param import_path The module's import path.
 * @return A `ty::type_info` with the field type.
 */
static ty::type_info to_unresolved_type_info(
  ty::context& type_ctx,
  const module_::field_descriptor& desc,
  const module_::module_resolver& resolver,
  const std::string& import_path)
{
    std::string type_import_package = import_path;
    if(desc.base_type.get_import_index().has_value())
    {
        // This is an imported type.
        // NOLINTNEXTLINE(bugprone-unchecked-optional-access)
        const module_::imported_symbol& sym = resolver.get_module().get_header().imports.at(desc.base_type.get_import_index().value());
        if(sym.type != module_::symbol_type::package)
        {
            throw resolve_error(
              fmt::format(
                "Cannot resolve imported type: Import table entry '{}' ('{}') is not a package.",
                desc.base_type.get_import_index().value(),
                sym.name));
        }
        type_import_package = sym.name;
    }

    return type_ctx.get_unresolved_type(
      {desc.base_type.base_type(), {0, 0}},
      to_type_class(desc.base_type),
      type_import_package);
}

/**
 * Convert a `variable_type` to a `ty::type_info`.
 *
 * @param type_ctx The type context for type lookups.
 * @param vt The variable type.
 * @param resolver The module resolver.
 * @param import_path The module's import path.
 * @return A `ty::type_info` with the variable type.
 */
static ty::type_info to_resolved_type_info(
  ty::context& type_ctx,
  const module_::variable_type& vt,
  const module_::module_resolver& resolver,
  const std::string& import_path)
{
    if(ty::is_builtin_type(vt.base_type()))
    {
        return type_ctx.get_type(
          vt.base_type(),
          vt.is_array());
    }

    std::optional<std::size_t> import_index = vt.get_import_index();
    std::string type_import_package = import_path;
    if(import_index.has_value())
    {
        // This is an imported type.
        const module_::imported_symbol& sym = resolver.get_module().get_header().imports.at(import_index.value());
        if(sym.type != module_::symbol_type::package)
        {
            throw resolve_error(
              fmt::format(
                "Cannot resolve imported type: Import table entry '{}' ('{}') is not a package.",
                import_index.value(),
                sym.name));
        }
        type_import_package = sym.name;
    }

    return type_ctx.get_type(
      vt.base_type(),
      vt.is_array(),
      type_import_package);
}

/*
 * Exceptions.
 */

resolve_error::resolve_error(const token_location& loc, const std::string& message)
: std::runtime_error{fmt::format("{}: {}", to_string(loc), message)}
{
}

/*
 * resolver context.
 */

static module_::module_resolver& resolve_module(
  file_manager& file_mgr,
  std::unordered_map<
    std::string,
    std::unique_ptr<module_::module_resolver>>& resolvers,
  const std::string& import_name,
  bool transitive)
{
    auto it = resolvers.find(import_name);
    if(it != resolvers.end())
    {
        throw resolve_error(fmt::format("Module '{}' is already resolved.", import_name));
    }

    std::string import_path = import_name;
    slang::utils::replace_all(import_path, package::delimiter, "/");

    fs::path fs_path = fs::path{import_path};
    if(!fs_path.has_extension())
    {
        fs_path = fs_path.replace_extension(package::module_ext);
    }
    fs::path resolved_path = file_mgr.resolve(fs_path);

    resolvers.insert(
      {import_name,
       std::make_unique<module_::module_resolver>(
         file_mgr,
         resolved_path,
         transitive)});
    return *resolvers[import_name].get();
}

module_::module_resolver& context::resolve_module(
  const std::string& import_name,
  bool transitive)
{
    auto it = resolvers.find(import_name);
    if(it != resolvers.end())
    {
        // module is already resolved.
        if(it->second->is_transitive() && !transitive)
        {
            it->second->make_explicit();
        }

        return *it->second;
    }

    return slang::resolve::resolve_module(file_mgr, resolvers, import_name, transitive);
}

/**
 * Add a constant to the type- and code generation contexts.
 *
 * @param ctx Code generation context.
 * @param type_ctx Type context.
 * @param resolver Resolver for the module containing the constant.
 * @param import_path Import path of the module containing the constant.
 * @param name The constant's name.
 * @param index Index into the module's constant table.
 */
static void add_constant(
  cg::context& ctx,
  ty::context& type_ctx,
  const module_::module_resolver& resolver,
  const std::string& import_path,
  const std::string& name,
  std::size_t index)
{
    const module_::module_header& header = resolver.get_module().get_header();
    const module_::constant_table_entry& entry = header.constants.at(index);

    if(entry.type == module_::constant_type::i32)
    {
        ctx.add_constant(name, std::get<std::int32_t>(entry.data), import_path);
        type_ctx.add_variable(
          {name, {0, 0}},
          type_ctx.get_unresolved_type({"i32", {0, 0}}, ty::type_class::tc_plain),
          import_path);    // FIXME for the typing context, constants and variables are the same right now.
    }
    else if(entry.type == module_::constant_type::f32)
    {
        ctx.add_constant(name, std::get<float>(entry.data), import_path);
        type_ctx.add_variable(
          {name, {0, 0}},
          type_ctx.get_unresolved_type({"f32", {0, 0}}, ty::type_class::tc_plain),
          import_path);    // FIXME for the typing context, constants and variables are the same right now.
    }
    else if(entry.type == module_::constant_type::str)
    {
        ctx.add_constant(name, std::get<std::string>(entry.data), import_path);
        type_ctx.add_variable(
          {name, {0, 0}},
          type_ctx.get_unresolved_type({"str", {0, 0}}, ty::type_class::tc_plain),
          import_path);    // FIXME for the typing context, constants and variables are the same right now.
    }
    else
    {
        throw resolve_error(
          fmt::format(
            "Constant '{}' has unknown type id {}.",
            name,
            static_cast<int>(entry.type)));
    }
}

/**
 * Add a function to the type- and code generation contexts.
 *
 * @param ctx Code generation context.
 * @param type_ctx Type context.
 * @param resolver Resolver for the module containing the function.
 * @param import_path Import path of the module containing the function.
 * @param name The function's name.
 * @param desc The function desciptor.
 */
static void add_function(
  cg::context& ctx,
  ty::context& type_ctx,
  const module_::module_resolver& resolver,
  const std::string& import_path,
  const std::string& name,
  const module_::function_descriptor& desc)
{
    std::vector<cg::value> prototype_arg_types;
    std::transform(
      desc.signature.arg_types.cbegin(),
      desc.signature.arg_types.cend(),
      std::back_inserter(prototype_arg_types),
      [&resolver, &import_path](const module_::variable_type& arg) -> cg::value
      {
          return to_value(arg, resolver, import_path);
      });

    ctx.add_prototype(
      name,
      to_value(
        desc.signature.return_type,
        resolver,
        import_path),
      prototype_arg_types,
      import_path);

    std::vector<ty::type_info> arg_types;
    arg_types.reserve(desc.signature.arg_types.size());
    for(const auto& arg: desc.signature.arg_types)
    {
        arg_types.emplace_back(to_resolved_type_info(type_ctx, arg, resolver, import_path));
    }

    type_ctx.add_function(
      {name, {0, 0}},
      std::move(arg_types),
      to_resolved_type_info(type_ctx, desc.signature.return_type, resolver, import_path),
      import_path);
}

/**
 * Add a type to the type- and code generation contexts.
 *
 * @param ctx Code generation context.
 * @param type_ctx Type context.
 * @param resolver Resolver for the module containing the constant.
 * @param import_path Import path of the module containing the type.
 * @param name The type's name.
 * @param desc The type desciptor.
 */
static void add_type(
  cg::context& ctx,
  ty::context& type_ctx,
  const module_::module_resolver& resolver,
  const std::string& import_path,
  const std::string& name,
  const module_::struct_descriptor& desc)
{
    // Add type to code generation context.
    {
        std::vector<std::pair<std::string, cg::value>> members;
        std::transform(
          desc.member_types.cbegin(),
          desc.member_types.cend(),
          std::back_inserter(members),
          [&import_path, &resolver](
            const std::pair<std::string, module_::field_descriptor>& member) -> std::pair<std::string, cg::value>
          {
              return {
                std::get<0>(member),
                to_value(
                  std::get<1>(member).base_type,
                  resolver,
                  import_path,
                  std::get<0>(member))};
          });

        ctx.add_import(module_::symbol_type::type, import_path, name);
        ctx.add_struct(name, members, desc.flags, import_path);
        ctx.get_global_scope()->add_struct(name, std::move(members), desc.flags, import_path);
    }

    // Add type to typing context.
    {
        std::vector<std::pair<token, ty::type_info>> members;
        members.reserve(desc.member_types.size());
        for(const auto& [member_name, member_type]: desc.member_types)
        {
            members.push_back(std::make_pair<token, ty::type_info>(
              {member_name, {0, 0}},
              to_unresolved_type_info(type_ctx, member_type, resolver, import_path)));
        }

        type_ctx.add_struct({name, {0, 0}}, std::move(members), import_path);
    }
}

void context::resolve_imports(cg::context& ctx, ty::context& type_ctx)
{
    const std::vector<ty::imported_module>& imports = type_ctx.get_imported_modules();

    for(const auto& import: imports)
    {
        if(import.path.empty())
        {
            throw resolve_error("Cannot resolve empty import.");
        }

        auto it = resolvers.find(import.path);
        if(it != resolvers.end())
        {
            // module is already resolved.
            continue;
        }

        module_::module_resolver& resolver = resolve_module(import.path, import.transitive);
        const module_::module_header& header = resolver.get_module().get_header();

        for(const auto& it: header.exports)
        {
            const std::string import_name = make_import_name(it.name, import.transitive);

            if(it.type == module_::symbol_type::constant)
            {
                add_constant(ctx, type_ctx, resolver, import.path, import_name, std::get<std::size_t>(it.desc));
            }
            else if(it.type == module_::symbol_type::function)
            {
                add_function(ctx, type_ctx, resolver, import.path, import_name, std::get<module_::function_descriptor>(it.desc));
            }
            else if(it.type == module_::symbol_type::type)
            {
                add_type(ctx, type_ctx, resolver, import.path, import_name, std::get<module_::struct_descriptor>(it.desc));
            }
            else if(it.type == module_::symbol_type::macro)
            {
                ctx.add_macro(import_name, std::get<module_::macro_descriptor>(it.desc), import.path);
                type_ctx.add_macro(import_name, import.path);
            }
            else
            {
                throw resolve_error(fmt::format("Found unknown symbol type '{}'.", static_cast<int>(it.type)));
            }
        }
    }
}

bool context::resolve_macros(cg::context& ctx, ty::context& type_ctx)
{
    bool needs_import_resolution = false;

    for(auto& m: ctx.get_macros())
    {
        const auto& desc = m->get_desc();
        if(!desc.serialized_ast.has_value())
        {
            throw resolve_error(
              fmt::format(
                "Macro '{}' has empty AST.",
                m->get_name()));
        }

        memory_read_archive ar{
          desc.serialized_ast.value(),
          true,
          endian::little};

        std::unique_ptr<ast::expression> macro_ast;
        ar& ast::expression_serializer{macro_ast};

        // update namespace information.
        macro_ast->visit_nodes(
          [](ast::expression& e) -> void
          {
              if(e.get_id() != ast::node_identifier::namespace_access_expression)
              {
                  return;
              }

              if(!e.is_macro_invocation())
              {
                  return;
              }

              // this updates the namespace information.
              (void)e.as_macro_invocation();

              // TODO Function calls need to be resolved.
          },
          false,
          false);

        // Add import from macro AST to type context.
        macro_ast->visit_nodes(
          [&type_ctx, &needs_import_resolution](const ast::expression& e) -> void
          {
              if(e.is_macro_invocation()
                 && e.get_namespace_path().has_value())
              {
                  if(!type_ctx.has_import(e.get_namespace_path().value()))
                  {
                      type_ctx.add_import(e.get_namespace_path().value(), true);
                      needs_import_resolution = true;
                  }
              }

              // TODO Function calls need to be resolved.
          },
          true,
          false);
    }

    return needs_import_resolution;
}

}    // namespace slang::resolve
