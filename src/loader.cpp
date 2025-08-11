/**
 * slang - a simple scripting language.
 *
 * name resolution.
 *
 * \author Felix Lubbe
 * \copyright Copyright (c) 2025
 * \license Distributed under the MIT software license (see accompanying LICENSE.txt).
 */

#include <format>
#include <ranges>

#include "compiler/ast/ast.h"
#include "compiler/ast/node_registry.h"
#include "compiler/codegen.h"
#include "compiler/typing.h"
#include "shared/type_utils.h"
#include "package.h"
#include "loader.h"
#include "utils.h"

namespace cg = slang::codegen;
namespace ty = slang::typing;
namespace si = slang::interpreter;

namespace slang::loader
{

/*
 * Helpers.
 */

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
              std::format(
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

/*
 * Exceptions.
 */

resolve_error::resolve_error(const source_location& loc, const std::string& message)
: std::runtime_error{std::format("{}: {}", to_string(loc), message)}
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
        throw resolve_error(std::format("Module '{}' is already resolved.", import_name));
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

    return slang::loader::resolve_module(file_mgr, resolvers, import_name, transitive);
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

        // TODO type context
        throw std::runtime_error("add_constant");
    }
    else if(entry.type == module_::constant_type::f32)
    {
        ctx.add_constant(name, std::get<float>(entry.data), import_path);

        // TODO type context
        throw std::runtime_error("add_constant");
    }
    else if(entry.type == module_::constant_type::str)
    {
        ctx.add_constant(name, std::get<std::string>(entry.data), import_path);

        // TODO type context
        throw std::runtime_error("add_constant");
    }
    else
    {
        throw resolve_error(
          std::format(
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
    std::vector<cg::value> prototype_arg_types =
      desc.signature.arg_types
      | std::views::transform([&resolver, &import_path](const module_::variable_type& arg)
                              { return to_value(arg, resolver, import_path); })
      | std::ranges::to<std::vector>();

    ctx.add_prototype(
      name,
      to_value(
        desc.signature.return_type,
        resolver,
        import_path),
      prototype_arg_types,
      import_path);

    // TODO type context
    throw std::runtime_error("add_function");
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
        std::vector<std::pair<std::string, cg::value>> members =
          desc.member_types
          | std::views::transform(
            [&import_path, &resolver](
              const std::pair<std::string, module_::field_descriptor>& member) -> std::pair<std::string, cg::value>
            { return {
                std::get<0>(member),
                to_value(
                  std::get<1>(member).base_type,
                  resolver,
                  import_path,
                  std::get<0>(member))}; })
          | std::ranges::to<std::vector>();

        ctx.add_import(module_::symbol_type::type, import_path, name);
        ctx.add_struct(name, members, desc.flags, import_path);
        ctx.get_global_scope()->add_struct(name, std::move(members), desc.flags, import_path);
    }

    // Add type to typing context.
    // TODO
    throw std::runtime_error("add_type");
}

void context::resolve_imports(cg::context& ctx, ty::context& type_ctx)
{
    for(const auto& [name, resolver]: resolvers)
    {
        std::println("resolve_imports: {}", name);

        const module_::module_header& header = resolver->get_module().get_header();
        for(const module_::exported_symbol& exp: header.exports)
        {
            std::println("resolve_imports: export {} ({})", exp.name, module_::to_string(exp.type));

            if(exp.type == module_::symbol_type::constant)
            {
                // TODO
            }
            else if(exp.type == module_::symbol_type::function)
            {
                // TODO
            }
            else if(exp.type == module_::symbol_type::type)
            {
                // TODO
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
                  // TODO type context
                  throw std::runtime_error("resolve_macros");
              }

              // TODO Function calls need to be resolved.
          },
          true,
          false);
    }

    return needs_import_resolution;
}

}    // namespace slang::loader
