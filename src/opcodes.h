/**
 * slang - a simple scripting language.
 *
 * instruction opcodes.
 *
 * \author Felix Lubbe
 * \copyright Copyright (c) 2024
 * \license Distributed under the MIT software license (see accompanying LICENSE.txt).
 */

#pragma once

#include "archives/archive.h"

namespace slang
{

/** Opcode base type. */
using opcode_base = std::uint8_t;

/** Instruction opcodes. */
enum class opcode : opcode_base
{
    iconst, /** Load i32 constant onto stack. */
    fconst, /** Load f32 constant onto stack. */
    sconst, /** Load str constant onto stack. */
    iload,  /** Load i32 variable onto stack. */
    fload,  /** Load f32 variable onto stack. */
    sload,  /** Load str variable onto stack. */
    istore, /** Store i32 from stack. */
    fstore, /** Store f32 from stack. */
    sstore, /** Store str from stack. */
    iadd,   /** Add two i32 from the stack. */
    fadd,   /** Add two f32 from the stack. */
    isub,   /** Subtract two i32 from the stack. */
    fsub,   /** Subtract two f32 from the stack.  */
    imul,   /** Mulitply two i32 from the stack. */
    fmul,   /** Multiply two f32 from the stack. */
    idiv,   /** Divide two i32 from the stack. */
    fdiv,   /** Divide two f32 from the stack.  */
    i2f,    /** Convert a i32 into an f32. */
    f2i,    /** Convert a f32 into an i32. */
    invoke, /** Invoke a function. */
    ret,    /** Return void from a function. */
    iret,   /** Return an i32 from a function. */
    fret,   /** Return an f32 from a function. */
    sret,   /** Return a str from a function. */

    opcode_count /** Opcode count. Not an opcode. */
};

/** Get a string representation of the opcode. */
std::string to_string(opcode op);

/** A opcode error. */
class opcode_error : public std::runtime_error
{
public:
    /**
     * Construct a `opcode_error`.
     *
     * @param message The error message.
     */
    opcode_error(const std::string& message)
    : std::runtime_error{message}
    {
    }
};

/**
 * Opcode serializer.
 *
 * @param ar The archive to use for serialization.
 * @param op The opcode to serialize.
 *
 * @throws Throws an opcode_error if the (read) opcode is invalid.
 */
archive& operator&(archive& ar, opcode& op);

}    // namespace slang