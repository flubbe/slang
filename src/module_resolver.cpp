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
#include "module_resolver.h"
#include "package.h"

namespace slang::module_
{

module_resolver::module_resolver(
  file_manager& file_mgr,
  fs::path path,
  bool transitive)
: path{std::move(path)}
, transitive{transitive}
{
    auto read_ar = file_mgr.open(this->path, slang::file_manager::open_mode::read);
    (*read_ar) & mod;
}

}    // namespace slang::module_
