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
    case opcode::aconst_null: return "aconst_null";
    case opcode::iconst: return "iconst";
    case opcode::fconst: return "fconst";
    case opcode::sconst: return "sconst";
    case opcode::iload: return "iload";
    case opcode::fload: return "fload";
    case opcode::aload: return "aload";
    case opcode::aaload: return "aaload";
    case opcode::istore: return "istore";
    case opcode::fstore: return "fstore";
    case opcode::astore: return "astore";
    case opcode::aastore: return "aastore";
    case opcode::iaload: return "iaload";
    case opcode::faload: return "faload";
    case opcode::saload: return "saload";
    case opcode::iastore: return "iastore";
    case opcode::fastore: return "fastore";
    case opcode::sastore: return "sastore";
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
    case opcode::idup: return "idup";
    case opcode::fdup: return "fdup";
    case opcode::adup: return "adup";
    case opcode::dup_x1: return "dup_x1";
    case opcode::pop: return "pop";
    case opcode::apop: return "apop";
    case opcode::invoke: return "invoke";
    case opcode::new_: return "new";
    case opcode::newarray: return "newarray";
    case opcode::anewarray: return "anewarray";
    case opcode::arraylength: return "arraylength";
    case opcode::checkcast: return "checkcast";
    case opcode::ret: return "ret";
    case opcode::iret: return "iret";
    case opcode::fret: return "fret";
    case opcode::sret: return "sret";
    case opcode::aret: return "aret";
    case opcode::setfield: return "setfield";
    case opcode::getfield: return "getfield";
    case opcode::iand: return "iand";
    case opcode::land: return "land";
    case opcode::ior: return "ior";
    case opcode::lor: return "lor";
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
    case opcode::acmpeq: return "acmpeq";
    case opcode::acmpne: return "acmpne";
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
