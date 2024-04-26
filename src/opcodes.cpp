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
    case opcode::i2f: return "i2f";
    case opcode::f2i: return "f2i";
    case opcode::invoke: return "invoke";
    case opcode::ret: return "ret";
    case opcode::iret: return "iret";
    case opcode::fret: return "fret";
    case opcode::sret: return "sret";
    case opcode::iand: return "iand";
    case opcode::ior: return "ior";
    case opcode::ixor: return "ixor";
    case opcode::ishl: return "ishl";
    case opcode::ishr: return "ishr";
    case opcode::imod: return "imod";
    case opcode::icmpl: return "icmpl";
    case opcode::fcmpl: return "fcmpl";
    case opcode::icmple: return "icmple";
    case opcode::fcmple: return "fcmple";
    case opcode::icmpg: return "icmpg";
    case opcode::fcmpg: return "fcmpg";
    case opcode::icmpge: return "icmpge";
    case opcode::fcmpge: return "fcmpge";
    case opcode::icmpeq: return "icmpeq";
    case opcode::fcmpeq: return "fcmpeq";
    case opcode::icmpne: return "icmpne";
    case opcode::fcmpne: return "fcmpne";
    case opcode::jnz: return "jnz";
    case opcode::jmp: return "jmp";
    case opcode::label: return "label";
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
