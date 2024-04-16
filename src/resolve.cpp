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

void context::resolve_imports(slang::codegen::context& ctx, slang::typing::context& type_ctx)
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

                std::vector<slang::codegen::value> prototype_arg_types;
                std::transform(desc.signature.arg_types.cbegin(), desc.signature.arg_types.cend(), std::back_inserter(prototype_arg_types),
                               [](const std::string& arg)
                               {
                                   if(ty::is_builtin_type(arg))
                                   {
                                       return slang::codegen::value{arg};
                                   }

                                   return slang::codegen::value{"aggregate", arg};
                               });

                ctx.add_prototype(exp.name, desc.signature.return_type, prototype_arg_types, import_path);

                std::vector<token> arg_types;
                for(auto& arg: desc.signature.arg_types)
                {
                    arg_types.emplace_back(arg, reference_location);
                }

                type_ctx.add_function({exp.name, reference_location}, std::move(arg_types), {desc.signature.return_type, reference_location});
            }
            else
            {
                throw std::runtime_error(fmt::format("Import resolution not implemented for non-function symbol '{}'.", exp.name));
            }
        }
    }
}

}    // namespace slang::resolve