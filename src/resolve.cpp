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
#include "module.h"
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
        std::unique_ptr<slang::file_archive> ar = mgr.open(resolved_path, slang::file_manager::open_mode::read);

        module_header hdr;
        (*ar) & hdr;

        for(auto& exp: hdr.exports)
        {
            if(exp.type == symbol_type::function)
            {
                auto& desc = std::get<function_descriptor>(exp.desc);

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
                                   desc.signature.return_type.second ? std::make_optional(std::get<1>(desc.signature.return_type))
                                                                     : std::nullopt};
                }
                else
                {
                    return_type = {"aggregate",
                                   std::get<0>(desc.signature.return_type),
                                   std::nullopt,
                                   desc.signature.return_type.second ? std::make_optional(std::get<1>(desc.signature.return_type))
                                                                     : std::nullopt};
                }

                ctx.add_prototype(exp.name, return_type, prototype_arg_types, import_path);

                std::vector<ty::type> arg_types;
                for(auto& arg: desc.signature.arg_types)
                {
                    arg_types.emplace_back(type_ctx.get_type(std::get<0>(arg), std::get<1>(arg)));
                }

                ty::type resolved_return_type = type_ctx.get_type(return_type.get_resolved_type(),
                                                                  return_type.is_array());

                type_ctx.add_function({exp.name, reference_location},
                                      std::move(arg_types), std::move(resolved_return_type), import_path);
            }
            else if(exp.type == symbol_type::type)
            {
                auto& desc = std::get<type_descriptor>(exp.desc);

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

                // TODO Add type to `ctx`.

                type_ctx.add_struct({exp.name, reference_location}, std::move(members));
            }
            else
            {
                throw std::runtime_error(fmt::format("Import resolution not implemented for symbol '{}'.", exp.name));
            }
        }
    }

    // TODO resolve imported types.
}

}    // namespace slang::resolve