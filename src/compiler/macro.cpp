/**
 * slang - a simple scripting language.
 *
 * macro collection / expansion environment.
 *
 * \author Felix Lubbe
 * \copyright Copyright (c) 2025
 * \license Distributed under the MIT software license (see accompanying LICENSE.txt).
 */

#include <algorithm>

#include "compiler/macro.h"

namespace slang::macro
{

void env::add_macro(
  std::string name,
  module_::macro_descriptor desc,
  std::optional<std::string> import_path)
{
    if(std::ranges::find_if(
         macros,
         [&name, &import_path](const std::unique_ptr<macro>& m) -> bool
         {
             return m->get_name() == name
                    && m->get_import_path() == import_path;
         })
       != macros.end())
    {
        // FIXME Should be some redefinition_error. Could be from slang::collect, but needs to be adapted.
        throw std::runtime_error(std::format("Macro '{}' already defined.", name));
    }

    macros.emplace_back(
      std::make_unique<macro>(
        std::move(name),
        std::move(desc),
        std::move(import_path)));
}

}    // namespace slang::macro