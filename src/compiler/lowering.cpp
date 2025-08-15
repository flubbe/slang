/**
 * slang - a simple scripting language.
 *
 * type lowering context.
 *
 * \author Felix Lubbe
 * \copyright Copyright (c) 2025
 * \license Distributed under the MIT software license (see accompanying LICENSE.txt).
 */

#include "codegen.h"

namespace slang::lowering
{

const cg::type& context::lower(ty::type_id id)
{
    throw std::runtime_error("context::lower");
}

std::string context::get_name(ty::type_id id) const
{
    throw std::runtime_error("context::get_name (type id)");
}

std::string context::get_name(cg::type_kind kind) const
{
    throw std::runtime_error("context::get_name (kind)");
}

cg::type context::deref(const cg::type& type)
{
    throw std::runtime_error("context::deref");
}

}    // namespace slang::lowering