/**
 * slang - a simple scripting language.
 *
 * type lowering context.
 *
 * \author Felix Lubbe
 * \copyright Copyright (c) 2025
 * \license Distributed under the MIT software license (see accompanying LICENSE.txt).
 */

#include "codegen/codegen.h"
#include "typing.h"

namespace slang::lowering
{

void context::initialize_builtins()
{
    null_type = {type_ctx.get_null_type(), cg::type_kind::null};
    void_type = {type_ctx.get_void_type(), cg::type_kind::void_};
    i32_type = {type_ctx.get_i32_type(), cg::type_kind::i32};
    f32_type = {type_ctx.get_f32_type(), cg::type_kind::f32};
    str_type = {type_ctx.get_str_type(), cg::type_kind::str};

    type_cache.insert({type_ctx.get_null_type(), null_type});
    type_cache.insert({type_ctx.get_void_type(), void_type});
    type_cache.insert({type_ctx.get_i32_type(), i32_type});
    type_cache.insert({type_ctx.get_f32_type(), f32_type});
    type_cache.insert({type_ctx.get_str_type(), str_type});
}

const cg::type& context::lower(ty::type_id id)
{
    auto it = type_cache.find(id);
    if(it != type_cache.end())
    {
        return it->second;
    }

    // Lower all other types to reference types, and cache type id.
    auto [insert_it, success] = type_cache.insert({id, cg::type{id, cg::type_kind::ref}});
    if(!success)
    {
        throw cg::codegen_error(
          std::format(
            "Could not cache type for lowering: {}",
            type_ctx.to_string(id)));
    }

    return insert_it->second;
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
    if(!type.get_type_id().has_value())
    {
        throw cg::codegen_error(
          std::format(
            "Could not deref type: Front-end type not set (back-end type is '{}')",
            cg::to_string(type.get_type_kind())));
    }

    if(!type_ctx.is_array(type.get_type_id().value()))
    {
        throw cg::codegen_error(
          std::format(
            "Could not deref non-array type '{}'.",
            type_ctx.to_string(type.get_type_id().value())));
    }

    return lower(type_ctx.get_base_type(type.get_type_id().value()));
}

std::string context::to_string() const
{
    std::string buf;

    for(const auto& [front_end_type, back_end_type]: type_cache)
    {
        auto type_info = type_ctx.get_type_info(front_end_type);
        if(type_info.kind != ty::type_kind::struct_)
        {
            continue;
        }

        ty::struct_info struct_info = std::get<ty::struct_info>(type_info.data);
        buf += std::format("%{} = type {{\n", struct_info.name);

        if(!struct_info.fields.empty())
        {
            for(const auto& field_info: struct_info.fields | std::views::take(struct_info.fields.size() - 1))
            {
                buf += std::format(
                  " {} %{},\n",
                  type_ctx.to_string(field_info.type),
                  field_info.name);
            }
            buf += std::format(
              " {} %{},\n",
              type_ctx.to_string(struct_info.fields.back().type),
              struct_info.fields.back().name);
        }

        buf += "}\n";
    }

    return buf;
}

}    // namespace slang::lowering