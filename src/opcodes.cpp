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

std::string to_string(opcode op)
{
    switch(op)
    {
    case opcode::iconst: return "iconst";
    case opcode::fconst: return "fconst";
    case opcode::sconst: return "sconst";
    case opcode::iload: return "iload";
    case opcode::fload: return "fload";
    case opcode::sload: return "sload";
    case opcode::istore: return "istore";
    case opcode::fstore: return "fstore";
    case opcode::sstore: return "sstore";
    case opcode::iadd: return "iadd";
    case opcode::fadd: return "fadd";
    case opcode::isub: return "isub";
    case opcode::fsub: return "fsub";
    case opcode::imul: return "imul";
    case opcode::fmul: return "fmul";
    case opcode::idiv: return "idiv";
    case opcode::fdiv: return "fdiv";
    case opcode::invoke: return "invoke";
    case opcode::ret: return "ret";
    case opcode::opcode_count:; /* fall-through */
    }

    return "unknown";
}

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
