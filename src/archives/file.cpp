/**
 * slang - a simple scripting language.
 *
 * portable archive and serialization support.
 *
 * \author Felix Lubbe
 * \copyright Copyright (c) 2025
 * \license Distributed under the MIT software license (see accompanying LICENSE.txt).
 */

#include <format>

#include "file.h"

namespace slang
{

file_archive::file_archive(fs::path path, bool read, bool write, endian target_byte_order)
: archive{read, write, true, target_byte_order}
, path{std::move(path)}
{
    if(read && write)
    {
        throw serialization_error(std::format("Cannot open file '{}' for reading and writing simultaneously.", this->path.c_str()));
    }

    if(write)
    {
        file.open(this->path, std::fstream::out | std::fstream::binary);
    }
    else if(read)
    {
        file.open(this->path, std::fstream::in | std::fstream::binary);
    }

    if(!file)
    {
        throw serialization_error(std::format("Unable to open file '{}'.", this->path.c_str()));
    }
}

}    // namespace slang
