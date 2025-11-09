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
#include <format>
#include <memory>
#include <type_traits>

#include "archives/archive.h"
#include "ast.h"
#include "utils.h"

namespace slang::ast
{

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
template<utils::smart_ptr T>
    requires(std::is_base_of_v<expression, typename T::element_type>)
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
        if(id > std::to_underlying(node_identifier::last))
        {
            throw serialization_error(
              std::format(
                "Invalid AST node id ({} > {}).",
                id,
                std::to_underlying(node_identifier::last)));
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
template<utils::smart_ptr T>
    requires(std::is_base_of_v<expression, typename T::element_type>)
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
                if(id > std::to_underlying(node_identifier::last))
                {
                    throw serialization_error(
                      std::format(
                        "Invalid AST node id ({} >= {}).",
                        id,
                        std::to_underlying(node_identifier::last)));
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
