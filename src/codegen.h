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

/* Forward declarations. */
namespace slang
{
struct token_location;
}    // namespace slang

namespace slang::codegen
{

/** A code generation error. */
class codegen_error : public std::runtime_error
{
public:
    /**
     * Construct a codegen_error.
     *
     * NOTE Use the other constructor if you want to include location information in the error message.
     *
     * @param message The error message.
     */
    codegen_error(const std::string& message)
    : std::runtime_error{message}
    {
    }

    /**
     * Construct a codegen_error.
     *
     * @param loc The error location in the source.
     * @param message The error message.
     */
    codegen_error(const slang::token_location& loc, const std::string& message);
};

/**
 * A value. Values can be generated by evaluating an expression.
 */
class value
{
    /** The variable's type. Can be one of: i32, f32, str, addr, ptr, composite. */
    std::string type;

    /** Composite type name. Only valid if type is value_type::composite. */
    std::optional<std::string> composite_type;

protected:
    /**
     * Validate the value.
     *
     * @throws A codegen_error if the pair (type, composite_type) is invalid.
     */
    void validate() const
    {
        bool is_builtin = (type == "i32") || (type == "f32") || (type == "str");
        bool is_ref = (type == "addr") || (type == "ptr");
        if(is_builtin || is_ref)
        {
            return;
        }

        if(type != "composite")
        {
            throw codegen_error(fmt::format("Invalid value type '{}'.", type));
        }

        if(!composite_type.has_value() || composite_type->length() == 0)
        {
            throw codegen_error("Empty composite type.");
        }
    }

public:
    /** Default constructors. */
    value() = default;
    value(const value&) = default;
    value(value&&) = default;

    /** Default assignments. */
    value& operator=(const value&) = default;
    value& operator=(value&&) = default;

    /**
     * Construct a value.
     *
     * @param type The value type.
     * @param composite_type Type name for composite types.
     */
    value(std::string type, std::optional<std::string> composite_type = std::nullopt)
    : type{std::move(type)}
    , composite_type{std::move(composite_type)}
    {
        validate();
    }

    /**
     * Get the value as a readable string.
     */
    std::string to_string() const
    {
        if(!is_composite())
        {
            return type;
        }

        return get_composite_type();
    }

    /** Get the value's type. */
    std::string get_type() const
    {
        return type;
    }

    /** Return whether this value has a composite type. */
    bool is_composite() const
    {
        return type == "composite";
    }

    /**
     * Get the value's composite type.
     *
     * @throws A codegen_error if called on non-composite type.
     */
    std::string get_composite_type() const
    {
        if(!is_composite())
        {
            throw codegen_error(fmt::format("Cannot get composite type for '{}', as it is not composite.", type));
        }

        return *composite_type;
    }
};

/**
 * A variable.
 */
class variable
{
    /** The variable's name. */
    std::string name;

    /** The variable's type. */
    value type;

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
    variable(std::string name, std::string type, std::string composite_type = {})
    : name{std::move(name)}
    , type{std::move(type), std::move(composite_type)}
    {
    }

    /** Get a string representation of the variable. */
    std::string to_string() const
    {
        return fmt::format("{} %{}", type.to_string(), name);
    }

    /** Get the variable's name. */
    std::string get_name() const
    {
        return name;
    }

    /** Get the variable's type. */
    class value get_value() const
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

    /** Get the argument value. */
    virtual value get_value() const = 0;
};

/**
 * A constant instruction argument.
 */
class const_argument : public argument
{
    /** The constant type. */
    value type;

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
    , type{"i32"}
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
    , type{"f32"}
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
    , type{"str"}
    , value{s}
    {
    }

    /**
     * Create a constant argument from a type and a variant.
     *
     * @param type The constant type.
     * @param v The variant containing the constant.
     */
    const_argument(std::string type, std::variant<int, float, std::string> v)
    : argument()
    , type{type}
    , value{std::move(v)}
    {
    }

    void register_const(class context* ctx) override;

    std::string to_string() const override
    {
        if(type.get_type() == "i32")
        {
            return fmt::format("i32 {}", std::get<int>(value));
        }
        else if(type.get_type() == "f32")
        {
            return fmt::format("f32 {}", std::get<float>(value));
        }
        else if(type.get_type() == "str")
        {
            return fmt::format("str @{}", (constant_index.has_value() ? *constant_index : -1));
        }

        throw codegen_error(fmt::format("Unrecognized const_argument type."));
    }

    virtual slang::codegen::value get_value() const override
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

    value get_value() const override
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
    value vt;

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
    type_argument(value vt)
    : argument()
    , vt{std::move(vt)}
    {
    }

    std::string to_string() const override
    {
        return vt.to_string();
    }

    value get_value() const override
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

    value get_value() const override
    {
        return var.get_value();
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

    value get_value() const override
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
        return name == "jmp" || name == "ifeq";
    }

    /** Returns whether the instruction is a return instruction. */
    virtual bool is_return() const
    {
        return name == "ret";
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

    /** Return whether this block ends with a return statement. */
    bool ends_with_return() const
    {
        return instrs.size() > 0 && instrs.back()->is_return();
    }

    /**
     * Return whether this block is valid.
     *
     * A block is valid if it contains a single return or branch instruction,
     * located at its end.
     */
    bool is_valid() const
    {
        std::size_t branch_return_count = 0;
        bool last_instruction_branch_return = false;
        for(auto& it: instrs)
        {
            last_instruction_branch_return = it->is_branching() || it->is_return();
            if(last_instruction_branch_return)
            {
                branch_return_count += 1;
            }
        }
        return branch_return_count == 1 && last_instruction_branch_return;
    }
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
     * Create a scope and initialize it with function arguments.
     *
     * @param name The scope's name (usually the same as the function's name)
     * @param args The function's arguments.
     */
    scope(std::string name, std::vector<std::unique_ptr<variable>> args)
    : name{std::move(name)}
    , args{std::move(args)}
    {
    }

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
     * Get the variable for the given name.
     *
     * @param name The variable's name.
     * @returns The variable or nullptr.
     */
    variable* get_variable(const std::string& name)
    {
        auto it = std::find_if(args.begin(), args.end(),
                               [&name](const std::unique_ptr<variable>& v) -> bool
                               {
                                   return v->get_name() == name;
                               });
        if(it != args.end())
        {
            return it->get();
        }

        it = std::find_if(locals.begin(), locals.end(),
                          [&name](const std::unique_ptr<variable>& v) -> bool
                          {
                              return v->get_name() == name;
                          });

        if(it != locals.end())
        {
            return it->get();
        }

        return nullptr;
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
    context* ctx;

    /** The scope. */
    scope* s;

public:
    /** No default constructor. */
    scope_guard() = delete;

    /** Default copy and move constructors. */
    scope_guard(const scope_guard&) = default;
    scope_guard(scope_guard&&) = default;

    /**
     * Construct a scope guard.
     *
     * @param ctx The associated context.
     * @param s The scope.
     */
    scope_guard(context* ctx, scope* s);

    /** Destructor. */
    ~scope_guard();

    /** Default assignments.*/
    scope_guard& operator=(const scope_guard&) = default;
    scope_guard& operator=(scope_guard&&) = default;
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
    function(std::string name, std::string return_type, std::vector<std::unique_ptr<variable>> args)
    : name{name}
    , return_type{std::move(return_type)}
    , scope{std::move(name), std::move(args)}
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

    /** String representation of function. */
    std::string to_string() const
    {
        std::string buf = fmt::format("define {} @{}(", return_type, name);

        const auto& args = scope.get_args();
        if(args.size() > 0)
        {
            for(std::size_t i = 0; i < args.size() - 1; ++i)
            {
                buf += fmt::format("{}, ", args[i]->to_string());
            }
            buf += fmt::format("{}) ", args.back()->to_string());
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
    op_or,            /** a | b*/
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

    /** The current scope stack. */
    std::vector<scope*> current_scopes;

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
    void validate_insertion_point() const
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
    function* create_function(std::string name, std::string return_type, std::vector<std::unique_ptr<variable>> args);

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
    void generate_binary_op(binary_op op, value op_type);

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
    void generate_const(value vt, std::variant<int, float, std::string> val);

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
    void generate_ret(std::optional<value> arg = std::nullopt);

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

    /** Returns whether the last instruction was a return instruction. */
    bool ends_with_return() const;

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
    if(type.get_type() == "str")
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

/*
 * scope_guard implementation.
 */

inline scope_guard::scope_guard(context* ctx, scope* s)
: ctx{ctx}
, s{s}
{
    ctx->enter_scope(s);
}

inline scope_guard::~scope_guard()
{
    ctx->exit_scope(s);
}

}    // namespace slang::codegen
