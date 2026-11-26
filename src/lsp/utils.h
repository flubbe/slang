/**
 * slang - a simple scripting language.
 *
 * Utilities.
 *
 * \author Felix Lubbe
 * \copyright Copyright (c) 2026
 * \license Distributed under the MIT software license (see accompanying LICENSE.txt).
 */

#include "filemanager.h"

namespace slang::lsp
{

/**
 * Add a search path.
 *
 * @note If the path is not a directory, it is ignored.
 * @param file_manager The file manager to add the search path to.
 * @param path The search path to add.
 */
inline void add_search_path(
  slang::file_manager& file_manager,
  const fs::path& path)
{
    std::error_code error;
    if(!fs::is_directory(path, error) || error)
    {
        return;
    }

    const fs::path canonical_path = fs::canonical(path, error);
    if(!error)
    {
        file_manager.add_search_path(canonical_path);
    }
}

/** Convert a `file://` URI to a filesystem path, if valid. */
inline std::optional<fs::path> path_from_file_uri(
  std::string_view uri)
{
    constexpr std::string_view prefix{"file://"};
    if(!uri.starts_with(prefix))
    {
        return std::nullopt;
    }

    std::string path{uri.substr(prefix.size())};
    if(path.starts_with("localhost/"))
    {
        path.erase(0, std::string_view{"localhost"}.size());
    }

    std::string decoded;
    decoded.reserve(path.size());
    for(std::size_t i = 0; i < path.size(); ++i)
    {
        if(path[i] != '%')
        {
            decoded += path[i];
            continue;
        }

        if(i + 2 >= path.size())
        {
            return std::nullopt;
        }

        unsigned int value = 0;
        const char* first = path.data() + i + 1;
        const char* last = first + 2;
        const auto [end, error] = std::from_chars(first, last, value, 16);
        if(error != std::errc{} || end != last)
        {
            return std::nullopt;
        }

        decoded += static_cast<char>(value);
        i += 2;
    }

#if defined(_WIN32)
    if(decoded.size() >= 3 && decoded[0] == '/' && decoded[2] == ':')
    {
        decoded.erase(0, 1);
    }
#endif

    return fs::path{decoded};
}

}    // namespace slang::lsp