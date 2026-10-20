/**
 * slang - a simple scripting language.
 *
 * token stream for parsing.
 *
 * \author Felix Lubbe
 * \copyright Copyright (c) 2026
 * \license Distributed under the MIT software license (see accompanying LICENSE.txt).
 */

#pragma once

#include <memory>
#include <vector>

#include "archives/archive.h"
#include "shared/token.h"

namespace slang
{

/**
 * Immutable token stream.
 *
 * The stream holds a vector of tokens. It provides lightweight copy-on-write
 * semantics by sharing the underlying vector; copying a token_stream is cheap.
 * Mutations are performed via explicit methods that return a new stream.
 */
class token_stream
{
    /** Shared storage - std::shared_ptr enables cheap copies. */
    std::shared_ptr<std::vector<token>> tokens;

public:
    /** Default constructor. */
    token_stream()
    : tokens{std::make_shared<std::vector<token>>()}
    {
    }

    /** Construct from an existing vector. */
    explicit token_stream(
      const std::vector<token>& vec)
    : tokens{std::make_shared<std::vector<token>>(vec)}
    {
    }

    /** Construct by moving a vector. */
    explicit token_stream(
      std::vector<token>&& vec)
    : tokens{std::make_shared<std::vector<token>>(std::move(vec))}
    {
    }

    /** Accessors. */
    [[nodiscard]]
    const std::vector<token>& data() const
    {
        return *tokens;
    }

    [[nodiscard]]
    std::size_t size() const
    {
        return tokens->size();
    }

    [[nodiscard]]
    bool empty() const
    {
        return tokens->empty();
    }

    const token& operator[](std::size_t i) const
    {
        return (*tokens)[i];
    }

    const token& at(std::size_t i) const
    {
        return tokens->at(i);
    }

    const token& front() const
    {
        return tokens->front();
    }

    const token& back() const
    {
        return tokens->back();
    }

    auto begin() const
    {
        return tokens->begin();
    }

    auto end() const
    {
        return tokens->end();
    }

    auto cbegin() const
    {
        return tokens->cbegin();
    }

    auto cend() const
    {
        return tokens->cend();
    }

    /** Create a new stream with an additional token appended. */
    [[nodiscard]]
    token_stream append(
      const token& t) const
    {
        auto copy = std::make_shared<std::vector<token>>(*tokens);
        copy->push_back(t);
        token_stream res;
        res.tokens = std::move(copy);
        return res;
    }

    /** Concatenate two streams. */
    [[nodiscard]] token_stream concat(
      const token_stream& other) const
    {
        auto copy = std::make_shared<std::vector<token>>(*tokens);
        copy->insert(
          copy->end(),
          other.tokens->begin(),
          other.tokens->end());
        token_stream res;
        res.tokens = std::move(copy);
        return res;
    }

    /** Push a token in place (copy-on-write if shared). */
    void push_back(
      token t)
    {
        if(!tokens
           || tokens.use_count() > 1)
        {
            tokens = tokens
                       ? std::make_shared<std::vector<token>>(*tokens)
                       : std::make_shared<std::vector<token>>();
        }
        tokens->push_back(std::move(t));
    }

    /** Clear stream contents (copy-on-write if shared). */
    void clear()
    {
        if(tokens
           && tokens.use_count() == 1)
        {
            tokens->clear();
        }
        else
        {
            tokens = std::make_shared<std::vector<token>>();
        }
    }

    /** Serialization support. */
    friend archive& operator&(
      archive& ar,
      token_stream& ts);
};

/**
 * Serialize a `token_stream`.
 *
 * @param ar The archive to use.
 * @param ts The token stream.
 * @returns The input archive.
 */
archive& operator&(
  archive& ar,
  token_stream& ts);

}    // namespace slang
