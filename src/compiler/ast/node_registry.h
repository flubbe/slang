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
#include <type_traits>

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
    null = 0, /** The null object. */

    expression = 1,
    named_expression = 2,
    literal_expression = 3,
    type_cast_expression = 4,
    namespace_access_expression = 5,
    access_expression = 6,
    import_expression = 7,
    directive_expression = 8,
    variable_reference_expression = 9,
    variable_declaration_expression = 10,
    constant_declaration_expression = 11,
    array_initializer_expression = 12,
    struct_definition_expression = 13,
    struct_anonymous_initializer_expression = 14,
    named_initializer = 15,
    struct_named_initializer_expression = 16,
    binary_expression = 17,
    unary_expression = 18,
    new_expression = 19,
    null_expression = 20,
    postfix_expression = 21,
    block = 22,
    function_expression = 23,
    call_expression = 24,
    macro_invocation = 25,
    return_statement = 26,
    if_statement = 27,
    while_statement = 28,
    break_statement = 29,
    continue_statement = 30,
    macro_branch = 31,
    macro_expression_list = 32,
    macro_expression = 33,

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
    auto i_u8 = static_cast<std::uint8_t>(i);
    ar & i_u8;
    if(i_u8 > static_cast<std::uint8_t>(node_identifier::last))
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

/** AST expression serializer. */
template<
  typename T,
  typename = std::enable_if_t<
    (utils::is_shared_ptr_v<T> || utils::is_unique_ptr_v<T>)
    && std::is_base_of_v<expression, typename T::element_type>>>
struct expression_serializer
{
    /** Reference to the expression. */
    T& expr;

    /** Delete default constructors. */
    expression_serializer() = delete;
    expression_serializer(const expression_serializer&) = delete;
    expression_serializer(expression_serializer&&) = delete;

    /** Delete assignments. */
    expression_serializer& operator=(const expression_serializer) = delete;
    expression_serializer& operator=(expression_serializer&&) = delete;

    /** Default destructor. */
    ~expression_serializer() = default;

    /**
     * Create an expression serializer.
     *
     * @param expr Reference to the expression to be serialized.
     */
    explicit expression_serializer(T& expr)
    : expr{expr}
    {
    }

    /**
     * Expression serializer.
     *
     * @param ar The archive to use for serialization.
     */
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

/**
 * Expression serializer.
 *
 * @param ar The archive to use for serialization.
 * @param expr The expression to serialize.
 */
template<typename T>
archive& operator&(archive& ar, expression_serializer<T> expr)
{
    expr.serialize(ar);
    return ar;
}

/** AST expression serializer, vector version. */
template<
  typename T,
  typename = std::enable_if_t<
    (utils::is_shared_ptr_v<T> || utils::is_unique_ptr_v<T>)
    && std::is_base_of_v<expression, typename T::element_type>>>
struct expression_vector_serializer
{
    /** Reference to the expression vector. */
    std::vector<T>& exprs;

    /** Delete default constructors. */
    expression_vector_serializer() = delete;
    expression_vector_serializer(const expression_vector_serializer&) = delete;
    expression_vector_serializer(expression_vector_serializer&&) = delete;

    /** Delete assignments. */
    expression_vector_serializer& operator=(const expression_vector_serializer) = delete;
    expression_vector_serializer& operator=(expression_vector_serializer&&) = delete;

    /** Default destructor. */
    ~expression_vector_serializer() = default;

    /**
     * Create a serializer for a vector of expressions.
     *
     * @param exprs Vector of expressions.
     */
    explicit expression_vector_serializer(std::vector<T>& exprs)
    : exprs{exprs}
    {
    }

    /**
     * Expression serializer.
     *
     * @param ar The archive to use for serialization.
     */
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

/**
 * Expression serializer, vector version.
 *
 * @param ar The archive to use for serialization.
 * @param expr Vector of expression to serialize.
 */
template<typename T>
archive& operator&(archive& ar, expression_vector_serializer<T> exprs)
{
    exprs.serialize(ar);
    return ar;
}

}    // namespace slang::ast
