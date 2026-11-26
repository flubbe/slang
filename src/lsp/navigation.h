/**
 * slang - a simple scripting language.
 *
 * LSP navigation features: Document Symbols, Go to Definition, Find References.
 *
 * \author Felix Lubbe
 * \copyright Copyright (c) 2026
 * \license Distributed under the MIT software license (see accompanying LICENSE.txt).
 */

#pragma once

#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "lsp/diagnostics.h"

namespace slang
{
class file_manager;
}    // namespace slang

namespace slang::lsp
{

/** LSP SymbolKind. */
enum class symbol_kind : int
{
    file = 1,
    module = 2,
    namespace_ = 3,
    package = 4,
    class_ = 5,
    method = 6,
    property = 7,
    field = 8,
    constructor = 9,
    enum_ = 10,
    interface_ = 11,
    function = 12,
    variable = 13,
    constant = 14,
    string = 15,
    number = 16,
    boolean = 17,
    array = 18,
    object = 19,
    key = 20,
    null_ = 21,
    enum_member = 22,
    struct_ = 23,
    event = 24,
    operator_ = 25,
    type_parameter = 26
};

/** LSP DocumentSymbol. */
struct document_symbol
{
    std::string name;
    symbol_kind kind{symbol_kind::variable};
    lsp_range range;
    lsp_range selection_range;
    std::vector<document_symbol> children;
};

/**
 * Extract hierarchical document symbols from document source code.
 */
std::vector<document_symbol> compute_document_symbols(const std::string& source);

/**
 * Serialize a list of document symbols to a JSON result string.
 */
std::string format_document_symbols_result(const std::vector<document_symbol>& symbols);

/**
 * Locate the definition of the symbol at the given position.
 *
 * @param uri Document URI.
 * @param source Document source code.
 * @param pos Cursor position to look up.
 * @param searchPaths Additional directories to search for .sl files.
 * @return Location of the symbol definition, or nullopt if not found.
 */
std::optional<lsp_location> find_definition(
  const std::string& uri,
  const std::string& source,
  lsp_position pos,
  const std::vector<std::string>& searchPaths);

/**
 * Serialize definition result to a JSON result string (Location or null).
 */
std::string format_definition_result(const std::optional<lsp_location>& loc);

/**
 * Find all references to the symbol at the given position.
 */
std::vector<lsp_location> find_references(
  const std::string& uri,
  const std::string& source,
  lsp_position pos,
  bool include_declaration);

/**
 * Serialize references result to a JSON result string.
 */
std::string format_references_result(const std::vector<lsp_location>& locations);

/**
 * Convert a filesystem path to a `file://` URI.
 */
std::string file_uri_from_path(const std::filesystem::path& path);

/**
 * Configure a `file_manager` with default and user-provided search paths.
 */
void configure_search_paths(
  slang::file_manager& file_manager,
  const std::string& uri,
  const std::vector<std::string>& search_paths);

/**
 * Resolve the `.sl` source file path for a module qualified name (e.g. `math` or `a::b`).
 */
std::optional<std::filesystem::path> resolve_module_source_path(
  const slang::file_manager& file_manager,
  std::string_view module_qualified_name);

}    // namespace slang::lsp
