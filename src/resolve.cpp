/**
 * slang - a simple scripting language.
 *
 * name resolution.
 *
 * \author Felix Lubbe
 * \copyright Copyright (c) 2024
 * \license Distributed under the MIT software license (see accompanying LICENSE.txt).
 */

#include <fmt/core.h>

#include "codegen.h"
#include "package.h"
#include "resolve.h"
#include "typing.h"
#include "utils.h"

namespace cg = slang::codegen;
namespace ty = slang::typing;

namespace slang::resolve
{

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

module_header& context::get_module_header(const fs::path& resolved_path)
{
    std::string path_str = resolved_path.string();
    auto it = headers.find(path_str);
    if(it != headers.end())
    {
        return it->second;
    }

    std::unique_ptr<slang::file_archive> ar = mgr.open(resolved_path, slang::file_manager::open_mode::read);

    module_header hdr;
    (*ar) & hdr;

    auto insert_it = headers.insert({path_str, std::move(hdr)});
    return insert_it.first->second;
}

/**
 * Convert an import name of the for `a::b::c` into a path `a/b/c`.
 *
 * @param import_name The import name.
 * @returns Returns a path for the import name.
 */
static fs::path import_name_to_path(std::string import_name)
{
    slang::utils::replace_all(import_name, package::delimiter, "/");
    return fs::path{import_name}.replace_extension(package::module_ext);
}

void context::resolve_module(const fs::path& resolved_path)
{
    auto mod_it = std::find_if(modules.begin(), modules.end(),
                               [&resolved_path](const module_entry& entry) -> bool
                               { return entry.resolved_path == resolved_path.string(); });
    if(mod_it != modules.end())
    {
        if(!mod_it->is_resolved)
        {
            throw resolve_error(fmt::format("Circular import for module '{}'.", resolved_path.string()));
        }

        return;
    }

    modules.push_back({resolved_path.string(), false});

    module_header& header = get_module_header(resolved_path);

    for(auto& it: header.imports)
    {
        if(it.type == symbol_type::package)
        {
            // packages are loaded while resolving other symbols.
            continue;
        }

        // resolve the symbol's package.
        if(it.package_index >= header.imports.size())
        {
            throw resolve_error(fmt::format("Error while resolving imports: Import symbol '{}' has invalid package index.", it.name));
        }
        else if(header.imports[it.package_index].type != symbol_type::package)
        {
            throw resolve_error(fmt::format("Error while resolving imports: Import symbol '{}' refers to non-package import entry.", it.name));
        }

        fs::path resolved_import_path = mgr.resolve(import_name_to_path(header.imports[it.package_index].name));
        resolve_module(resolved_import_path);

        // find the imported symbol.
        module_header& import_header = get_module_header(resolved_import_path);

        auto exp_it = std::find_if(import_header.exports.begin(), import_header.exports.end(),
                                   [&it](const exported_symbol& exp) -> bool
                                   {
                                       return exp.name == it.name;
                                   });
        if(exp_it == import_header.exports.end())
        {
            throw resolve_error(
              fmt::format("Error while resolving imports: Symbol '{}' is not exported by module '{}'.",
                          it.name, header.imports[it.package_index].name));
        }
        if(exp_it->type != it.type)
        {
            throw resolve_error(
              fmt::format(
                "Error while resolving imports: Symbol '{}' from module '{}' has wrong type (expected '{}', got '{}').",
                it.name, header.imports[it.package_index].name, slang::to_string(it.type), slang::to_string(exp_it->type)));
        }
    }

    for(auto& it: header.exports)
    {
        // resolve the symbol.
        if(it.type == symbol_type::function)
        {
            if(imported_functions.find(resolved_path.string()) == imported_functions.end())
            {
                imported_functions.insert({resolved_path.string(), {}});
            }
            imported_functions[resolved_path.string()].push_back({it.name, std::get<function_descriptor>(it.desc)});
        }
        else if(it.type == symbol_type::type)
        {
            if(imported_types.find(resolved_path.string()) == imported_types.end())
            {
                imported_types.insert({resolved_path.string(), {}});
            }
            imported_types[resolved_path.string()].push_back({it.name, std::get<type_descriptor>(it.desc)});
        }
        else
        {
            throw resolve_error(fmt::format("Cannot resolve symbol '{}' of type '{}'.", it.name, to_string(it.type)));
        }
    }

    mod_it = std::find_if(modules.begin(), modules.end(),
                          [&resolved_path](const module_entry& entry) -> bool
                          { return entry.resolved_path == resolved_path.string(); });
    if(mod_it == modules.end())
    {
        throw resolve_error(fmt::format("Internal error: Resolved module '{}' not in modules list.", resolved_path.string()));
    }
    mod_it->is_resolved = true;
}

void context::resolve_imports(cg::context& ctx, ty::context& type_ctx)
{
    const std::vector<std::vector<token>>& imports = type_ctx.get_imports();

    auto transform = [](const token& p) -> std::string
    { return p.s; };

    for(auto& import: imports)
    {
        if(import.size() == 0)
        {
            throw resolve_error("Cannot resolve empty import.");
        }

        auto reference_location = import[0].location;

        std::string import_path = utils::join(import, {transform}, package::delimiter);

        fs::path fs_path = fs::path{utils::join(import, {transform}, "/")}.replace_extension(package::module_ext);
        fs::path resolved_path = mgr.resolve(fs_path);

        resolve_module(resolved_path);

        auto imported_functions_it = imported_functions.find(resolved_path.string());
        if(imported_functions_it != imported_functions.end())
        {
            for(auto& it: imported_functions_it->second)
            {
                auto& desc = it.second;

                std::vector<cg::value> prototype_arg_types;
                std::transform(desc.signature.arg_types.cbegin(), desc.signature.arg_types.cend(), std::back_inserter(prototype_arg_types),
                               [](const auto& arg)
                               {
                                   if(ty::is_builtin_type(std::get<0>(arg)))
                                   {
                                       return cg::value{std::get<0>(arg), std::nullopt, std::nullopt, std::get<1>(arg)};
                                   }

                                   return cg::value{"aggregate", std::get<0>(arg), std::nullopt, std::get<1>(arg)};
                               });

                cg::value return_type;
                if(ty::is_builtin_type(std::get<0>(desc.signature.return_type)))
                {
                    return_type = {std::get<0>(desc.signature.return_type),
                                   std::nullopt,
                                   std::nullopt,
                                   desc.signature.return_type.second};
                }
                else
                {
                    return_type = {"aggregate",
                                   std::get<0>(desc.signature.return_type),
                                   std::nullopt,
                                   desc.signature.return_type.second};
                }

                ctx.add_prototype(it.first, return_type, prototype_arg_types, import_path);

                std::vector<ty::type> arg_types;
                for(auto& arg: desc.signature.arg_types)
                {
                    arg_types.emplace_back(type_ctx.get_type(std::get<0>(arg), std::get<1>(arg)));
                }

                ty::type resolved_return_type = type_ctx.get_type(return_type.get_resolved_type(),
                                                                  return_type.is_array());

                type_ctx.add_function({it.first, reference_location},
                                      std::move(arg_types), std::move(resolved_return_type), import_path);
            }
        }

        auto imported_types_it = imported_types.find(resolved_path.string());
        if(imported_types_it != imported_types.end())
        {
            for(auto& it: imported_types_it->second)
            {
                auto& desc = it.second;

                // Add type to code generation context.
                {
                    std::vector<std::pair<std::string, cg::value>> members;
                    std::transform(desc.member_types.cbegin(), desc.member_types.cend(), std::back_inserter(members),
                                   [](const std::pair<std::string, type_info>& member) -> std::pair<std::string, cg::value>
                                   {
                                       if(ty::is_builtin_type(std::get<1>(member).base_type))
                                       {
                                           return {std::get<0>(member), cg::value{std::get<1>(member).base_type,
                                                                                  std::nullopt,
                                                                                  std::nullopt,
                                                                                  std::get<1>(member).array}};
                                       }

                                       return {std::get<0>(member), cg::value{
                                                                      "aggregate",
                                                                      std::get<1>(member).base_type,
                                                                      std::nullopt,
                                                                      std::get<1>(member).array}};
                                   });

                    ctx.add_type(it.first, std::move(members), import_path);
                }

                // Add type to typing context.
                {
                    std::vector<std::pair<token, ty::type>> members;
                    for(auto& [member_name, member_type]: desc.member_types)
                    {
                        ty::type resolved_member_type = type_ctx.get_unresolved_type(
                          {member_type.base_type, reference_location},
                          ty::is_builtin_type(member_type.base_type)
                            ? typing::type_class::tc_plain
                            : typing::type_class::tc_struct);

                        members.push_back(std::make_pair<token, ty::type>(
                          {member_name, reference_location},
                          std::move(resolved_member_type)));
                    }

                    type_ctx.add_struct({it.first, reference_location}, std::move(members), import_path);
                }
            }
        }
    }
}

}    // namespace slang::resolve