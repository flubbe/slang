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
    aconst_null, /** Load a null reference onto the stack. */
    iconst,      /** Load i32 constant onto stack. */
    fconst,      /** Load f32 constant onto stack. */
    sconst,      /** Load str constant onto stack. */
    iload,       /** Load element at index 0 from i32 array onto stack. */
    fload,       /** Load element at index 0 from f32 array onto stack. */
    sload,       /** Load element at index 0 from str array onto stack. */
    aload,       /** Load address onto stack. */
    iaload,      /** Load i32 from array onto stack. */
    faload,      /** Load f32 from array onto stack. */
    saload,      /** Load str from array onto stack. */
    istore,      /** Store from stack into i32 array at index 0. */
    fstore,      /** Store from stack into f32 array at index 0. */
    sstore,      /** Store from stack into str array at index 0. */
    astore,      /** Store from stack into array. */
    iastore,     /** Store i32 from stack into array. */
    fastore,     /** Store f32 from stack into array. */
    sastore,     /** Store str from stack into array. */
    idup,        /** Duplicate top i32 at a given depth in the stack. */
    fdup,        /** Duplicate top f32 at a given depth in the stack. */
    adup,        /** Duplicate top address at a given depth in the stack. */
    dup_x1,      /** Duplicate top stack element and push it 2 down the stack. */
    pop,         /** Pop-discard a 4-byte value from the stack. */
    apop,        /** Pop-discard a reference from the stack. */
    iadd,        /** Add two i32 from the stack. */
    fadd,        /** Add two f32 from the stack. */
    isub,        /** Subtract two i32 from the stack. */
    fsub,        /** Subtract two f32 from the stack.  */
    imul,        /** Mulitply two i32 from the stack. */
    fmul,        /** Multiply two f32 from the stack. */
    idiv,        /** Divide two i32 from the stack. */
    fdiv,        /** Divide two f32 from the stack.  */
    imod,        /** Modulus of the division of two i32. */
    i2f,         /** Convert a i32 into an f32. */
    f2i,         /** Convert a f32 into an i32. */
    invoke,      /** Invoke a function. */
    new_,        /** Create a new struct. */
    newarray,    /** Create a new array for i32, f32 or references. */
    arraylength, /** Return the length of an array. */
    ret,         /** Return void from a function. */
    iret,        /** Return an i32 from a function. */
    fret,        /** Return an f32 from a function. */
    sret,        /** Return a str from a function. */
    aret,        /** Return an address. */
    setfield,    /** Set a field in a struct. */
    getfield,    /** Get a field from a struct. */
    iand,        /** Bitwise and for two i32 from the stack. */
    land,        /** Logical and for two i32 from the stack. */
    ior,         /** Bitwise or for two i32 from the stack. */
    lor,         /** Logical or for two i32 from the stack. */
    ixor,        /** Bitwise xor for two i32 from the stack. */
    ishl,        /** Left shift for i32. */
    ishr,        /** Right shift for i32. */
    icmpl,       /** Check if the first i32 is less than the second i32. */
    fcmpl,       /** Check if the first f32 is less than the second f32. */
    icmple,      /** Check if the first i32 is less than or equal to the second i32. */
    fcmple,      /** Check if the first f32 is less than or equal to the second f32. */
    icmpg,       /** Check if the first i32 is greater than the second i32. */
    fcmpg,       /** Check if the first f32 is greater than the second f32. */
    icmpge,      /** Check if the first i32 is greater than or equal to the second i32. */
    fcmpge,      /** Check if the first f32 is greater than or equal to the second f32. */
    icmpeq,      /** Check if two i32 are equal. */
    fcmpeq,      /** Check if two f32 are equal. */
    icmpne,      /** Check if two i32 are not equal. */
    fcmpne,      /** Check if two i32 are not equal. */
    acmpeq,      /** Check if two addresses are equal. */
    acmpne,      /** Check if two addresses are not equal. */
    jnz,         /** Jump if not zero. */
    jmp,         /** Unconditional jump. */
    label,       /** A label. Not executable */

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
    explicit opcode_error(const std::string& message)
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