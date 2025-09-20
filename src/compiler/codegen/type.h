/**
 * slang - a simple scripting language.
 *
 * type used in code generation.
 *
 * \author Felix Lubbe
 * \copyright Copyright (c) 2025
 * \license Distributed under the MIT software license (see accompanying LICENSE.txt).
 */

#pragma once

#include <string>
#include <optional>

#include "compiler/type.h"

namespace slang::codegen
{

namespace ty = slang::typing;

/*
 * Forward declarations.
 */

struct name_resolver; /* codegen.h */

/** Lowered type. */
enum class type_kind : std::uint8_t
{
    void_, /** Void type. */
    null,  /** Null type. */
    i32,   /** 32-bit signed integer.  */
    f32,   /** 32-bit IEEE754 float. */
    str,   /** String. */
    ref,   /** Any reference, including arrays. */
};

/**
 * Convert a `type_kind` to a readable string.
 *
 * @param kind The type kind.
 * @returns Returns a readable string.
 */
std::string to_string(type_kind kind);

/** Type of a value. */
class type
{
    /** Front-end type id, if known. */
    std::optional<ty::type_id> type_id{std::nullopt};

    /** The lowered type. */
    type_kind back_end_type;

public:
    /** Defaulted and deleted constructors. */
    type() = default;
    type(const type&) = default;
    type(type&&) = default;

    /** Defaulted destructor. */
    ~type() = default;

    /** Default assignments. */
    type& operator=(const type&) = default;
    type& operator=(type&&) = default;

    /**
     * Construct a type.
     *
     * @param type_id The front-end type id.
     * @param back_end_type The back-end type.
     */
    type(ty::type_id type_id,
         type_kind back_end_type)
    : type_id{type_id}
    , back_end_type{back_end_type}
    {
    }

    /**
     * Construct a type.
     *
     * @param back_end_type The back-end type.
     */
    type(type_kind back_end_type)
    : back_end_type{back_end_type}
    {
    }

    /** Comparison. */
    bool operator==(const type& other) const
    {
        return type_id == other.type_id
               && back_end_type == other.back_end_type;
    }

    /** Comparison. */
    bool operator!=(const type& other) const
    {
        return !(*this == other);
    }

    /** Get the type id. */
    [[nodiscard]]
    std::optional<ty::type_id> get_type_id() const
    {
        return type_id;
    }

    /** Get the type kind / back-end type. */
    type_kind get_type_kind() const
    {
        return back_end_type;
    }

    /**
     * Get a readable string representation of the type.
     *
     * @param resolver Optional name resolver.
     * @returns Returns a string representation fo the type.
     */
    [[nodiscard]]
    std::string to_string(const name_resolver* resolver = nullptr) const;
};

}    // namespace slang::codegen
