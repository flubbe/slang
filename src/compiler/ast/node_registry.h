/**
 * slang - a simple scripting language.
 *
 * Node registry, associating node identifiers with constructors
 * of subclasses of `expression`.
 *
 * \author Felix Lubbe
 * \copyright Copyright (c) 2025
 * \license Distributed under the MIT software license (see accompanying LICENSE.txt).
 */

#pragma once

#include <cstdint>
#include <memory>

#include <fmt/core.h>

#include "archives/archive.h"
#include "utils.h"

namespace slang::ast
{

/*
 * Forward declarations.
 */

class expression;

/** Identifiers. */
enum class node_identifier : std::uint8_t
{
    null, /** The null object. */

    expression,
    named_expression,
    literal_expression,
    type_cast_expression,
    namespace_access_expression,
    access_expression,
    import_expression,
    directive_expression,
    variable_reference_expression,
    variable_declaration_expression,
    constant_declaration_expression,
    array_initializer_expression,
    struct_definition_expression,
    struct_anonymous_initializer_expression,
    named_initializer,
    struct_named_initializer_expression,
    binary_expression,
    unary_expression,
    new_expression,
    null_expression,
    postfix_expression,
    block,
    function_expression,
    call_expression,
    macro_invocation,
    return_statement,
    if_statement,
    while_statement,
    break_statement,
    continue_statement,
    macro_branch,
    macro_expression_list,
    macro_expression,

    last = macro_expression
};

/**
 * `node_identifier` serializer.
 *
 * @param ar The archive to use for serialization.
 * @param i The node identifier.
 * @returns The input archive.
 */
inline archive& operator&(archive& ar, node_identifier& i)
{
    std::uint8_t i_u8 = static_cast<std::uint8_t>(i);
    ar & i_u8;
    if(i > node_identifier::last)
    {
        throw serialization_error(
          fmt::format(
            "Node identifier out of range ({} > {}).",
            i_u8,
            static_cast<std::uint8_t>(i)));
    }
    return ar;
}

/**
 * Default-construct a node from an identifier.
 *
 * @param id The node identifier.
 * @returns Return an expression node of the specified type.
 * @throws Throws a `std::runtime_error` if the identifier is unknown.
 */
std::unique_ptr<expression> construct(node_identifier id);

/**
 * Default-construct a node from an identifier.
 *
 * @param id The node identifier.
 * @returns Return an expression node of the specified type.
 * @throws Throws a `std::runtime_error` if the identifier is unknown.
 */
template<typename T>
std::unique_ptr<T> construct(node_identifier id)
{
    return std::unique_ptr<T>{static_cast<T*>(construct(id).release())};
}

template<typename T>
struct expression_serializer
{
    T& expr;

public:
    expression_serializer(T& expr)
    : expr{expr}
    {
    }

    void serialize(archive& ar)
    {
        auto id = static_cast<std::uint8_t>(expr ? expr->get_id() : node_identifier::null);
        ar & id;
        if(id > static_cast<std::uint8_t>(node_identifier::last))
        {
            throw serialization_error(
              fmt::format(
                "Invalid AST node id ({} > {}).",
                id,
                static_cast<std::uint8_t>(node_identifier::last)));
        }

        if(ar.is_reading())
        {
            expr = T{
              static_cast<typename T::element_type*>(
                construct(static_cast<node_identifier>(id))
                  .release())};
        }

        if(expr != nullptr)
        {
            expr->serialize(ar);
        }
    }
};

template<typename T>
archive& operator&(archive& ar, expression_serializer<T> e)
{
    e.serialize(ar);
    return ar;
}

template<typename T>
struct expression_vector_serializer
{
    std::vector<T>& exprs;

public:
    expression_vector_serializer(std::vector<T>& exprs)
    : exprs{exprs}
    {
    }

    void serialize(archive& ar)
    {
        vle_int len{utils::numeric_cast<std::int64_t>(exprs.size())};
        ar & len;
        if(ar.is_reading())
        {
            exprs.reserve(len.i);

            for(std::int64_t i = 0; i < len.i; ++i)
            {
                std::uint8_t id{0};
                ar & id;
                if(id > static_cast<std::uint8_t>(node_identifier::last))
                {
                    throw serialization_error(
                      fmt::format(
                        "Invalid AST node id ({} >= {}).",
                        id,
                        static_cast<std::uint8_t>(node_identifier::last)));
                }

                exprs.emplace_back(
                  static_cast<typename T::pointer>(
                    construct(static_cast<node_identifier>(id)).release()));
                if(exprs[i] != nullptr)
                {
                    exprs[i]->serialize(ar);
                }
            }
        }
        else
        {
            for(std::int64_t i = 0; i < len.i; ++i)
            {
                if(exprs[i] != nullptr)
                {
                    auto id = static_cast<std::uint8_t>(exprs[i]->get_id());
                    ar & id;
                    exprs[i]->serialize(ar);
                }
                else
                {
                    ar& constant_serializer{node_identifier::null};
                }
            }
        }
    }
};

template<typename T>
archive& operator&(archive& ar, expression_vector_serializer<T> e)
{
    e.serialize(ar);
    return ar;
}

}    // namespace slang::ast
