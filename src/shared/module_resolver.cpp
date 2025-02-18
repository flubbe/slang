/**
 * slang - a simple scripting language.
 *
 * module resolver.
 *
 * \author Felix Lubbe
 * \copyright Copyright (c) 2025
 * \license Distributed under the MIT software license (see accompanying LICENSE.txt).
 */

#include "shared/type_utils.h"
#include "shared/module_resolver.h"
#include "package.h"

namespace slang::module_
{

module_resolver::module_resolver(
  file_manager& file_mgr,
  fs::path path,
  std::shared_ptr<resolution_recorder> recorder)
: path{std::move(path)}
, recorder{std::move(recorder)}
{
    // make sure we have an instruction recorder.
    if(!this->recorder)
    {
        throw resolution_error("Instruction recorder cannot be set to null.");
    }

    auto read_ar = file_mgr.open(this->path, slang::file_manager::open_mode::read);
    (*read_ar) & mod;

    // Record tables.
    this->recorder->section("Export table");
    for(auto& it: mod.header.exports)
    {
        this->recorder->record(it);

        if(it.type != module_::symbol_type::type)
        {
            continue;
        }
    }

    this->recorder->section("Constant table");
    for(auto& c: mod.header.constants)
    {
        this->recorder->constant(c);
    }

    this->recorder->section("Import table");
    for(auto& it: mod.header.imports)
    {
        this->recorder->record(it);
    }
}

}    // namespace slang::module_
