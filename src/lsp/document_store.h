/**
 * slang - a simple scripting language.
 *
 * In-memory document store for LSP.
 *
 * \author Felix Lubbe
 * \copyright Copyright (c) 2026
 * \license Distributed under the MIT software license (see accompanying LICENSE.txt).
 */

#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <unordered_map>

namespace slang::lsp
{

/** Tracked open text document. */
struct document
{
    std::string uri;
    std::int64_t version{0};
    std::string text;
};

/**
 * In-memory document store keyed by document URI.
 */
class document_store
{
public:
    document_store() = default;

    /**
     * Add or update an opened document.
     */
    void did_open(std::string uri, std::int64_t version, std::string text);

    /**
     * Update the full content and version of an open document.
     *
     * @return true if the document was found and updated; false if not found.
     */
    bool did_change(const std::string& uri, std::int64_t version, std::string text);

    /**
     * Remove a closed document from the store.
     *
     * @return true if the document was present and removed; false otherwise.
     */
    bool did_close(const std::string& uri);

    /**
     * Retrieve a pointer to an open document by URI.
     */
    [[nodiscard]] const document* get(const std::string& uri) const;

    /**
     * Check if a document is currently open.
     */
    [[nodiscard]] bool contains(const std::string& uri) const;

    /**
     * Number of tracked documents.
     */
    [[nodiscard]] std::size_t size() const noexcept
    {
        return docs.size();
    }

    /**
     * Clear all tracked documents.
     */
    void clear()
    {
        docs.clear();
    }

private:
    std::unordered_map<std::string, document> docs;
};

}    // namespace slang::lsp
