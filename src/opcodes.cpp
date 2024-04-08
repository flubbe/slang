/**
 * slang - a simple scripting language.
 *
 * instruction opcodes.
 *
 * \author Felix Lubbe
 * \copyright Copyright (c) 2024
 * \license Distributed under the MIT software license (see accompanying LICENSE.txt).
 */

#include <fmt/core.h>

#include "opcodes.h"

namespace slang
{

archive& operator&(archive& ar, opcode& op)
{
    auto op_base = static_cast<opcode_base>(op);
    ar & op_base;

    if(op_base >= static_cast<opcode_base>(opcode::opcode_count))
    {
        throw opcode_error(fmt::format("Invalid opcode '{}'.", op_base));
    }

    op = static_cast<opcode>(op_base);

    return ar;
}

}    // namespace slang
