/**
 * slang - a simple scripting language.
 *
 * code generation.
 *
 * \author Felix Lubbe
 * \copyright Copyright (c) 2024
 * \license Distributed under the MIT software license (see accompanying LICENSE.txt).
 */

#pragma once

#include <algorithm>
#include <stdexcept>
#include <list>
#include <string>
#include <unordered_map>
#include <vector>

#include <fmt/core.h>

#include "archives/archive.h"
#include "archives/memory.h"
#include "directive.h"
#include "module.h"

/*
 * Forward declarations.
 */

namespace slang
{
class instruction_emitter;  /* emitter.h */
class export_table_builder; /* emitter.h */
}    // namespace slang

namespace slang::ast
{
class expression; /* ast.h */
}    // namespace slang::ast

namespace slang::codegen
{

/** A code generation error. */
class codegen_error : public std::runtime_error
{
public:
    /**
     * Construct a `codegen_error`.
     *
     * @note Use the other constructor if you want to include location information in the error message.
     *
     * @param message The error message.
     */
    explicit codegen_error(const std::string& message)
    : std::runtime_error{message}
    {
    }

    /**
     * Construct a `codegen_error`.
     *
     * @param loc The error location in the source.
     * @param message The error message.
     */
    codegen_error(const slang::token_location& loc, const std::string& message);
};

/** Type classes. */
enum class type_class
{
    void_,   /** Void type. */
    null,    /** Null type. */
    i32,     /** 32-bit signed integer.  */
    f32,     /** 32-bit IEEE754 float. */
    str,     /** String. */
    struct_, /** Struct. */
    addr,    /** Anonymous address. */
    fn       /** A function. */
};

/**
 * Convert a built-in type to a `type_class`.
 *
 * @param s The type string to convert.
 * @return Returns the corresponding type class.
 * @throws Throws a `codegen_error` if the string is not a type class.
 */
type_class to_type_class(const std::string& s);

/** Type of a value. */
class type
{
    /** Type class. */
    type_class ty;

    /** Array dimensions (0 for scalar types). */
    std::size_t array_dims;

    /** Struct name. */
    std::optional<std::string> struct_name;

    /** Import path (for external struct's). */
    std::optional<std::string> import_path;

public:
    /** Defaulted and deleted constructors. */
    type() = delete;
    type(const type&) = default;
    type(type&&) = default;

    /** Default assignments. */
    type& operator=(const type&) = default;
    type& operator=(type&&) = default;

    /**
     * Construct a type.
     *
     * @param ty Type class.
     * @param array_dims Array dimensions. 0 for scalar types.
     * @param struct_name Struct name if `ty` is `type_class::struct_`.
     * @param import_path Import path of a struct (`std::nullopt` for the current module).
     */
    type(type_class ty,
         std::size_t array_dims,
         std::optional<std::string> struct_name = std::nullopt,
         std::optional<std::string> import_path = std::nullopt)
    : ty{ty}
    , array_dims{array_dims}
    , struct_name{std::move(struct_name)}
    , import_path{std::move(import_path)}
    {
    }

    /** Comparison. */
    bool operator==(const type& other) const
    {
        return ty == other.ty
               && array_dims == other.array_dims
               && struct_name == other.struct_name
               && import_path == other.import_path;
    }

    /** Comparison. */
    bool operator!=(const type& other) const
    {
        return !(*this == other);
    }

    /** Get the type class. */
    type_class get_type_class() const
    {
        return ty;
    }

    /** Return whether the type is an array. */
    bool is_array() const
    {
        return array_dims != 0;
    }

    /** Return whether this is a void type. */
    bool is_void() const
    {
        return ty == type_class::void_;
    }

    /** Return whether this is a null type. */
    bool is_null() const
    {
        return ty == type_class::null;
    }

    /** Return whether this is a struct type. */
    bool is_struct() const
    {
        return ty == type_class::struct_;
    }

    /** Whether this is a reference type (i.e., either an array or a struct). */
    bool is_reference() const
    {
        return is_array() || is_struct();
    }

    /** Whether this type is imported. */
    bool is_import() const
    {
        return import_path.has_value();
    }

    /** Return the array dimensions (0 for scalar types). */
    std::size_t get_array_dims() const
    {
        return array_dims;
    }

    /**
     * Return a dereferenced array type.
     *
     * @throws Throws a `codegen_error` if the type is not an array.
     */
    type deref() const
    {
        if(!is_array())
        {
            throw codegen_error("Tried to dereference a scalar type.");
        }

        return {ty, array_dims - 1, struct_name, import_path};
    }

    /** Get the base type. */
    type base_type() const
    {
        return {ty, 0, struct_name, import_path};
    }

    /** Return the struct's name, or `std::nullopt` if not a struct. */
    std::optional<std::string> get_struct_name() const
    {
        return struct_name;
    }

    /** Return the type's import path for external types, or `std::nullopt`. */
    std::optional<std::string> get_import_path() const
    {
        return import_path;
    }

    /** Get a readable string representation of the type. */
    std::string to_string() const;
};

/** A value. Values can be generated by evaluating an expression. */
class value
{
    /** The value's type */
    type ty;

    /** An optional name for the value. */
    std::optional<std::string> name;

protected:
    /**
     * Validate the value.
     *
     * @throws A `codegen_error` if the type is invalid.
     */
    void validate() const;

public:
    /** Default constructors. */
    value() = delete;
    value(const value&) = default;
    value(value&&) = default;

    /** Default assignments. */
    value& operator=(const value&) = default;
    value& operator=(value&&) = default;

    /**
     * Construct a value.
     *
     * @param type The value type.
     * @param name Optional name for this value.
     */
    value(type ty, std::optional<std::string> name = std::nullopt)
    : ty{std::move(ty)}
    , name{std::move(name)}
    {
        validate();
    }

    /**
     * Copy the type into a new value.
     */
    value copy_type() const
    {
        return {ty, std::nullopt};
    }

    /**
     * Return the de-referenced type.
     *
     * @throws Throws a `codegen_error` if the value is not an array.
     */
    value deref() const
    {
        return {ty.deref()};
    }

    /**
     * Get the value as a readable string.
     */
    std::string to_string() const;

    /** Get the value's type. */
    type get_type() const
    {
        return ty;
    }

    /** Set the value's name. */
    void set_name(std::string name)
    {
        this->name = std::move(name);
    }

    /** Get the value's name. */
    std::optional<std::string> get_name() const
    {
        return name;
    }

    /** Delete the value's name. */
    void delete_name()
    {
        name = std::nullopt;
    }

    /** Return whether the value has a name. */
    bool has_name() const
    {
        return name != std::nullopt;
    }
};

/** A constant integer value. */
class constant_int : public value
{
    /** The integer. */
    int i;

public:
    /** Deleted and defaulted constructors. */
    constant_int() = delete;
    constant_int(const constant_int&) = default;
    constant_int(constant_int&&) = default;

    /** Default assignments. */
    constant_int& operator=(const constant_int&) = default;
    constant_int& operator=(constant_int&&) = default;

    /**
     * Construct a constant integer.
     *
     * @param i The integer.
     * @param name An optional name.
     */
    constant_int(int i, std::optional<std::string> name = std::nullopt)
    : value{type{type_class::i32, 0}, std::move(name)}
    , i{i}
    {
    }

    /** Get the integer. */
    int get_int() const
    {
        return i;
    }
};

/** A constant floating point value. */
class constant_float : public value
{
    /** The floating point value. */
    float f;

public:
    /** Default constructors. */
    constant_float() = delete;
    constant_float(const constant_float&) = default;
    constant_float(constant_float&&) = default;

    /** Default assignments. */
    constant_float& operator=(const constant_float&) = default;
    constant_float& operator=(constant_float&&) = default;

    /**
     * Construct a constant floating point value.
     *
     * @param f The floating point value.
     * @param name An optional name.
     */
    constant_float(float f, std::optional<std::string> name = std::nullopt)
    : value{type{type_class::f32, 0}, std::move(name)}
    , f{f}
    {
    }

    /** Get the floating point value. */
    float get_float() const
    {
        return f;
    }
};

/** A constant string value. */
class constant_str : public value
{
    /** The string. */
    std::string s;

    /** Index into the constant table. */
    int constant_index{-1};

public:
    /** Default constructors. */
    constant_str() = delete;
    constant_str(const constant_str&) = default;
    constant_str(constant_str&&) = default;

    /** Default assignments. */
    constant_str& operator=(const constant_str&) = default;
    constant_str& operator=(constant_str&&) = default;

    /**
     * Construct a constant string value.
     *
     * @param s The string.
     * @param name An optional name.
     */
    constant_str(std::string s, std::optional<std::string> name = std::nullopt)
    : value{type{type_class::str, 0}, std::move(name)}
    , s{std::move(s)}
    {
    }

    /** Get the string value. */
    std::string get_str() const
    {
        return s;
    }

    /**
     * Set the index into the constant table.
     *
     * @param index The new index. A index of -1 means "no index".
     *              Must be non-negative or -1.
     */
    void set_constant_index(int index)
    {
        if(index < -1)
        {
            throw codegen_error(fmt::format("String index must be non-negative or -1. Got {}.", index));
        }

        constant_index = index;
    }

    /** Get the index into the constant table. A value of -1 indicates "no index". */
    int get_constant_index() const
    {
        return constant_index;
    }
};

/**
 * An instruction argument.
 */
class argument
{
public:
    /** Default constructors. */
    argument() = default;
    argument(const argument&) = default;
    argument(argument&&) = default;

    /** Destructor. */
    virtual ~argument() = default;

    /** Default assignment. */
    argument& operator=(const argument&) = default;
    argument& operator=(argument&&) = default;

    /** Register this argument in the constant table, if necessary. */
    virtual void register_const([[maybe_unused]] class context& ctx)
    {
    }

    /** Get a string representation of the argument. */
    virtual std::string to_string() const = 0;

    /** Get the argument type. */
    virtual const value* get_value() const = 0;
};

/**
 * A constant instruction argument.
 */
class const_argument : public argument
{
    /** The constant type. */
    std::unique_ptr<value> type;

public:
    /** Default and delered constructors. */
    const_argument() = default;
    const_argument(const const_argument&) = delete;
    const_argument(const_argument&&) = default;

    /** Default and deleted assignments. */
    const_argument& operator=(const const_argument&) = delete;
    const_argument& operator=(const_argument&&) = default;

    /**
     * Create an integer argument.
     *
     * @param i The constant integer.
     */
    explicit const_argument(int i)
    : argument()
    , type{std::make_unique<constant_int>(i)}
    {
    }

    /**
     * Create a floating-point argument.
     *
     * @param f The floating-point value.
     */
    const_argument(float f)
    : argument()
    , type{std::make_unique<constant_float>(f)}
    {
    }

    /**
     * Create a string argument.
     *
     * @note A string needs to be registered with a context using `register_const`.
     *
     * @param s The string.
     */
    const_argument(std::string s)
    : argument()
    , type{std::make_unique<constant_str>(s)}
    {
    }

    void register_const(class context& ctx) override;
    std::string to_string() const override;

    const value* get_value() const override
    {
        return type.get();
    }
};

/**
 * Function argument.
 */
class function_argument : public argument
{
    /** The function's name. */
    std::unique_ptr<value> name;

    /** An optional import path for the function. */
    std::optional<std::string> import_path = std::nullopt;

public:
    /** Defaulted and deleted constructors. */
    function_argument() = default;
    function_argument(const function_argument&) = delete;
    function_argument(function_argument&&) = default;

    /** Defaulted and deleted assignments. */
    function_argument& operator=(const function_argument&) = delete;
    function_argument& operator=(function_argument&&) = default;

    /**
     * Create a function argument.
     *
     * @param name The function name.
     * @param import_path The import path of the function, or `std::nullopt`.
     */
    function_argument(std::string name, std::optional<std::string> import_path)
    : argument()
    , name{std::make_unique<value>(type{type_class::fn, 0}, std::move(name))}
    , import_path{std::move(import_path)}
    {
    }

    /**
     * Set the import path.
     *
     * @param path The import path, or `std::nullopt`.
     */
    void set_import_path(std::optional<std::string> import_path)
    {
        this->import_path = std::move(import_path);
    }

    /** Get the import path. */
    const std::optional<std::string>& get_import_path() const
    {
        return import_path;
    }

    std::string to_string() const override
    {
        if(import_path.has_value())
        {
            return fmt::format("@{}::{}", *import_path, *name->get_name());
        }

        return fmt::format("@{}", *name->get_name());
    }

    const value* get_value() const override
    {
        return name.get();
    }
};

/**
 * Type argument.
 */
class type_argument : public argument
{
    /** The type. */
    value vt;

    /** An optional import path for the function. */
    std::optional<std::string> import_path = std::nullopt;

public:
    /** Defaulted and deleted constructors. */
    type_argument() = delete;
    type_argument(const type_argument&) = default;
    type_argument(type_argument&&) = default;

    /** Default assignments. */
    type_argument& operator=(const type_argument&) = default;
    type_argument& operator=(type_argument&&) = default;

    /**
     * Create a type argument.
     *
     * @param v Value containing the type information to use.
     */
    explicit type_argument(value vt)
    : argument()
    , vt{vt.copy_type()}
    {
    }

    /**
     * Set the import path.
     *
     * @param path The import path, or `std::nullopt`.
     */
    void set_import_path(std::optional<std::string> import_path)
    {
        this->import_path = std::move(import_path);
    }

    /** Get the import path. */
    const std::optional<std::string>& get_import_path() const
    {
        return import_path;
    }

    std::string to_string() const override
    {
        if(import_path.has_value())
        {
            return fmt::format("{}::{}", *import_path, vt.to_string());
        }

        return vt.to_string();
    }

    const value* get_value() const override
    {
        return &vt;
    }
};

/**
 * A variable instruction argument.
 */
class variable_argument : public argument
{
    /** The variable. */
    std::unique_ptr<value> var;

public:
    /** Defaulted and deleted constructors. */
    variable_argument() = delete;
    variable_argument(const variable_argument&) = delete;
    variable_argument(variable_argument&&) = default;

    /** Default assignments. */
    variable_argument& operator=(const variable_argument&) = delete;
    variable_argument& operator=(variable_argument&&) = default;

    /**
     * Create a variable argument.
     *
     * @param v The variable.
     */
    explicit variable_argument(std::unique_ptr<value> v)
    : argument()
    , var{std::move(v)}
    {
    }

    std::string to_string() const override
    {
        return fmt::format("{}", var->to_string());
    }

    const value* get_value() const override
    {
        return var.get();
    }
};

/**
 * A label argument for jump instructions.
 */
class label_argument : public argument
{
    /** The label. */
    std::string label;

public:
    /** Default constructors. */
    label_argument() = default;
    label_argument(const label_argument&) = default;
    label_argument(label_argument&&) = default;

    /** Default assignments. */
    label_argument& operator=(const label_argument&) = default;
    label_argument& operator=(label_argument&&) = default;

    /**
     * Construct a label.
     *
     * @param label The label.
     */
    explicit label_argument(std::string label)
    : argument()
    , label{std::move(label)}
    {
    }

    /** Return the label. */
    const std::string& get_label() const
    {
        return label;
    }

    std::string to_string() const override
    {
        return fmt::format("%{}", label);
    }

    const value* get_value() const override
    {
        throw codegen_error(fmt::format("Cannot get type from label '{}'.", to_string()));
    }
};

/** Type casts. */
enum class type_cast
{
    i32_to_f32, /* i32 to f32 */
    f32_to_i32, /* f32 to i32 */
};

/**
 * Return a string representation of the type cast.
 *
 * @param tc The type cast.
 * @returns A string representation of the type cast.
 */
std::string to_string(type_cast tc);

/**
 * A type cast argument.
 */
class cast_argument : public argument
{
    /** The type cast. */
    type_cast cast;

    /** Value of the cast. */
    std::unique_ptr<value> v;

public:
    /** Default constructors. */
    cast_argument() = default;
    cast_argument(const cast_argument&) = delete;
    cast_argument(cast_argument&&) = default;

    /** Default assignments. */
    cast_argument& operator=(const cast_argument&) = delete;
    cast_argument& operator=(cast_argument&&) = default;

    /**
     * Construct a type cast.
     *
     * @param cast The cast type.
     */
    explicit cast_argument(type_cast cast)
    : argument()
    , cast{cast}
    {
        if(cast == type_cast::i32_to_f32)
        {
            v = std::make_unique<value>(type{type_class::f32, 0});
        }
        else if(cast == type_cast::f32_to_i32)
        {
            v = std::make_unique<value>(type{type_class::i32, 0});
        }
        else
        {
            throw codegen_error("Unknown cast type.");
        }
    }

    /** Return the cast type. */
    type_cast get_cast() const
    {
        return cast;
    }

    std::string to_string() const override
    {
        return fmt::format("{}", ::slang::codegen::to_string(cast));
    }

    const value* get_value() const override
    {
        return v.get();
    }
};

/**
 * Field access argument.
 */
class field_access_argument : public argument
{
    /** The struct information. */
    type struct_type;

    /** The field's member. */
    value member;

public:
    /** Defaulted and deleted constructors. */
    field_access_argument() = delete;
    field_access_argument(const field_access_argument&) = delete;
    field_access_argument(field_access_argument&&) = default;

    /** Default assignments. */
    field_access_argument& operator=(const field_access_argument&) = delete;
    field_access_argument& operator=(field_access_argument&&) = default;

    /**
     * Construct a field access.
     *
     * @param struct_type Type information for the struct.
     * @param member_type The field's accessed member.
     */
    field_access_argument(type struct_type, value member)
    : argument()
    , struct_type{std::move(struct_type)}
    , member{std::move(member)}
    {
    }

    /** Return the struct name. */
    std::string get_struct_name() const
    {
        return *struct_type.get_struct_name();
    }

    /** Return the struct's import path. */
    std::optional<std::string> get_import_path() const
    {
        return struct_type.get_import_path();
    }

    /** Return the field's member. */
    value get_member() const
    {
        return member;
    }

    std::string to_string() const override
    {
        return fmt::format("%{}, {}", get_struct_name(), member.to_string());
    }

    const value* get_value() const override
    {
        return nullptr;
    }
};

/**
 * Instruction base class.
 */
class instruction
{
    /** The instruction name. */
    std::string name;

    /** The instruction's arguments. */
    std::vector<std::unique_ptr<argument>> args;

public:
    /** Default constructors. */
    instruction() = default;
    instruction(const instruction&) = default;
    instruction(instruction&&) = default;

    /** Destructor. */
    virtual ~instruction() = default;

    /** Default assignments. */
    instruction& operator=(const instruction&) = default;
    instruction& operator=(instruction&&) = default;

    /**
     * Construct an instruction without arguments.
     *
     * @param name The instruction's opcode name.
     */
    explicit instruction(std::string name)
    : name{std::move(name)}
    {
    }

    /**
     * Construct an instruction with arguments.
     *
     * @param name The instruction's opcode name.
     * @param args The instruction's arguments.
     */
    instruction(std::string name, std::vector<std::unique_ptr<argument>> args)
    : name{std::move(name)}
    , args{std::move(args)}
    {
    }

    /** Returns whether the instruction is branching. */
    virtual bool is_branching() const
    {
        return name == "jmp" || name == "jnz";
    }

    /** Returns whether the instruction is a return instruction. */
    virtual bool is_return() const
    {
        return name == "ret";
    }

    /** Get the instruction name. */
    const std::string& get_name() const
    {
        return name;
    }

    /** Get the instruction's arguments. */
    const std::vector<std::unique_ptr<argument>>& get_args() const
    {
        return args;
    }

    /** Get instruction representation as string. */
    std::string to_string() const;
};

/**
 * A block that has a single named entry point, a single exit point and
 * no branching.
 */
class basic_block
{
    friend class context;

    /** The block's entry label. */
    std::string label;

    /** The instructions. */
    std::vector<std::unique_ptr<instruction>> instrs;

    /** The associated inserting context (if any). */
    class context* inserting_context{nullptr};

    /** Whether the block is marked as unreachable. */
    bool unreachable = false;

    /**
     * Set a new context for inserting instructions.
     * Pass nullptr to clear the context.
     *
     * @param ctx The new context or nullptr.
     */
    void set_inserting_context(class context* ctx);

    /**
     * Create a basic_block.
     *
     * @param label The block's label.
     */
    explicit basic_block(std::string label)
    : label{std::move(label)}
    {
    }

public:
    /** Default constructors. */
    basic_block() = delete;
    basic_block(const basic_block&) = delete;
    basic_block(basic_block&&) = delete;

    /** Destructor. */
    virtual ~basic_block()
    {
        // clear references to this block.
        set_inserting_context(nullptr);
    }

    /** Default assignments. */
    basic_block& operator=(const basic_block&) = delete;
    basic_block& operator=(basic_block&&) = delete;

    /**
     * Add a non-branching instruction.
     *
     * Throws a `codegen_error` if the instruction is branching.
     *
     * @param instr The instruction.
     */
    void add_instruction(std::unique_ptr<instruction> instr)
    {
        instrs.emplace_back(std::move(instr));
    }

    /** Get string representation of block. */
    std::string to_string() const;

    /** Get the inserting context. May return nullptr. */
    class context* get_inserting_context() const
    {
        return inserting_context;
    }

    /** Get the block label. */
    std::string get_label() const
    {
        return label;
    }

    /** Return whether this block ends with a return statement. */
    bool ends_with_return() const
    {
        return instrs.size() > 0 && instrs.back()->is_return();
    }

    /** Return whether this block ends with a branch statement. */
    bool ends_with_branch() const
    {
        return instrs.size() > 0 && instrs.back()->is_branching();
    }

    /** Mark the block as unreachable. */
    void set_unreachable()
    {
        unreachable = true;
    }

    /**
     * Check whether this block is unreachable.
     *
     * Currently this is done by checking if the block is explicitly marked as unreachable.
     */
    bool is_unreachable() const
    {
        return unreachable;
    }

    /**
     * Return whether this block is valid.
     *
     * A block is valid if it contains a single return or branch instruction,
     * located at its end.
     */
    bool is_valid() const;

    /** Get the block's instructions. */
    const std::vector<std::unique_ptr<instruction>>& get_instructions() const
    {
        return instrs;
    }

    /** Get the block's instructions. */
    std::vector<std::unique_ptr<instruction>>& get_instructions()
    {
        return instrs;
    }

    /**
     * Create a `basic_block`.
     *
     * @param ctx The context for the new block.
     * @param name The block name.
     * @return Returns a new `basic_block`.
     */
    static basic_block* create(class context& ctx, std::string name);
};

/** A user-defined type. */
class struct_
{
    /** The type's name. */
    std::string name;

    /** The type's members as pairs `(name, type)`. */
    std::vector<std::pair<std::string, value>> members;

    /** Flags. */
    std::uint8_t flags = 0;

    /** The module path for imported types and `std::nullopt` for types within the current module. */
    std::optional<std::string> import_path;

public:
    /** Default constructors. */
    struct_() = default;
    struct_(const struct_&) = default;
    struct_(struct_&&) = default;

    /** Destructor. */
    virtual ~struct_() = default;

    /** Default assignment. */
    struct_& operator=(const struct_&) = default;
    struct_& operator=(struct_&&) = default;

    /**
     * Construct a new struct.
     *
     * @param name The type's name.
     * @param members The type's members.
     * @param flags THe type's flags.
     * @param import_path The import path of the module for imported types.
     */
    struct_(std::string name,
            std::vector<std::pair<std::string, value>> members,
            std::uint8_t flags = 0,
            std::optional<std::string> import_path = std::nullopt)
    : name{std::move(name)}
    , members{std::move(members)}
    , flags{flags}
    , import_path{std::move(import_path)}
    {
    }

    /** Get the type's name. */
    std::string get_name() const
    {
        return name;
    }

    /** Get the type's members. */
    const std::vector<std::pair<std::string, value>>& get_members() const
    {
        return members;
    }

    /** Get the type's flags. */
    std::uint8_t get_flags() const
    {
        return flags;
    }

    /** Return whether this is an imported type. */
    bool is_import() const
    {
        return import_path.has_value();
    }

    /** Return the import path. */
    const std::optional<std::string>& get_import_path() const
    {
        return import_path;
    }

    /** String representation of a type. */
    std::string to_string() const;
};

/**
 * A scope has a name and holds variables.
 */
class scope
{
    /** The scope's name. */
    std::string name;

    /** Reference to the outer scope (if any). */
    scope* outer{nullptr};

    /** Arguments for function scopes. */
    std::vector<std::unique_ptr<value>> args;

    /** Variables inside the scope. */
    std::vector<std::unique_ptr<value>> locals;

    /** Structs. */
    std::unordered_map<std::string, struct_> structs;

public:
    /** Constructors. */
    scope() = default;
    scope(const scope&) = delete;
    scope(scope&&) = default;

    /** Destructor. */
    virtual ~scope() = default;

    /** Assignments. */
    scope& operator=(const scope&) = delete;
    scope& operator=(scope&&) = default;

    /**
     * Create a scope from a name (e.g. "<global>").
     *
     * @param name The scope's name.
     */
    explicit scope(std::string name)
    : name{std::move(name)}
    {
    }

    /**
     * Create a scope and initialize it with function arguments.
     *
     * @param name The scope's name (usually the same as the function's name)
     * @param args The function's arguments.
     */
    scope(std::string name, std::vector<std::unique_ptr<value>> args)
    : name{std::move(name)}
    , args{std::move(args)}
    {
    }

    /**
     * Check if the name is already contained in this scope as an argument or a local variable.
     *
     * @returns True if the name exists.
     * @throws Throws a `codegen_error` if an unnamed value is found within the scope.
     */
    bool contains(const std::string& name) const;

    /**
     * Check if the scope contains a struct.
     *
     * @param name The struct's name.
     * @param import_path An optional import path.
     * @returns True if the struct exists.
     */
    bool contains_struct(const std::string& name, const std::optional<std::string>& import_path = std::nullopt) const;

    /**
     * Get the variable for the given name.
     *
     * @param name The variable's name.
     * @returns The variable or nullptr.
     * @throws Throws a `codegen_error` if an unnamed value is found within the scope.
     */
    value* get_value(const std::string& name);

    /**
     * Get the index on argument or a local. Indices are with respect to
     * the list `[arg1, ... argN, local1, ... localM]`.
     *
     * Indices are not constant during code generation. They are constant
     * during instruction emission.
     *
     * @param name Name or the local or the argument.
     * @throws A `codegen_error` the the name could not be found.
     */
    std::size_t get_index(const std::string& name) const;

    /**
     * Add an argument.
     *
     * @param arg The argument.
     * @throws Throws a `codegen_error` if the supplied argument has no name or if the
     *         scope already has an object with the same name.
     */
    void add_argument(std::unique_ptr<value> arg);

    /**
     * Add a local variable.
     *
     * @param arg The variable.
     * @throws Throws a `codegen_error` if the supplied local has no name or if the
     *         scope already has an object with the same name.
     */
    void add_local(std::unique_ptr<value> arg);

    /**
     * Add a struct to the scope.
     *
     * @param name The struct's name.
     * @param members The struct's members.
     * @param import_path Optional import path.
     * @param flags The struct's flags.
     * @throws Throws a `codegen_error` if the name is already registered as a type in this scope.
     */
    void add_struct(std::string name,
                    std::vector<std::pair<std::string, value>> members,
                    std::uint8_t flags,
                    std::optional<std::string> import_path = std::nullopt);

    /** Get the arguments for this scope. */
    const std::vector<std::unique_ptr<value>>& get_args() const
    {
        return args;
    }

    /** Get the locals for this scope. */
    const std::vector<std::unique_ptr<value>>& get_locals() const
    {
        return locals;
    }

    /**
     * Get the struct in this scope.
     *
     * @param name The struct's name.
     * @param import_path Optional import path of the struct. If set to `std::nullopt`, only structs within
     *                    the current module are searched.
     * @returns Returns the struct definition.
     * @throws Throws a `codegen_error` if the struct is not found.
     */
    const std::vector<std::pair<std::string, value>>&
      get_struct(const std::string& name,
                 std::optional<std::string> import_path = std::nullopt) const;

    /** Get the outer scope. */
    scope* get_outer()
    {
        return outer;
    }

    /** Get the outer scope. */
    const scope* get_outer() const
    {
        return outer;
    }

    /** Get a string representation of the scope. */
    std::string to_string() const
    {
        if(outer)
        {
            return fmt::format("{}::{}", outer->to_string(), name);
        }

        return name;
    }
};

/**
 * A scope guard that automatically gets called when the scope is exited.
 */
class scope_guard
{
    /** The associated context. */
    context& ctx;

    /** The scope. */
    scope* s;

public:
    /** No default constructor. */
    scope_guard() = delete;

    /** Deleted copy and move constructors. */
    scope_guard(const scope_guard&) = delete;
    scope_guard(scope_guard&&) = delete;

    /**
     * Construct a scope guard.
     *
     * @param ctx The associated context.
     * @param s The scope.
     */
    scope_guard(context& ctx, scope* s);

    /** Destructor. */
    ~scope_guard();

    /** Deleted assignments.*/
    scope_guard& operator=(const scope_guard&) = delete;
    scope_guard& operator=(scope_guard&&) = delete;
};

/**
 * A guard that signals function entry and exit.
 */
class function_guard
{
    /** The associated context. */
    context& ctx;

public:
    /** No default constructor. */
    function_guard() = delete;

    /** Deleted copy and move constructors. */
    function_guard(const function_guard&) = delete;
    function_guard(function_guard&&) = delete;

    /**
     * Construct a scope guard.
     *
     * @param ctx The associated context.
     * @param s The scope.
     */
    function_guard(context& ctx, class function* fn);

    /** Destructor. */
    ~function_guard();

    /** Deleted assignments.*/
    function_guard& operator=(const function_guard&) = delete;
    function_guard& operator=(function_guard&&) = delete;
};

/**
 * Function prototype information.
 */
class prototype
{
    /** The function's name. */
    std::string name;

    /** The return type of the function. */
    value return_type;

    /** The argument types. */
    std::vector<value> arg_types;

    /** The module path for imported functions and `std::nullopt` for prototypes within the current module. */
    std::optional<std::string> import_path;

public:
    /** Defaulted and deleted constructors. */
    prototype() = delete;
    prototype(const prototype&) = default;
    prototype(prototype&&) = default;

    /** Default assignments. */
    prototype& operator=(const prototype&) = default;
    prototype& operator=(prototype&&) = default;

    /**
     * Construct a function prototype.
     *
     * @param name The function's name.
     * @param return_type The function's return type.
     * @param arg_type The function's argument types.
     * @param import_path The import path of the module for imported functions.
     */
    prototype(std::string name, value return_type, std::vector<value> arg_types, std::optional<std::string> import_path = std::nullopt)
    : name{std::move(name)}
    , return_type{std::move(return_type)}
    , arg_types{std::move(arg_types)}
    , import_path{std::move(import_path)}
    {
    }

    /** Get the function's name. */
    const std::string& get_name() const
    {
        return name;
    }

    /** Get the function's return type. */
    const value& get_return_type() const
    {
        return return_type;
    }

    /** Get the function's argument types. */
    const std::vector<value>& get_arg_types() const
    {
        return arg_types;
    }

    /** Return whether this is an imported function. */
    bool is_import() const
    {
        return import_path.has_value();
    }

    /** Return the import path. */
    const std::optional<std::string>& get_import_path() const
    {
        return import_path;
    }
};

/**
 * A relocatable function.
 */
class function
{
    /** The function's name. */
    std::string name;

    /** Whether this is a native function. */
    bool native;

    /** Import library name for native functions. */
    std::string import_library;

    /** The function's return type. */
    std::unique_ptr<value> return_type;

    /** The function variable scope. */
    slang::codegen::scope scope;

    /** Function instructions. */
    std::list<basic_block*> instr_blocks;

public:
    /** Constructors. */
    function() = delete;
    function(const function&) = delete;
    function(function&&) = default;

    /**
     * Construct a function from name, return type and argument list.
     *
     * @param name The function's name.
     * @param return_type The function's return type.
     * @param args The function's argument list.
     */
    function(std::string name, std::unique_ptr<value> return_type, std::vector<std::unique_ptr<value>> args)
    : name{name}
    , native{false}
    , return_type{std::move(return_type)}
    , scope{std::move(name), std::move(args)}
    {
    }

    /**
     * Construct a native function from import library name and function name.
     *
     * @param import_library The import library's name.
     * @param name The function's name.
     * @param return_type The function's return type.
     * @param args The function's argument list.
     */
    function(std::string import_library,
             std::string name,
             std::unique_ptr<value> return_type,
             std::vector<std::unique_ptr<value>> args)
    : name{name}
    , native{true}
    , import_library{import_library}
    , return_type{std::move(return_type)}
    , scope{std::move(name), std::move(args)}
    {
    }

    /** Destructor. */
    ~function() = default;

    /** Assignments. */
    function& operator=(const function&) = delete;
    function& operator=(function&&) = delete;

    /** Get the function's name. */
    const std::string& get_name() const
    {
        return name;
    }

    /**
     * Append an instruction block.
     *
     * @param block The instruction block to append.
     */
    void append_basic_block(basic_block* block)
    {
        instr_blocks.push_back(block);
    }

    /** Return whether the function ends with a return statement. */
    bool ends_with_return() const
    {
        return instr_blocks.size() > 0 && instr_blocks.back()->ends_with_return();
    }

    /** Create a local variable. */
    void create_local(std::unique_ptr<value> v)
    {
        scope.add_local(std::move(v));
    }

    /** Get the function's scope. */
    class scope* get_scope()
    {
        return &scope;
    }

    /** Get the function's scope. */
    const class scope* get_scope() const
    {
        return &scope;
    }

    /** Returns the function's signature as a pair `(return_type, arg_types)`. */
    std::pair<type, std::vector<type>> get_signature() const
    {
        std::vector<type> arg_types;
        auto& args = scope.get_args();
        std::transform(args.cbegin(), args.cend(), std::back_inserter(arg_types),
                       [](const auto& arg) -> type
                       {
                           return arg->get_type();
                       });
        return {return_type->get_type(), std::move(arg_types)};
    }

    /** Return whether this is a native function. */
    bool is_native() const
    {
        return native;
    }

    /**
     * Return the import library for a native function.
     *
     * @throws Throws a `codegen_error` if this is not a native function.
     *
     * @returns The import library name.
     */
    std::string get_import_library() const
    {
        if(!native)
        {
            throw codegen_error(fmt::format("Cannot get import library for function '{}', as it was not declared as native.", get_name()));
        }

        return import_library;
    }

    /**
     * Return the basic blocks.
     *
     * @returns The function's basic blocks.
     */
    const std::list<basic_block*>& get_basic_blocks() const
    {
        return instr_blocks;
    }

    /** String representation of function. */
    std::string to_string() const;
};

/**
 * A binary operation. Reads two alike values from the stack and puts a value of the same type onto the stack.
 */
enum class binary_op
{
    op_mul,           /** a * b */
    op_div,           /** a / b */
    op_mod,           /** a % b */
    op_add,           /** a + b */
    op_sub,           /** a - b */
    op_shl,           /** a << b */
    op_shr,           /** a >> b */
    op_less,          /** a < b */
    op_less_equal,    /** a <= b */
    op_greater,       /** a > b */
    op_greater_equal, /** a >= b */
    op_equal,         /** a == b */
    op_not_equal,     /** a != b */
    op_and,           /** a & b */
    op_xor,           /** a ^ b */
    op_or,            /** a | b */
    op_logical_and,   /** a && b */
    op_logical_or,    /** a || b */
};

/**
 * Return a string representation of a binary operator.
 *
 * @param op The binary operator.
 * @returns A string representation of the operator.
 */
std::string to_string(binary_op op);

/** An imported symbol. */
struct imported_symbol
{
    /** Symbol type. */
    module_::symbol_type type;

    /** Symbol name. */
    std::string name;

    /** The import path of the module. */
    std::string import_path;

    /** Default constructors. */
    imported_symbol() = default;
    imported_symbol(const imported_symbol&) = default;
    imported_symbol(imported_symbol&&) = default;

    /** Default assignments. */
    imported_symbol& operator=(const imported_symbol&) = default;
    imported_symbol& operator=(imported_symbol&&) = default;

    /**
     * Construct an `imported_symbol`.
     *
     * @param type The symbol's type.
     * @param name The symbol's name.
     * @param import_path Path of the module the symbol is imported from.
     */
    imported_symbol(module_::symbol_type type, std::string name, std::string import_path)
    : type{type}
    , name{std::move(name)}
    , import_path{std::move(import_path)}
    {
    }
};

/** Constant table entry. */
struct constant_table_entry : public module_::constant_table_entry
{
    /** Optional import path. */
    std::optional<std::string> import_path = std::nullopt;

    /** Optional name. */
    std::optional<std::string> name = std::nullopt;

    /** Whether to export the constant. */
    bool add_to_exports = false;

    /**
     * Construct a constant table entry.
     *
     * @throws Throws a `codegen_error` if `add_to_exports` is `true` and no `name` was set.
     *
     * @param type The constant type.
     * @param value The constant's value.
     * @param import_path Optional import path.
     * @param name Optional name for the constant.
     * @param add_to_exports Whether to export the constant. Defaults to `false`.
     */
    template<typename T>
    constant_table_entry(
      module_::constant_type type,
      T value,
      std::optional<std::string> import_path = std::nullopt,
      std::optional<std::string> name = std::nullopt,
      bool add_to_exports = false)
    : module_::constant_table_entry{type, std::move(value)}
    , import_path{std::move(import_path)}
    , name{std::move(name)}
    , add_to_exports{add_to_exports}
    {
        if(add_to_exports && !this->name.has_value())
        {
            throw codegen_error("Cannot export constant without a name.");
        }
    }
};

/** Code generator context. */
class context
{
    friend class slang::instruction_emitter;
    friend class slang::export_table_builder;
    friend class basic_block;

    /** List of structs. */
    std::vector<std::unique_ptr<struct_>> types;

    /** Constant table. */
    std::vector<constant_table_entry> constants;

    /** Imported constants. */
    std::vector<constant_table_entry> imported_constants;

    /** Global scope. */
    std::unique_ptr<scope> global_scope{std::make_unique<scope>("<global>")};

    /** The current scope stack. */
    std::vector<scope*> current_scopes;

    /** Scope name stack for name resolution. */
    std::vector<std::string> resolution_scopes;

    /** Struct stack for access resolution. */
    std::vector<type> struct_access;

    /** Directive stack. */
    std::vector<directive> directive_stack;

    /** List of function prototypes. */
    std::vector<std::unique_ptr<prototype>> prototypes;

    /** List of functions. */
    std::vector<std::unique_ptr<function>> funcs;

    /** The currently compiled function, or `nullptr`. */
    function* current_function{nullptr};

    /** The basic blocks for `break` and `continue` statements. */
    std::vector<std::pair<basic_block*, basic_block*>> basic_block_brk_cnt;

    /** List of basic blocks. */
    std::vector<std::unique_ptr<basic_block>> basic_blocks;

    /** Imported symbols. */
    std::vector<imported_symbol> imports;

    /** A label counter for unique label generation. */
    std::size_t label_count = 0;

    /** Current instruction insertion point. */
    basic_block* insertion_point{nullptr};

    /** Holds the array type when declaring an array. */
    std::optional<value> array_type = std::nullopt;

    /** Expressions that are known to be compile-time constant. */
    std::unordered_map<const ast::expression*, bool> constant_expressions;

    /** Expressions with value known at compile-time. */
    std::unordered_map<const ast::expression*, std::unique_ptr<value>> expression_values;

protected:
    /**
     * Check that the insertion point is not null.
     *
     * @throws Throws a `codegen_error` if the insertion point is null.
     */
    void validate_insertion_point() const
    {
        if(!insertion_point)
        {
            throw codegen_error("Invalid insertion point (nullptr).");
        }
    }

public:
    /** Whether to evaluate constant subexpressions during code generation. */
    bool evaluate_constant_subexpressions{true};

public:
    /** Constructors. */
    context() = default;
    context(const context&) = delete;
    context(context&&) = default;

    /** Destructor. */
    virtual ~context()
    {
        set_insertion_point(nullptr);
    }

    /** Assignments. */
    context& operator=(const context&) = delete;
    context& operator=(context&&) = default;

    /**
     * Add a symbol to the import table. No-op if the symbol already exists.
     *
     * @param type The symbol type.
     * @param import_path Path of the module that exports the symbol.
     * @param name The symbol's name.
     * @throws Throws a `codegen_error` if the symbol already exists but the symbol type does not match.
     */
    void add_import(
      module_::symbol_type type,
      std::string import_path,
      std::string name);

    /**
     * Get the import index of a symbol. If the symbol is not found in the imports,
     * a `codegen_error` is thrown.
     *
     * @param type The symbol type.
     * @param import_path Path of the module that exports the symbol.
     * @param name The symbol's name.
     * @return The symbol's index in the import table.
     * @throws Throws a `codegen_error` if the symbol is not found.
     */
    std::size_t get_import_index(
      module_::symbol_type type,
      std::string import_path,
      std::string name) const;

    /**
     * Create a struct.
     *
     * Throws a `codegen_error` if the struct already exists or if it contains undefined types.
     *
     * @param name The struct's name.
     * @param members The struct's members as pairs `(name, type)`.
     * @param flags Struct flags.
     * @param import_path An optional import path for the struct.
     * @returns A representation of the created struct.
     */
    struct_* add_struct(
      std::string name,
      std::vector<std::pair<std::string, value>> members,
      std::uint8_t flags = 0,
      std::optional<std::string> import_path = std::nullopt);

    /**
     * Get a struct definition.
     *
     * Throws a `codegen_error` if the struct is unknown.
     *
     * @param name The struct name.
     * @param import_path Import path.
     */
    struct_* get_type(
      const std::string& name,
      std::optional<std::string> import_path = std::nullopt);

    /**
     * Add a `i32` constant to the constant table.
     *
     * @param name The constant's name.
     * @param i The constant's value.
     * @param import_path Import path.
     */
    void add_constant(
      std::string name,
      std::int32_t i,
      std::optional<std::string> import_path = std::nullopt);

    /**
     * Add a `f32` constant to the constant table.
     *
     * @param name The constant's name.
     * @param f The constant's value.
     * @param import_path Import path.
     */
    void add_constant(
      std::string name,
      float i,
      std::optional<std::string> import_path = std::nullopt);

    /**
     * Add a `str` constant to the constant table.
     *
     * @param name The constant's name.
     * @param s The constant's value.
     * @param import_path Import path.
     */
    void add_constant(
      std::string name,
      std::string s,
      std::optional<std::string> import_path = std::nullopt);

    /**
     * Get a reference to a string or create a new one if it does not exist.
     *
     * @param str The string.
     * @returns An index into the constant table.
     */
    std::size_t get_string(std::string str);

    /**
     * Get a constant from the constant table.
     *
     * @param name The constant's name.
     * @param import_path The constant's import path.
     * @returns Returns a `constant_table_entry` if the constant was found, and `std::nullopt` otherwise.
     */
    std::optional<constant_table_entry> get_constant(
      const std::string& name,
      const std::optional<std::string>& import_path = std::nullopt);

    /**
     * Add a function prototype.
     *
     * Throws a `codegen_error` if the prototype already exists.
     *
     * @param name The function's name.
     * @param return_type The function's return type.
     * @param args The function's arguments.
     * @param import_path The import path for the prototype.
     * @returns A representation of the prototype.
     */
    prototype* add_prototype(
      std::string name,
      value return_type,
      std::vector<value> args,
      std::optional<std::string> import_path = std::nullopt);

    /**
     * Get a function's prototype.
     *
     * Throws a `codegen_error` if the prototype is not found.
     *
     * @param name The function's name.
     * @param import_path The import path of the function, or `std::nullopt`.
     * @returns A reference the the function's prototype.
     */
    const prototype& get_prototype(
      const std::string& name,
      std::optional<std::string> import_path) const;

    /**
     * Add a function definition.
     *
     * Throws a `codegen_error` if the function name already exists.
     *
     * @param name The function's name.
     * @param return_type The function's return type.
     * @param args The function's arguments.
     * @returns A representation of the function.
     */
    function* create_function(
      std::string name,
      std::unique_ptr<value> return_type,
      std::vector<std::unique_ptr<value>> args);

    /**
     * Add a function with a native implementation in a library.
     *
     * Throws a `codegen_error` if the function name already exists.
     *
     * @param lib_name Name of the library the function should be imported from.
     * @param name The function's name.
     * @param return_type The function's return type.
     * @param args The function's arguments.
     */
    void create_native_function(
      std::string lib_name,
      std::string name,
      std::unique_ptr<value> return_type,
      std::vector<std::unique_ptr<value>> arg);

    /**
     * Set instruction insertion point.
     *
     * @param ip The insertion point for instructions.
     */
    void set_insertion_point(basic_block* ip);

    /**
     * Get the current insertion point.
     *
     * @param validate Whether to throw a `codegen_error` if the insertion point is `nullptr`.
     *                 Defaults to `false`.
     * @return Returns the current insertion point.
     */
    basic_block* get_insertion_point(bool validate = false) const
    {
        if(validate && insertion_point == nullptr)
        {
            throw codegen_error("Invalid insertion point.");
        }
        return insertion_point;
    }

    /**
     * Enter a new scope.
     *
     * @param s The new scope.
     */
    void enter_scope(scope* s)
    {
        current_scopes.push_back(s);
    }

    /**
     * Exit a scope.
     *
     * @param s The scope to leave. Has to be the last entered scope.
     */
    void exit_scope(scope* s)
    {
        if(current_scopes.size() == 0)
        {
            throw codegen_error("No scope to leave.");
        }

        if(current_scopes.back() != s)
        {
            throw codegen_error("Tried exiting wrong scope.");
        }

        current_scopes.pop_back();
    }

    /** Get the current scope. */
    scope* get_scope()
    {
        if(current_scopes.size() > 0)
        {
            return current_scopes.back();
        }

        return global_scope.get();
    }

    /** Get the global scope. */
    scope* get_global_scope()
    {
        return global_scope.get();
    }

    /** Get the global scope. */
    const scope* get_global_scope() const
    {
        return global_scope.get();
    }

    /** Return whether a given scope is the global scope. */
    bool is_global_scope(const scope* s) const
    {
        return s == global_scope.get();
    }

    /**
     * Push a name onto the struct access resolution stack.
     *
     * @param ty The struct.
     */
    void push_struct_access(type ty);

    /** Pop a name from the struct access resolution stack. */
    void pop_struct_access();

    /** Whether we are accessing a struct. */
    bool is_struct_access() const
    {
        return struct_access.size() != 0;
    }

    /**
     * Get the currently accessed struct.
     *
     * @throws Throws a `codegen_error` if no struct is accessed.
     */
    type get_accessed_struct() const;

    /**
     * Get a member of a struct by name.
     *
     * @param loc Source location of the access.
     * @param struct_name Name of the struct.
     * @param member_name Name of the accessed member.
     * @param import_path Import path of the struct. Optional for module-local structs.
     * @returns Returns a type describing the struct member.
     * @throws Throws a `codegen_error` if the struct or the requested member could not be found.
     */
    value get_struct_member(
      token_location loc,
      const std::string& struct_name,
      const std::string& member_name,
      std::optional<std::string> import_path = std::nullopt) const;

    /**
     * Push a directive onto the directive stack.
     *
     * @param dir The directive.
     */
    void push_directive(directive dir)
    {
        directive_stack.emplace_back(std::move(dir));
    }

    /** Pop a directive from the directive stack. */
    void pop_directive()
    {
        if(directive_stack.size() == 0)
        {
            throw codegen_error("No directive to pop.");
        }
        directive_stack.pop_back();
    }

    /** Get all directives. */
    const std::vector<directive>& get_directives() const
    {
        return directive_stack;
    }

    /** Get directives matching a given name. */
    std::vector<directive> get_directives(const std::string& name) const
    {
        std::vector<directive> directives;
        std::copy_if(
          directive_stack.begin(),
          directive_stack.end(),
          std::back_inserter(directives),
          [&name](const directive& dir)
          {
              return dir.name.s == name;
          });
        return directives;
    }

    /**
     * Check if the directive has the specified flag set, that is, it is
     * part of the directive's keys with a value of `true` or `false`. If
     * the flag is not found or the values are invalid, `default_value` is
     * returned.
     *
     * If multiple directives with the same flag, or multiple flags with the same
     * name are found, the respective last one is used.
     *
     * @param name The directive's name.
     * @param flag_name The flag's name.
     * @param default_value The default value.
     * @returns The flag's value or the default value.
     */
    bool get_directive_flag(
      const std::string& name,
      const std::string& flag_name,
      bool default_value) const
    {
        for(auto it = directive_stack.rbegin(); it != directive_stack.rend(); ++it)
        {
            if(it->name.s == name)
            {
                auto flag_it = std::find_if(
                  it->args.begin(),
                  it->args.end(),
                  [&flag_name](const std::pair<token, token>& arg)
                  {
                      return arg.first.s == flag_name;
                  });
                if(flag_it != it->args.end())
                {
                    if(flag_it->second.s.empty() || flag_it->second.s == "true")
                    {
                        return true;
                    }
                    else if(flag_it->second.s == "false")
                    {
                        return false;
                    }
                    else
                    {
                        return default_value;
                    }
                }
            }
        }

        return default_value;
    }

    /** Get the last on the stack directive matching a given name. */
    std::optional<directive> get_last_directive(const std::string& name) const
    {
        auto it = std::find_if(
          directive_stack.rbegin(),
          directive_stack.rend(),
          [&name](const directive& dir)
          {
              return dir.name.s == name;
          });

        if(it != directive_stack.rend())
        {
            return *it;
        }

        return std::nullopt;
    }

    /**
     * Enter a function. Only one function can be entered at a time.
     *
     * @param fn The function.
     */
    void enter_function(function* fn)
    {
        if(current_function != nullptr)
        {
            throw codegen_error("Nested function definition.");
        }

        if(fn == nullptr)
        {
            throw codegen_error("No function specified.");
        }

        current_function = fn;
    }

    /** Exit a function. */
    void exit_function()
    {
        if(current_function == nullptr)
        {
            throw codegen_error("No function to exit.");
        }

        current_function = nullptr;
    }

    /**
     * Return the current function, or nullptr.
     *
     * @param validate If set to `true`, the function will throw a `codegen_error` instead of returning `nullptr`.
     *                 Defaults to `false`.
     * @return Returns the current function.
     */
    function* get_current_function([[maybe_unused]] bool validate = false)
    {
        return current_function;
    }

    /**
     * Set the array type when an array is declared.
     *
     * @param v The array type.
     * @throws Throws a `codegen_error` if the array type was already set.
     */
    void set_array_type(value v)
    {
        if(array_type.has_value())
        {
            throw codegen_error("Cannot set array type since it is already set.");
        }
        array_type = v;
    }

    /**
     * Return the array type.
     *
     * @return The array type.
     * @throws Throws a `codegen_error` if no array was declared.
     */
    value get_array_type() const
    {
        if(!array_type.has_value())
        {
            throw codegen_error("Cannot get array type since no array was declared.");
        }
        return *array_type;
    }

    /**
     * Clear the array type. Needs to be called when array declaration is completed.
     *
     * @throws Throws a `codegen_error` if no array was declared.
     */
    void clear_array_type()
    {
        if(!array_type.has_value())
        {
            throw codegen_error("Cannot clear array type since no array was declared.");
        }
        array_type.reset();
    }

    /**
     * Push a new `break`-`continue` `basic_block` pair.
     *
     * @param brk_cnt The `break`-`continue` pair.
     */
    void push_break_continue(std::pair<basic_block*, basic_block*> brk_cnt)
    {
        basic_block_brk_cnt.push_back(std::move(brk_cnt));
    }

    /**
     * Pop a `break`-`continue` `basic_block` pair.
     *
     * @param loc An optional token location. If provided, this is used in error reporting.
     * @throws Throws a `codegen_error` if the stack is empty.
     */
    void pop_break_continue(std::optional<token_location> loc = std::nullopt)
    {
        if(basic_block_brk_cnt.size() == 0)
        {
            if(loc.has_value())
            {
                throw codegen_error(*loc, "Encountered break or continue statement outside of loop.");
            }
            else
            {
                throw codegen_error("Encountered break or continue statement outside of loop.");
            }
        }
        basic_block_brk_cnt.pop_back();
    }

    /**
     * Return the top `break`-`continue` `basic_block` pair.
     *
     * @param loc An optional token location. If provided, this is used in error reporting.
     * @throws Throws a `codegen_error` if the stack is empty.
     */
    std::pair<basic_block*, basic_block*> top_break_continue(std::optional<token_location> loc = std::nullopt)
    {
        if(basic_block_brk_cnt.size() == 0)
        {
            if(loc.has_value())
            {
                throw codegen_error(*loc, "Encountered break or continue statement outside of loop.");
            }
            else
            {
                throw codegen_error("Encountered break or continue statement outside of loop.");
            }
        }
        return basic_block_brk_cnt.back();
    }

    /** Get `break`-`continue` stack size. */
    std::size_t get_break_continue_stack_size() const
    {
        return basic_block_brk_cnt.size();
    }

    /*
     * Compile-time expression evaluation.
     */

    /**
     * Set an expression's compile-time constancy info.
     *
     * @param expr The expression.
     * @param is_constant Whether the expression is constant.
     */
    void set_expression_constant(const ast::expression& expr, bool is_constant = true);

    /**
     * Get an expression's compile-time constancy info.
     *
     * @param expr The expression.
     * @returns `true` if the expression is known to be compile-time constant, `false` otherwise.
     * @throws Throws a `codegen_error` if the expression is not known.
     */
    bool get_expression_constant(const ast::expression& expr) const;

    /**
     * Check if an expression is known to be compile-time constant.
     *
     * @param expr The expression.
     * @returns `true` if the expression is known to be compile-time constant, `false` otherwise.
     */
    bool has_expression_constant(const ast::expression& expr) const;

    /**
     * Set an expression's compile-time value.
     *
     * @param expr The expression.
     * @param value The expression's value.
     */
    void set_expression_value(const ast::expression& expr, std::unique_ptr<value> v);

    /**
     * Get an expression's compile-time value.
     *
     * @param expr The expression.
     * @returns The expression's value.
     * @throws Throws a `codegen_error` if the expression is not known.
     */
    const value& get_expression_value(const ast::expression& expr) const;

    /**
     * Check if an expression is known to have a compile-time value.
     *
     * @param expr The expression.
     * @returns `true` if the expression is known to have a compile-time value, `false` otherwise.
     */
    bool has_expression_value(const ast::expression& expr) const;

    /*
     * Code generation.
     */

    /**
     * Generate an `arraylength` instruction.
     *
     * Reads an array reference from the stack and pushes its length as an `i32` onto the stack.
     */
    void generate_arraylength();

    /**
     * Generate a binary operator instruction.
     *
     * Reads two values from the stack and pushes the result of the operation to the stack.
     *
     * @param op The binary operation to execute.
     * @param op_type The type specifier for the operation.
     */
    void generate_binary_op(binary_op op, value op_type);

    /**
     * Generate an unconditional branch instruction.
     *
     * @param block The block to jump to. Cannot be a `nullptr`.
     */
    void generate_branch(basic_block* block);

    /**
     * Generate a type cast.
     *
     * Reads a value of a type from the stack and pushes the same value as another type back to the stack.
     *
     * @param tc The type cast to do.
     */
    void generate_cast(type_cast tc);

    /**
     * Generate a `checkcast` instruction for type cast validation.
     *
     * Reads an address off the stack, validates that it is of a given type, and writes it back to the stack.
     * If the check fails, an `interpreter_error` is thrown`.
     *
     * @param tc The type to check for.
     */
    void generate_checkcast(type target_type);

    /**
     * Generate a conditional branch.
     *
     * Pops 'condition off the stack. If 'condition' is != 0, jumps to `then_block`, else to `else_block`.
     *
     * @param then_block The block to jump to if the condition is not false. Cannot be a `nullptr`.
     * @param else_block The block to jump to if the condition is false. Can be a `nullptr`.
     * @throws Throws a `codegen_error` if `then_block` is `nullptr`.
     */
    void generate_cond_branch(basic_block* then_block, basic_block* else_block);

    /**
     * Load a constant value onto the stack.
     *
     * @param vt The value type.
     * @param val The value.
     */
    void generate_const(value vt, std::variant<int, float, std::string> val);

    /** Load 'null' onto the stack. */
    void generate_const_null();

    /**
     * Duplicate the top stack value.
     *
     * @param vt The value type.
     * @param vals The values to skip on the stack before insertion.
     */
    void generate_dup(value vt, std::vector<value> vals = {});

    /**
     * Load a field of a struct instance onto the stack.
     *
     * @param arg The field access details.
     */
    void generate_get_field(std::unique_ptr<field_access_argument> arg);

    /**
     * Statically or dynamically invoke a function. If the invokation
     * is dynamic, the function is loaded from the stack.
     *
     * @param name The function's name for statically invoked functions.
     */
    void generate_invoke(std::optional<std::unique_ptr<function_argument>> name = std::nullopt);

    /**
     * Load an array element onto the stack
     *
     * @param arg The variable to load or `nullptr` for a reference already loaded onto the stack.
     * @param load_element Whether we are loading an element from an array.
     */
    void generate_load(std::unique_ptr<argument> arg, bool load_element = false);

    /**
     * Create a new instance of a type.
     *
     * @param vt The type.
     */
    void generate_new(value vt);

    /**
     * Create a new array of a given built-in type.
     *
     * @param vt The array type.
     */
    void generate_newarray(value vt);

    /**
     * Create a new array of a given custom type.
     *
     * @param vt The array type.
     */
    void generate_anewarray(value vt);

    /**
     * Pop a value from the stack.
     *
     * @param vt The value type.
     */
    void generate_pop(value vt);

    /**
     * Return from a function.
     *
     * @param arg The returned type or std::nullopt.
     */
    void generate_ret(std::optional<value> arg = std::nullopt);

    /**
     * Store the top of the stack into a field of a struct instance on the stack.
     *
     * @param arg The field access details.
     */
    void generate_set_field(std::unique_ptr<field_access_argument> arg);

    /**
     * Store the top of the stack into a variable or an array element.
     *
     * @param arg The variable to store into.
     * @param store_element Whether we are storing an element into an array.
     */
    void generate_store(std::unique_ptr<argument> arg, bool store_element = false);

    /**
     * Generate a label to be used by branches and jump instructions.
     *
     * @returns A unique label identifier.
     */
    std::string generate_label();

    /*
     * Readable representation.
     */

    /** Generate a string representation. */
    std::string to_string() const;
};

/*
 * const_argument implementation.
 */

inline void const_argument::register_const(context& ctx)
{
    if(type->get_type().to_string() == "str")
    {
        auto type_str = static_cast<constant_str*>(type.get());
        type_str->set_constant_index(ctx.get_string(type_str->get_str()));
    }
}

/*
 * basic_block implementation.
 */

inline void basic_block::set_inserting_context(context* ctx)
{
    context* old_context = inserting_context;
    inserting_context = nullptr;

    // Clear the associated context's insertion point.
    if(old_context != nullptr && old_context->get_insertion_point() == this)
    {
        old_context->set_insertion_point(nullptr);
    }

    inserting_context = ctx;
    if(inserting_context != nullptr && inserting_context->get_insertion_point() != this)
    {
        inserting_context->set_insertion_point(this);
    }
}

inline basic_block* basic_block::create(context& ctx, std::string name)
{
    return ctx.basic_blocks.emplace_back(std::unique_ptr<basic_block>(new basic_block(name))).get();
}

/*
 * scope_guard implementation.
 */

inline scope_guard::scope_guard(context& ctx, scope* s)
: ctx{ctx}
, s{s}
{
    ctx.enter_scope(s);
}

inline scope_guard::~scope_guard()
{
    ctx.exit_scope(s);
}

/*
 * function_guard implementation.
 */

inline function_guard::function_guard(context& ctx, function* fn)
: ctx{ctx}
{
    ctx.enter_function(fn);
}

inline function_guard::~function_guard()
{
    ctx.exit_function();
}

}    // namespace slang::codegen
