/**
 * slang - a simple scripting language.
 *
 * instruction opcodes.
 *
 * \author Felix Lubbe
 * \copyright Copyright (c) 2025
 * \license Distributed under the MIT software license (see accompanying LICENSE.txt).
 */

#include <format>
#include <utility>

#include "opcodes.h"

namespace slang
{

/*
 * opcodes.
 */

std::string to_string(opcode op)
{
    switch(op)
    {
    case opcode::aconst_null: return "aconst_null";
    case opcode::iconst: return "iconst";
    case opcode::lconst: return "lconst";
    case opcode::fconst: return "fconst";
    case opcode::dconst: return "dconst";
    case opcode::sconst: return "sconst";
    case opcode::iload: return "iload";
    case opcode::lload: return "lload";
    case opcode::fload: return "fload";
    case opcode::dload: return "dload";
    case opcode::aload: return "aload";
    case opcode::istore: return "istore";
    case opcode::lstore: return "lstore";
    case opcode::fstore: return "fstore";
    case opcode::dstore: return "dstore";
    case opcode::astore: return "astore";
    case opcode::caload: return "caload";
    case opcode::saload: return "saload";
    case opcode::iaload: return "iaload";
    case opcode::laload: return "laload";
    case opcode::faload: return "faload";
    case opcode::daload: return "daload";
    case opcode::aaload: return "aaload";
    case opcode::castore: return "castore";
    case opcode::sastore: return "sastore";
    case opcode::iastore: return "iastore";
    case opcode::lastore: return "lastore";
    case opcode::fastore: return "fastore";
    case opcode::dastore: return "dastore";
    case opcode::aastore: return "aastore";
    case opcode::iadd: return "iadd";
    case opcode::ladd: return "ladd";
    case opcode::fadd: return "fadd";
    case opcode::dadd: return "dadd";
    case opcode::isub: return "isub";
    case opcode::lsub: return "lsub";
    case opcode::fsub: return "fsub";
    case opcode::dsub: return "dsub";
    case opcode::imul: return "imul";
    case opcode::lmul: return "lmul";
    case opcode::fmul: return "fmul";
    case opcode::dmul: return "dmul";
    case opcode::idiv: return "idiv";
    case opcode::ldiv: return "ldiv";
    case opcode::fdiv: return "fdiv";
    case opcode::ddiv: return "ddiv";
    case opcode::i2c: return "i2c";
    case opcode::i2s: return "i2s";
    case opcode::i2l: return "i2l";
    case opcode::i2f: return "i2f";
    case opcode::i2d: return "i2d";
    case opcode::l2i: return "l2i";
    case opcode::l2f: return "l2f";
    case opcode::l2d: return "l2d";
    case opcode::f2i: return "f2i";
    case opcode::f2l: return "f2l";
    case opcode::f2d: return "f2d";
    case opcode::d2i: return "d2i";
    case opcode::d2l: return "d2l";
    case opcode::d2f: return "d2f";
    case opcode::dup: return "dup";
    case opcode::dup2: return "dup2";
    case opcode::adup: return "adup";
    case opcode::dup_x1: return "dup_x1";
    case opcode::dup_x2: return "dup_x2";
    case opcode::pop: return "pop";
    case opcode::pop2: return "pop2";
    case opcode::apop: return "apop";
    case opcode::invoke: return "invoke";
    case opcode::new_: return "new";
    case opcode::newarray: return "newarray";
    case opcode::anewarray: return "anewarray";
    case opcode::arraylength: return "arraylength";
    case opcode::checkcast: return "checkcast";
    case opcode::ret: return "ret";
    case opcode::iret: return "iret";
    case opcode::lret: return "lret";
    case opcode::fret: return "fret";
    case opcode::dret: return "dret";
    case opcode::sret: return "sret";
    case opcode::aret: return "aret";
    case opcode::setfield: return "setfield";
    case opcode::getfield: return "getfield";
    case opcode::iand: return "iand";
    case opcode::land: return "land";
    case opcode::ior: return "ior";
    case opcode::lor: return "lor";
    case opcode::ixor: return "ixor";
    case opcode::lxor: return "lxor";
    case opcode::ishl: return "ishl";
    case opcode::lshl: return "lshl";
    case opcode::ishr: return "ishr";
    case opcode::lshr: return "lshr";
    case opcode::imod: return "imod";
    case opcode::lmod: return "lmod";
    case opcode::icmpl: return "icmpl";
    case opcode::lcmpl: return "lcmpl";
    case opcode::fcmpl: return "fcmpl";
    case opcode::dcmpl: return "dcmpl";
    case opcode::icmple: return "icmple";
    case opcode::lcmple: return "lcmple";
    case opcode::fcmple: return "fcmple";
    case opcode::dcmple: return "dcmple";
    case opcode::icmpg: return "icmpg";
    case opcode::lcmpg: return "lcmpg";
    case opcode::fcmpg: return "fcmpg";
    case opcode::dcmpg: return "dcmpg";
    case opcode::icmpge: return "icmpge";
    case opcode::lcmpge: return "lcmpge";
    case opcode::fcmpge: return "fcmpge";
    case opcode::dcmpge: return "dcmpge";
    case opcode::icmpeq: return "icmpeq";
    case opcode::lcmpeq: return "lcmpeq";
    case opcode::fcmpeq: return "fcmpeq";
    case opcode::dcmpeq: return "dcmpeq";
    case opcode::icmpne: return "icmpne";
    case opcode::lcmpne: return "lcmpne";
    case opcode::fcmpne: return "fcmpne";
    case opcode::dcmpne: return "dcmpne";
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

    if(op_base >= std::to_underlying(opcode::opcode_count))
    {
        throw opcode_error(std::format("Invalid opcode '{}'.", op_base));
    }

    op = static_cast<opcode>(op_base);

    return ar;
}

}    // namespace slang
