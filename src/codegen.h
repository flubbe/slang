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

#include <exception>
#include <list>
#include <string>
#include <vector>

#include <fmt/core.h>

namespace slang::codegen
{

/** A code generation error. */
class codegen_error : public std::runtime_error
{
public:
    /**
     * Construct a codegen_error.
     *
     * @param message The error message.
     */
    codegen_error(const std::string& message)
    : std::runtime_error{message}
    {
    }
};

/** Value type. */
enum class value_type
{
    i32,      /** 32-bit integer value */
    f32,      /** 32-bit floating-point value */
    str,      /** string value */
    addr,     /** Load the address of a variable. */
    ptr,      /** A pointer. */
    composite /** A composite type. */
};

/** Convert a value_type into a string. */
inline std::string to_string(value_type vt)
{
    if(vt == value_type::i32)
    {
        return {"i32"};
    }
    else if(vt == value_type::f32)
    {
        return {"f32"};
    }
    else if(vt == value_type::str)
    {
        return {"str"};
    }
    else if(vt == value_type::addr)
    {
        return {"addr"};
    }
    else if(vt == value_type::ptr)
    {
        return {"ptr"};
    }
    else if(vt == value_type::composite)
    {
        return {"composite"};
    }

    throw std::runtime_error("Invalid argument to to_string(value_type).");
}

/**
 * Convert a string to a value_type.
 *
 * @param s The string to convert.
 * @returns The value_type corresponding to the string.
 * @throws A std::runtime_error if the string did not contain a value_type.
 */
inline value_type value_type_from_string(const std::string& s)
{
    if(s == "i32")
    {
        return value_type::i32;
    }
    else if(s == "f32")
    {
        return value_type::f32;
    }
    else if(s == "str")
    {
        return value_type::str;
    }
    else if(s == "addr")
    {
        return value_type::addr;
    }
    else if(s == "ptr")
    {
        return value_type::ptr;
    }
    else if(s == "composite")
    {
        return value_type::composite;
    }

    throw std::runtime_error(fmt::format("Could not convert '{}' to value_type.", s));
}

/**
 * A variable.
 */
class variable
{
    /** The variable's name. */
    std::string name;

    /** The variable's type. */
    value_type type;

    /** Composite type name. Only valid if type is value_type::composite. */
    std::string composite_type;

public:
    /** Default constructors. */
    variable() = default;
    variable(const variable&) = default;
    variable(variable&&) = default;

    /** Destructor. */
    virtual ~variable() = default;

    /** Default assignments. */
    variable& operator=(const variable&) = default;
    variable& operator=(variable&&) = default;

    /**
     * Construct a variable.
     *
     * @param name The variable's name.
     * @param type The variable's type.
     */
    variable(std::string name, value_type type, std::string composite_type = {})
    : name{std::move(name)}
    , type{type}
    , composite_type{std::move(composite_type)}
    {
    }

    /** Get a string representation of the variable. */
    std::string to_string() const
    {
        if(type != value_type::composite)
        {
            return fmt::format("{} %{}", codegen::to_string(type), name);
        }

        return fmt::format("{} %{}", composite_type, name);
    }

    /** Get the variable's name. */
    std::string get_name() const
    {
        return name;
    }

    /** Get the variable's type. */
    value_type get_type() const
    {
        return type;
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
    virtual void register_const(class context* ctx)
    {
    }

    /** Get a string representation of the argument. */
    virtual std::string to_string() const = 0;

    /** Get the argument type. */
    virtual value_type get_type() const = 0;
};

/**
 * A constant instruction argument.
 */
class const_argument : public argument
{
    /** The constant type. */
    value_type type;

    /** The constant. */
    std::variant<int, float, std::string> value;

    /** Index into the constant table (for strings). */
    std::optional<std::int32_t> constant_index{std::nullopt};

public:
    /** Default constructors. */
    const_argument() = default;
    const_argument(const const_argument&) = default;
    const_argument(const_argument&&) = default;

    /** Default assignments. */
    const_argument& operator=(const const_argument&) = default;
    const_argument& operator=(const_argument&&) = default;

    /**
     * Create an integer argument.
     *
     * @param i The constant integer.
     */
    const_argument(int i)
    : argument()
    , type{value_type::i32}
    , value{i}
    {
    }

    /**
     * Create a floating-point argument.
     *
     * @param f The floating-point integer.
     */
    const_argument(float f)
    : argument()
    , type{value_type::f32}
    , value{f}
    {
    }

    /**
     * Create a string argument.
     *
     * @param s The string.
     */
    const_argument(std::string s)
    : argument()
    , type{value_type::str}
    , value{s}
    {
    }

    /**
     * Create a constant argument from a type and a variant.
     *
     * @param type The constant type.
     * @param v The variant containing the constant.
     */
    const_argument(value_type type, std::variant<int, float, std::string> v)
    : argument()
    , type{type}
    , value{std::move(v)}
    {
    }

    void register_const(class context* ctx) override;

    std::string to_string() const override
    {
        if(type == value_type::i32)
        {
            return fmt::format("i32 {}", std::get<int>(value));
        }
        else if(type == value_type::f32)
        {
            return fmt::format("f32 {}", std::get<float>(value));
        }
        else if(type == value_type::str)
        {
            return fmt::format("str @{}", (constant_index.has_value() ? *constant_index : -1));
        }

        throw codegen_error(fmt::format("Unrecognized const_argument type."));
    }

    value_type get_type() const override
    {
        return type;
    }
};

/**
 * Function argument.
 */
class function_argument : public argument
{
    /** The function's name. */
    std::string name;

public:
    /** Default constructors. */
    function_argument() = default;
    function_argument(const function_argument&) = default;
    function_argument(function_argument&&) = default;

    /** Default assignments. */
    function_argument& operator=(const function_argument&) = default;
    function_argument& operator=(function_argument&&) = default;

    /**
     * Create a function argument.
     *
     * @param name The function name.
     */
    function_argument(std::string name)
    : argument()
    , name{std::move(name)}
    {
    }

    std::string to_string() const override
    {
        return fmt::format("@{}", name);
    }

    value_type get_type() const override
    {
        throw codegen_error(fmt::format("Requested type of a function argument."));
    }
};

/**
 * Type argument.
 */
class type_argument : public argument
{
    /** The type. */
    value_type vt;

public:
    /** Default constructors. */
    type_argument() = default;
    type_argument(const type_argument&) = default;
    type_argument(type_argument&&) = default;

    /** Default assignments. */
    type_argument& operator=(const type_argument&) = default;
    type_argument& operator=(type_argument&&) = default;

    /**
     * Create a type argument.
     *
     * @param v The variable.
     */
    type_argument(value_type vt)
    : argument()
    , vt{std::move(vt)}
    {
    }

    std::string to_string() const override
    {
        return codegen::to_string(vt);
    }

    value_type get_type() const override
    {
        return vt;
    }
};

/**
 * A variable instruction argument.
 */
class variable_argument : public argument
{
    /** The variable. */
    variable var;

public:
    /** Default constructors. */
    variable_argument() = default;
    variable_argument(const variable_argument&) = default;
    variable_argument(variable_argument&&) = default;

    /** Default assignments. */
    variable_argument& operator=(const variable_argument&) = default;
    variable_argument& operator=(variable_argument&&) = default;

    /**
     * Create a variable argument.
     *
     * @param v The variable.
     */
    variable_argument(variable v)
    : argument()
    , var{std::move(v)}
    {
    }

    std::string to_string() const override
    {
        return fmt::format("{}", var.to_string());
    }

    value_type get_type() const override
    {
        return var.get_type();
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
    label_argument(std::string label)
    : argument()
    , label{std::move(label)}
    {
    }

    std::string to_string() const override
    {
        return fmt::format("%{}", label);
    }

    value_type get_type() const override
    {
        throw codegen_error(fmt::format("Cannot get type from label '{}'.", to_string()));
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
    instruction(std::string name)
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
        return false;
    }

    /** Get instruction representation as string. */
    std::string to_string() const
    {
        std::string buf;
        if(args.size() > 0)
        {
            buf = " ";
            for(std::size_t i = 0; i < args.size() - 1; ++i)
            {
                buf += args[i]->to_string() + ", ";
            }
            buf += args.back()->to_string();
        }
        return fmt::format("{}{}", name, buf);
    }
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

    /**
     * Set a new context for inserting instructions.
     * Pass nullptr to clear the context.
     *
     * @param ctx The new context or nullptr.
     */
    void set_inserting_context(class context* ctx);

public:
    /** Default constructors. */
    basic_block() = default;
    basic_block(const basic_block&) = default;
    basic_block(basic_block&&) = default;

    /** Destructor. */
    virtual ~basic_block()
    {
        // clear references to this block.
        set_inserting_context(nullptr);
    }

    /** Default assignments. */
    basic_block& operator=(const basic_block&) = default;
    basic_block& operator=(basic_block&&) = default;

    /**
     * Create a basic_block.
     *
     * @param label The block's label.
     */
    basic_block(std::string label)
    : label{std::move(label)}
    {
    }

    /**
     * Add a non-branching instruction.
     *
     * Throws a codegen_error if the instruction is branching.
     *
     * @param instr The instruction.
     */
    void add_instruction(std::unique_ptr<instruction> instr)
    {
        instrs.emplace_back(std::move(instr));
    }

    /** Get string representation of block. */
    std::string to_string() const
    {
        if(instrs.size() == 0)
        {
            return fmt::format("{}:", label);
        }

        std::string buf = fmt::format("{}:\n", label);
        for(std::size_t i = 0; i < instrs.size() - 1; ++i)
        {
            buf += fmt::format(" {}\n", instrs[i]->to_string());
        }
        buf += fmt::format(" {}", instrs.back()->to_string());
        return buf;
    }

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
};

/**
 * A scope has a name and holds variables.
 */
class scope
{
    /** Reference to the outer scope (if any). */
    scope* outer{nullptr};

    /** Arguments for function scopes. */
    std::vector<std::unique_ptr<variable>> args;

    /** Variables inside the scope. */
    std::vector<std::unique_ptr<variable>> locals;

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
     * Check if the name is already contained in this scope as an argument or a local variable.
     *
     * @returns True if the name exists.
     */
    bool contains(const std::string& name) const
    {
        if(std::find_if(args.begin(), args.end(),
                        [&name](const std::unique_ptr<variable>& v) -> bool
                        {
                            return v->get_name() == name;
                        })
           != args.end())
        {
            return true;
        }

        if(std::find_if(locals.begin(), locals.end(),
                        [&name](const std::unique_ptr<variable>& v) -> bool
                        {
                            return v->get_name() == name;
                        })
           != locals.end())
        {
            return true;
        }

        return false;
    }

    /**
     * Add an argument.
     *
     * @param arg The argument.
     * @throws Throws a `codegen_error` if the scope already has an object with the same name.
     */
    void add_argument(std::unique_ptr<variable> arg)
    {
        if(contains(arg->get_name()))
        {
            throw codegen_error(fmt::format("Argument name '{}' already contained in scope.", arg->get_name()));
        }
        args.emplace_back(std::move(arg));
    }

    /**
     * Add a local variable.
     *
     * @param arg The variable.
     * @throws Throws a `codegen_error` if the scope already has an object with the same name.
     */
    void add_local(std::unique_ptr<variable> arg)
    {
        if(contains(arg->get_name()))
        {
            throw codegen_error(fmt::format("Argument name '{}' already contained in scope.", arg->get_name()));
        }
        locals.emplace_back(std::move(arg));
    }

    /** Get the arguments for this scope. */
    const std::vector<std::unique_ptr<variable>>& get_args() const
    {
        return args;
    }

    /** Get the locals for this scope. */
    const std::vector<std::unique_ptr<variable>>& get_locals() const
    {
        return locals;
    }
};

/**
 * A relocatable function.
 */
class function
{
    /** The function's name. */
    std::string name;

    /** The function's return type. */
    std::string return_type;

    /** The function's arguments. */
    std::vector<variable> args;

    /** The function variable scope. */
    scope scope;

    /** Function instructions. */
    std::list<basic_block> instr_blocks;

public:
    /** Constructors. */
    function() = default;
    function(const function&) = delete;
    function(function&&) = default;

    /**
     * Construct a function from a name and return type.
     */
    function(std::string name, std::string return_type, std::vector<variable> args)
    : name{std::move(name)}
    , return_type{std::move(return_type)}
    , args{std::move(args)}
    {
    }

    /** Destructor. */
    virtual ~function() = default;

    /** Assignments. */
    function& operator=(const function&) = delete;
    function& operator=(function&&) = default;

    /** Get the function's name. */
    const std::string& get_name() const
    {
        return name;
    }

    /** Get the instruction block. */
    basic_block* create_basic_block(std::string name)
    {
        instr_blocks.emplace_back(std::move(name));
        return &instr_blocks.back();
    }

    /** Create a local variable. */
    void create_local(std::unique_ptr<variable> v)
    {
        scope.add_local(std::move(v));
    }

    /** String representation of function. */
    std::string to_string() const
    {
        std::string buf = fmt::format("define {} @{}(", return_type, name);
        if(args.size() > 0)
        {
            for(std::size_t i = 0; i < args.size() - 1; ++i)
            {
                buf += fmt::format("{}, ", args[i].to_string());
            }
            buf += fmt::format("{}) ", args.back().to_string());
        }
        else
        {
            buf += ") ";
        }

        buf += "{\n";
        for(auto& v: scope.get_locals())
        {
            buf += fmt::format("local {}\n", v->to_string());
        }
        for(auto& b: instr_blocks)
        {
            buf += fmt::format("{}\n", b.to_string());
        }
        buf += "}";

        return buf;
    }
};

/**
 * A user-defined type.
 */
class type
{
    /** The type's name. */
    std::string name;

    /** The type's members as pairs (type_name, member_name). */
    std::vector<std::pair<std::string, std::string>> members;

public:
    /** Default constructors. */
    type() = default;
    type(const type&) = default;
    type(type&&) = default;

    /** Destructor. */
    virtual ~type() = default;

    /** Default assignment. */
    type& operator=(const type&) = default;
    type& operator=(type&&) = default;

    /**
     * Construct a new type.
     *
     * @param name The type's name.
     * @param members The type's members.
     */
    type(std::string name, std::vector<std::pair<std::string, std::string>> members)
    : name{std::move(name)}
    , members{std::move(members)}
    {
    }

    /** Get the type's name. */
    std::string get_name() const
    {
        return name;
    }

    /** String representation of a type. */
    std::string to_string() const
    {
        std::string buf = fmt::format("%{} = type {{\n", name);
        if(members.size() > 0)
        {
            for(std::size_t i = 0; i < members.size() - 1; ++i)
            {
                buf += fmt::format(" {} %{},\n", members[i].second, members[i].first);
            }
            buf += fmt::format(" {} %{},\n", members.back().second, members.back().first);
        }
        buf += "}";
        return buf;
    }
};

/**
 * A binary operation. Reads two alike values from the stack and puts a value of the same type onto the stack.
 */
enum class binary_op
{
    op_add, /** a + b */
    op_sub, /** a - b */
    op_mul, /** a * b */
    op_div, /** a / b */
    op_mod, /** a % b */
    op_and, /** a & b */
    op_or,  /** a | b*/
    op_shl, /** a << b */
    op_shr, /** a >> b */
};

/**
 * Return a string representation of a binary operator.
 *
 * @param op The binary operator.
 * @returns A string representation of the operator.
 */
std::string to_string(binary_op op);

/**
 * Code generator context.
 */
class context
{
    /** List of types. */
    std::vector<std::unique_ptr<type>> types;

    /** String table. */
    std::vector<std::string> strings;

    /** Global scope. */
    std::unique_ptr<scope> global_scope;

    /** List of functions. */
    std::vector<std::unique_ptr<function>> funcs;

    /** Current instruction insertion point. */
    basic_block* insertion_point{nullptr};

protected:
    /**
     * Check that the insertion point is not null.
     *
     * Throws a codegen_error if the insertion point is null.
     */
    void validate_insertion_point()
    {
        if(!insertion_point)
        {
            throw codegen_error("Invalid insertion point (nullptr).");
        }
    }

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
     * Create a type.
     *
     * Throws a codegen_error if the type already exists or if it contains undefined types.
     *
     * @param name The type's name.
     * @returns A representation of the created type.
     */
    type* create_type(std::string name, std::vector<std::pair<std::string, std::string>> members);

    /**
     * Get a reference to a string or create a new one if it does not exist.
     *
     * @param str The string.
     * @returns An index into the string table.
     */
    std::size_t get_string(std::string str);

    /**
     * Add a function definition.
     *
     * Throws a codegen_error if the function name already exists.
     *
     * @param name The function's name.
     * @param return_type The function's return type.
     * @param args The function's arguments.
     * @returns A representation of the function.
     */
    function* create_function(std::string name, std::string return_type, std::vector<variable> args);

    /**
     * Set instruction insertion point.
     *
     * @param ip The insertion point for instructions.
     */
    void set_insertion_point(basic_block* ip);

    /** Get the current insertion point. */
    basic_block* get_insertion_point() const
    {
        return insertion_point;
    }

    /*
     * Code generation.
     */

    /**
     * Generate a binary operator instruction.
     *
     * Reads two values from the stack and pushes the result of the operation to the stack.
     *
     * @param op The binary operation to execute.
     * @param op_type The type specifier for the operation.
     */
    void generate_binary_op(binary_op op, value_type op_type);

    /**
     * Generate an unconditional branch instruction.
     *
     * @param label The name of the jump target. This is the label of a basic_block.
     */
    void generate_branch(std::unique_ptr<label_argument> label);

    /**
     * Generate a compare instruction.
     *
     * Reads two values [value0, value1] from the stack and pushes the comparison result to the stack:
     * If value0==value1, 0 is pushed onto the stack. If value0<value1, -1 is pushed onto the stack.
     * If value0>value1, 1 is pushed onto the stack.
     */
    void generate_cmp();

    /**
     * Generate a conditional branch.
     *
     * Pops the top three values [condition, jmp_true, jmp_false] (all i32) off the
     * stack. If 'condition' is != 0, jumps to jmp_true, else to jmp_false.
     *
     * @param then_block The block to jump to if the condition is not false.
     * @param else_block The block to jump to if the condition is false.
     */
    void generate_cond_branch(basic_block* then_block, basic_block* else_block);

    /**
     * Load a constant value onto the stack.
     *
     * @param vt The value type.
     * @param val The value.
     */
    void generate_const(value_type vt, std::variant<int, float, std::string> val);

    /**
     * Statically or dynamically invoke a function. If the invokation
     * is dynamic, the function is loaded from the stack.
     *
     * @param name The function's name for statically invoked functions.
     */
    void generate_invoke(std::optional<std::unique_ptr<function_argument>> name = std::nullopt);

    /**
     * Load a variable onto the stack.
     *
     * @param arg The variable or function to load.
     */
    void generate_load(std::unique_ptr<argument> arg);

    /**
     * Load an element from a structure onto the stack.
     *
     * NOTE indices is a vector of `int`'s, since currently constant instruction arguments
     *      are `i32`, `f32` or `str`.
     *
     * @param indices Indices into a (possibly nested) structure.
     */
    void generate_load_element(std::vector<int> indices);

    /**
     * Return from a function.
     *
     * @param arg The returned type or std::nullopt.
     */
    void generate_ret(std::optional<value_type> arg = std::nullopt);

    /**
     * Store the top of the stack into a variable.
     *
     * @param arg The variable to store into.
     */
    void generate_store(std::unique_ptr<variable_argument> arg);

    /**
     * Store the top of the stack into a structure.
     *
     * NOTE indices is a vector of `int`'s, since currently constant instruction arguments
     *      are `i32`, `f32` or `str`.
     *
     * @param indices Indices into a (possibly nested) structure.
     */
    void generate_store_element(std::vector<int> indices);

    /** Generate a string representation. */
    std::string to_string() const
    {
        std::string buf;

        // string table.
        if(strings.size())
        {
            auto make_printable = [](const std::string& s) -> std::string
            {
                std::string str;

                // replace non-printable characters by their character codes.
                for(auto& c: s)
                {
                    if(!::isalnum(c) && c != ' ')
                    {
                        str += fmt::format("\\x{:02x}", c);
                    }
                    else
                    {
                        str += c;
                    }
                }

                return str;
            };

            for(std::size_t i = 0; i < strings.size() - 1; ++i)
            {
                buf += fmt::format(".string @{} \"{}\"\n", i, make_printable(strings[i]));
            }
            buf += fmt::format(".string @{} \"{}\"", strings.size() - 1, make_printable(strings.back()));

            // don't append a newline if the string table is the only non-empty buffer.
            if(types.size() != 0 || funcs.size() != 0)
            {
                buf += "\n";
            }
        }

        if(types.size() != 0)
        {
            for(std::size_t i = 0; i < types.size() - 1; ++i)
            {
                buf += fmt::format("{}\n", types[i]->to_string());
            }
            buf += fmt::format("{}", types.back()->to_string());

            // don't append a newline if there are no functions.
            if(funcs.size() != 0)
            {
                buf += "\n";
            }
        }

        if(funcs.size() != 0)
        {
            for(std::size_t i = 0; i < funcs.size() - 1; ++i)
            {
                buf += fmt::format("{}\n", funcs[i]->to_string());
            }
            buf += fmt::format("{}", funcs.back()->to_string());
        }

        return buf;
    }
};

/*
 * const_argument implementation.
 */
inline void const_argument::register_const(context* ctx)
{
    if(type == value_type::str)
    {
        constant_index = ctx->get_string(std::get<std::string>(value));
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

}    // namespace slang::codegen
