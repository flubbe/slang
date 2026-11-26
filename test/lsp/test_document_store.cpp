/**
 * slang - a simple scripting language.
 *
 * LSP document store tests.
 *
 * \author Felix Lubbe
 * \copyright Copyright (c) 2026
 * \license Distributed under the MIT software license (see accompanying LICENSE.txt).
 */

#include <gtest/gtest.h>

#include <string>

#include "../../src/lsp/document_store.h"

namespace
{

/* === Phase 4.1: Document lifecycle === */

TEST(document_store, open_and_get)
{
    slang::lsp::document_store store;
    const std::string uri = "file:///path/to/test.sl";

    EXPECT_EQ(store.size(), 0);
    EXPECT_FALSE(store.contains(uri));
    EXPECT_EQ(store.get(uri), nullptr);

    store.did_open(uri, 1, "fn main() -> int { return 0; }");
    EXPECT_EQ(store.size(), 1U);
    EXPECT_TRUE(store.contains(uri));

    const auto* doc = store.get(uri);
    ASSERT_NE(doc, nullptr);
    EXPECT_EQ(doc->version, 1);
    EXPECT_EQ(doc->text, "fn main() -> int { return 0; }");
}

TEST(document_store, change_updates_contents)
{
    slang::lsp::document_store store;
    const std::string uri = "file:///test.sl";

    store.did_open(uri, 1, "text v1");
    EXPECT_TRUE(store.did_change(uri, 2, "text v2"));

    const auto* doc = store.get(uri);
    ASSERT_NE(doc, nullptr);
    EXPECT_EQ(doc->version, 2);
    EXPECT_EQ(doc->text, "text v2");
}

TEST(document_store, change_nonexistent_document_fails)
{
    slang::lsp::document_store store;
    EXPECT_FALSE(
      store.did_change("file:///nonexistent.sl", 1, "text"));
}

TEST(document_store, close_removes_entry)
{
    slang::lsp::document_store store;
    const std::string uri = "file:///test.sl";

    store.did_open(uri, 1, "code");
    EXPECT_TRUE(store.did_close(uri));
    EXPECT_EQ(store.size(), 0U);
    EXPECT_FALSE(store.contains(uri));
    EXPECT_EQ(store.get(uri), nullptr);
}

TEST(document_store, close_again_fails)
{
    slang::lsp::document_store store;
    const std::string uri = "file:///test.sl";

    store.did_open(uri, 1, "code");
    store.did_close(uri);
    EXPECT_FALSE(store.did_close(uri));
}

/* === Phase 4.2: Multiple documents === */

TEST(document_store, multiple_documents_independent)
{
    slang::lsp::document_store store;
    store.did_open("file:///doc1.sl", 1, "code 1");
    store.did_open("file:///doc2.sl", 10, "code 2");

    EXPECT_EQ(store.size(), 2U);
    EXPECT_EQ(store.get("file:///doc1.sl")->text, "code 1");
    EXPECT_EQ(store.get("file:///doc2.sl")->text, "code 2");

    store.did_change("file:///doc1.sl", 2, "new code 1");
    EXPECT_EQ(store.get("file:///doc1.sl")->text, "new code 1");
    EXPECT_EQ(store.get("file:///doc2.sl")->text, "code 2");

    store.did_close("file:///doc1.sl");
    EXPECT_EQ(store.size(), 1U);
    EXPECT_FALSE(store.contains("file:///doc1.sl"));
    EXPECT_TRUE(store.contains("file:///doc2.sl"));
}

}    // namespace
