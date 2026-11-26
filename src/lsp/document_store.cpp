/**
 * slang - a simple scripting language.
 *
 * In-memory document store for LSP.
 *
 * \author Felix Lubbe
 * \copyright Copyright (c) 2026
 * \license Distributed under the MIT software license (see accompanying LICENSE.txt).
 */

#include "lsp/document_store.h"

namespace slang::lsp
{

void document_store::did_open(std::string uri, std::int64_t version, std::string text)
{
    docs[uri] = document{
      .uri = uri,
      .version = version,
      .text = std::move(text)};
}

bool document_store::did_change(const std::string& uri, std::int64_t version, std::string text)
{
    auto it = docs.find(uri);
    if(it == docs.end())
    {
        return false;
    }

    if(version < it->second.version)
    {
        return false;
    }

    it->second.version = version;
    it->second.text = std::move(text);
    return true;
}

bool document_store::did_close(const std::string& uri)
{
    return docs.erase(uri) > 0;
}

const document* document_store::get(const std::string& uri) const
{
    auto it = docs.find(uri);
    if(it == docs.end())
    {
        return nullptr;
    }
    return &it->second;
}

bool document_store::contains(const std::string& uri) const
{
    return docs.contains(uri);
}

}    // namespace slang::lsp
