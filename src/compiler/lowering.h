/**
 * slang - a simple scripting language.
 *
 * type lowering context.
 *
 * \author Felix Lubbe
 * \copyright Copyright (c) 2025
 * \license Distributed under the MIT software license (see accompanying LICENSE.txt).
 */

#pragma once

#include <unordered_map>

#include "type.h"

/*
 * Forward declarations.
 */

namespace slang::typing
{
class context; /* typing.h */
}    // namespace slang::typing

namespace slang::codegen
{
enum class type_kind : std::uint8_t; /* codegen.h */
class type;                          /* codegen.h */
}    // namespace slang::codegen

namespace slang::lowering
{

namespace cg = slang::codegen;
namespace ty = slang::typing;

/**
 * Type lowering context.
 *
 * Provides a translation layer between front-end and back-end types.
 */
class context
{
    /** Type context. */
    const ty::context& type_ctx;

    /** Type cache. */
    std::unordered_map<
      ty::type_id,
      cg::type>
      type_cache;

public:
    /** Deleted constructors. */
    context() = delete;
    context(const context&) = delete;
    context(context&&) = delete;

    /**
     * Construct a type lowering context.
     *
     * @param type_ctx The type context.
     */
    context(const ty::context& type_ctx)
    : type_ctx{type_ctx}
    {
    }

    /** Destructor. */
    ~context() = default;

    /** Deleted assignments. */
    context& operator=(const context&) = delete;
    context& operator=(const context&&) = delete;

    /**
     * Lower a front-end type id to a back-end type.
     *
     * @param id The front-end type id.
     * @returns Returns a lowered type.
     */
    const cg::type& lower(ty::type_id id);

    /**
     * Return the type name for a type id.
     *
     * @param id The front-end type id.
     * @returns Returns a readable type name.
     */
    std::string get_name(ty::type_id id) const;

    /**
     * Return the type name for a lowered type.
     *
     * @param id The lowered type.
     * @returns Returns a readable type name.
     */
    std::string get_name(cg::type_kind kind) const;

    /**
     * Dereference a type. Needs to have the front-end type id set.
     *
     * @param type The type to dereference.
     * @param Returns the dereferences type.
     */
    cg::type deref(const cg::type& type);
};

}    // namespace slang::lowering
