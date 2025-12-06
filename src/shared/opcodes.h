/**
 * slang - a simple scripting language.
 *
 * instruction opcodes.
 *
 * \author Felix Lubbe
 * \copyright Copyright (c) 2025
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
    lconst,      /** Load i64 constant onto stack. */
    fconst,      /** Load f32 constant onto stack. */
    dconst,      /** Load f64 constant onto stack. */
    sconst,      /** Load str constant onto stack. */
    iload,       /** Load i32 from variable onto stack. */
    lload,       /** Load i64 from variable onto stack. */
    fload,       /** Load f32 from variable onto stack. */
    dload,       /** Load f64 from variable onto stack. */
    aload,       /** Load address from variable onto stack. */
    caload,      /** Load i8 from array onto stack. */
    saload,      /** Load i16 from array onto stack. */
    iaload,      /** Load i32 from array onto stack. */
    laload,      /** Load i64 from array onto stack. */
    faload,      /** Load f32 from array onto stack. */
    daload,      /** Load f64 from array onto stack. */
    aaload,      /** Load address from array onto stack. */
    istore,      /** Store i32 from stack into variable. */
    lstore,      /** Store i64 from stack into variable. */
    fstore,      /** Store f32 from stack into variable. */
    dstore,      /** Store f64 from stack into variable. */
    astore,      /** Store address from stack into variable. */
    castore,     /** Store i8 from stack into array. */
    sastore,     /** Store i16 from stack into array. */
    iastore,     /** Store i32 from stack into array. */
    lastore,     /** Store i64 from stack into array. */
    fastore,     /** Store f32 from stack into array. */
    dastore,     /** Store f64 from stack into array. */
    aastore,     /** Store address from stack into array. */
    dup,         /** Duplicate top category 1 value of the stack. */
    dup2,        /** Duplicate top category 2 value of the stack. */
    adup,        /** Duplicate top address of the stack. */
    dup_x1,      /** Duplicate top stack element and push it 2 elements down the stack. */
    dup_x2,      /** Duplicate top stack element and push it 3 elements down the stack. */
    dup2_x0,     /** Duplicate top 2 stack elements. */
    pop,         /** Pop-discard a 4-byte value from the stack. */
    pop2,        /** Pop-discard a 8-byte value from the stack. */
    apop,        /** Pop-discard a reference from the stack. */
    iadd,        /** Add two i32 from the stack. */
    ladd,        /** Add two i64 from the stack. */
    fadd,        /** Add two f32 from the stack. */
    dadd,        /** Add two f64 from the stack. */
    isub,        /** Subtract two i32 from the stack. */
    lsub,        /** Subtract two i64 from the stack. */
    fsub,        /** Subtract two f32 from the stack.  */
    dsub,        /** Subtract two f64 from the stack. */
    imul,        /** Multtply two i32 from the stack. */
    lmul,        /** Multtply two i64 from the stack. */
    fmul,        /** Multiply two f32 from the stack. */
    dmul,        /** Multiply two f64 from the stack. */
    idiv,        /** Divide two i32 from the stack. */
    ldiv,        /** Divide two i64 from the stack. */
    fdiv,        /** Divide two f32 from the stack.  */
    ddiv,        /** Divide two f64 from the stack.  */
    imod,        /** Modulus of the division of two i32. */
    lmod,        /** Modulus of the division of two i64. */
    i2c,         /** Convert a i32 into an i8. */
    i2s,         /** Convert a i32 into an i16. */
    i2l,         /** Convert a i32 into an i64. */
    i2f,         /** Convert a i32 into an f32. */
    i2d,         /** Convert a i32 into an f64. */
    l2i,         /** Convert a i64 into an i32. */
    l2f,         /** Convert a i64 into an f32. */
    l2d,         /** Convert a i64 into an f64. */
    f2i,         /** Convert a f32 into an i32. */
    f2l,         /** Convert a f32 into an i64. */
    f2d,         /** Convert a f32 into an f64. */
    d2i,         /** Convert a f64 into an i32. */
    d2l,         /** Convert a f64 into an i64. */
    d2f,         /** Convert a f64 into an f32. */
    invoke,      /** Invoke a function. */
    new_,        /** Create a new struct. */
    newarray,    /** Create a new array for i8, i16, i32, i64, f32, f64, str, ref. */
    anewarray,   /** Create a new array for struct types. */
    arraylength, /** Return the length of an array. */
    checkcast,   /** Check if an object is of a given type. */
    ret,         /** Return void from a function. */
    iret,        /** Return an i32 from a function. */
    lret,        /** Return an i64 from a function. */
    fret,        /** Return an f32 from a function. */
    dret,        /** Return an f64 from a function. */
    sret,        /** Return a str from a function. */
    aret,        /** Return an address. */
    setfield,    /** Set a field in a struct. */
    getfield,    /** Get a field from a struct. */
    iand,        /** Bitwise and for two i32 from the stack. */
    land,        /** Bitwise and for two i64 from the stack. */
    ior,         /** Bitwise or for two i32 from the stack. */
    lor,         /** Bitwise or for two i64 from the stack. */
    ixor,        /** Bitwise xor for two i32 from the stack. */
    lxor,        /** Bitwise xor for two i64 from the stack. */
    ishl,        /** Left shift for i32. */
    lshl,        /** Left shift for i64. */
    ishr,        /** Right shift for i32. */
    lshr,        /** Right shift for i64. */
    icmpl,       /** Check if the first i32 is less than the second i32. */
    lcmpl,       /** Check if the first i64 is less than the second i64. */
    fcmpl,       /** Check if the first f32 is less than the second f32. */
    dcmpl,       /** Check if the first f64 is less than the second f64. */
    icmple,      /** Check if the first i32 is less than or equal to the second i32. */
    lcmple,      /** Check if the first i64 is less than or equal to the second i64. */
    fcmple,      /** Check if the first f32 is less than or equal to the second f32. */
    dcmple,      /** Check if the first f64 is less than or equal to the second f64. */
    icmpg,       /** Check if the first i32 is greater than the second i32. */
    lcmpg,       /** Check if the first i64 is greater than the second i64. */
    fcmpg,       /** Check if the first f32 is greater than the second f32. */
    dcmpg,       /** Check if the first f64 is greater than the second f64. */
    icmpge,      /** Check if the first i32 is greater than or equal to the second i32. */
    lcmpge,      /** Check if the first i64 is greater than or equal to the second i64. */
    fcmpge,      /** Check if the first f32 is greater than or equal to the second f32. */
    dcmpge,      /** Check if the first f64 is greater than or equal to the second f64. */
    icmpeq,      /** Check if two i32 are equal. */
    lcmpeq,      /** Check if two i64 are equal. */
    fcmpeq,      /** Check if two f32 are equal. */
    dcmpeq,      /** Check if two f64 are equal. */
    icmpne,      /** Check if two i32 are not equal. */
    lcmpne,      /** Check if two i64 are not equal. */
    fcmpne,      /** Check if two f32 are not equal. */
    dcmpne,      /** Check if two f64 are not equal. */
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
    using std::runtime_error::runtime_error;
};

/**
 * Opcode serializer.
 *
 * @param ar The archive to use for serialization.
 * @param op The opcode to serialize.
 *
 * @throws Throws an `opcode_error` if the (read) opcode is invalid.
 */
archive& operator&(archive& ar, opcode& op);

}    // namespace slang
