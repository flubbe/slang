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

void context::resolve_imports(ty::context& ctx)
{
    const std::vector<std::vector<token>>& imports = ctx.get_imports();

    auto transform = [](const token& p) -> std::string
    { return p.s; };

    for(auto& import: imports)
    {
        if(import.size() == 0)
        {
            throw resolve_error("Cannot resolve empty import.");
        }

        auto reference_location = import[0].location;

        fs::path import_path = fs::path{utils::join(import, {transform}, "/")}.replace_extension(package::module_ext);
        fs::path resolved_path = mgr.resolve(import_path);
        std::unique_ptr<slang::file_archive> ar = mgr.open(resolved_path, slang::file_manager::open_mode::read);

        module_header hdr;
        (*ar) & hdr;

        for(auto& exp: hdr.exports)
        {
            if(exp.type == symbol_type::function)
            {
                auto& desc = std::get<function_descriptor>(exp.desc);

                std::vector<token> arg_types;
                for(auto& arg: desc.signature.arg_types)
                {
                    arg_types.emplace_back(arg, reference_location);
                }

                ctx.add_function({exp.name, reference_location}, std::move(arg_types), {desc.signature.return_type, reference_location});
            }
            else
            {
                throw std::runtime_error(fmt::format("Import resolution not implemented for non-function symbol '{}'.", exp.name));
            }
        }
    }
}

}    // namespace slang::resolve