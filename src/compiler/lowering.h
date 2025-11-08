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

#include "compiler/codegen/type.h"
#include "shared/stack_value.h"
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
enum class type_kind : std::uint8_t; /* codegen/codegen.h */
class type;                          /* codegen/codegen.h */
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

    /** Built-in types. */
    cg::type null_type;
    cg::type void_type;
    cg::type i32_type;
    cg::type f32_type;
    cg::type str_type;

    /** Initialize the built-in types. */
    void initialize_builtins();

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
        initialize_builtins();
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
     * Dereference a type. Needs to have the front-end type id set.
     *
     * @param type The type to dereference.
     * @param Returns the dereferences type.
     * @throws Throws a `cg::codegen_error` if the front-end type is not set,
     *         or `type` is not an array type.
     */
    cg::type deref(const cg::type& type);

    /**
     * Get stack type size.
     *
     * @param type The type.
     * @returns Returns the stack type size as a `stack_value`.
     */
    stack_value get_stack_value(const cg::type& type) const;

    /** Print the contents as a readable string. */
    [[nodiscard]]
    std::string to_string() const;
};

}    // namespace slang::lowering
