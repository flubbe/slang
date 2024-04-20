/**
 * slang - a simple scripting language.
 *
 * interpreter.
 *
 * \author Felix Lubbe
 * \copyright Copyright (c) 2024
 * \license Distributed under the MIT software license (see accompanying LICENSE.txt).
 */

#include <functional>
#include <stdexcept>
#include <unordered_map>
#include <utility>
#include <variant>
#include <vector>

#include "filemanager.h"
#include "module.h"
#include "opcodes.h"

namespace slang::interpreter
{

/** Interpreter error. */
class interpreter_error : public std::runtime_error
{
public:
    /**
     * Construct a `interpreter_error`.
     *
     * @param message The error message.
     */
    interpreter_error(const std::string& message)
    : std::runtime_error{message}
    {
    }
};

/** Result and argument type. */
class value
{
    /** The stored value. */
    std::variant<int, float, std::string> v;

    /** Read this value from memory. */
    std::function<std::size_t(std::byte*, value&)> reader;

    /** Write this value to memory. */
    std::function<std::size_t(std::byte*, value&)> writer;

    /** Size of the value, in bytes. */
    std::size_t size;

    /** String identifying the type. */
    std::string type;

    /**
     * Reads a primitive type into a `value`.
     *
     * @param memory The memory to read from.
     * @param v The value to write the integer to.
     * @returns Returns `sizeof(T)`.
     */
    template<typename T>
    static std::size_t read_primitive_type(std::byte* memory, value& v)
    {
        static_assert(std::is_integral_v<T> || std::is_floating_point_v<T>,
                      "Primitive type must be an integer or a floating point type.");
        v = *reinterpret_cast<T*>(memory);
        return sizeof(T);
    }

    /**
     * Writes a primitive type value into memory.
     *
     * @param memory The memory to write into.
     * @param v The value to write.
     * @returns Returns `sizeof(T)`.
     */
    template<typename T>
    static std::size_t write_primitive_type(std::byte* memory, value& v)
    {
        static_assert(std::is_integral_v<T> || std::is_floating_point_v<T>,
                      "Primitive type must be an integer or a floating point type.");
        *reinterpret_cast<T*>(memory) = std::get<T>(v.v);
        return sizeof(T);
    }

    /**
     * Reads a string into a `value`.
     *
     * @note This copies the string.
     *
     * @param memory The memory to read from.
     * @param v The value write the string to.
     * @returns Returns `sizeof(std::string*)`.
     */
    static std::size_t read_str(std::byte* memory, value& v)
    {
        std::string* s = *reinterpret_cast<std::string**>(memory);
        v = *s;
        return sizeof(std::string*);
    }

    /**
     * Writes a string reference into memory.
     *
     * @note The string is owned by `v`.
     *
     * @param memory The memory to write into.
     * @param v The string value to write.
     * @returns Returns `sizeof(std::string*)`.
     */
    static std::size_t write_str(std::byte* memory, value& v)
    {
        std::string* s = std::get_if<std::string>(&v.v);
        if(!s)
        {
            throw std::bad_variant_access();
        }
        *reinterpret_cast<std::string**>(memory) = s;
        return sizeof(std::string*);
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
     * Construct a integer value.
     *
     * @param i The integer.
     */
    value(int i)
    : v{i}
    , reader{read_primitive_type<int>}
    , writer{write_primitive_type<int>}
    , size{sizeof(int)}
    , type{"i32"}
    {
    }

    /**
     * Construct a floating point value.
     *
     * @param f The floating point value.
     */
    value(float f)
    : v{f}
    , reader{read_primitive_type<float>}
    , writer{write_primitive_type<float>}
    , size{sizeof(float)}
    , type{"f32"}
    {
    }

    /**
     * Construct a string value.
     *
     * @note The string is owned by this `value`. This is relevant when using `write`,
     * which writes a pointer to the specified memory address.
     *
     * @param s The string.
     */
    value(std::string s)
    : v{std::move(s)}
    , reader{read_str}
    , writer{write_str}
    , size{sizeof(std::string*)}
    , type{"str"}
    {
    }

    /**
     * Construct a string value.
     *
     * @param s The string.
     */
    value(const char* s)
    : v{s}
    , reader{read_str}
    , writer{write_str}
    , size{sizeof(std::string*)}
    , type{"str"}
    {
    }

    /**
     * Read the value from memory.
     *
     * @param memory The memory to read from.
     * @returns Returns the value's size in bytes.
     */
    std::size_t read(std::byte* memory)
    {
        return reader(memory, *this);
    }

    /**
     * Write the value into memory.
     *
     * @param memory The memory to write to.
     * @returns Returns the value's size in bytes.
     */
    std::size_t write(std::byte* memory)
    {
        return writer(memory, *this);
    }

    /** Get the value's size. */
    std::size_t get_size() const
    {
        return size;
    }

    /** Get the value's type. */
    const std::string& get_type() const
    {
        return type;
    }

    /** Access the underlying value. */
    template<typename T>
    T* get()
    {
        T* v_ptr = std::get_if<T>(&v);
        if(!v_ptr)
        {
            throw std::bad_variant_access();
        }
        return v_ptr;
    }

    /** Access the underlying value. */
    template<typename T>
    const T* get() const
    {
        T* v_ptr = std::get_if<T>(&v);
        if(!v_ptr)
        {
            throw std::bad_variant_access();
        }
        return v_ptr;
    }

    /** Check if the value holds a given type. */
    template<typename T>
    bool holds_type() const
    {
        return std::holds_alternative<T>(v);
    }
};

/** Operand stack. */
class operand_stack
{
protected:
    /** The stack. */
    std::vector<std::uint8_t> stack;

public:
    /** Default constructors. */
    operand_stack() = default;
    operand_stack(const operand_stack&) = default;
    operand_stack(operand_stack&&) = default;

    /** Default assignments. */
    operand_stack& operator=(const operand_stack&) = default;
    operand_stack& operator=(operand_stack&&) = default;

    /** Check if the stack is empty. */
    bool empty() const noexcept
    {
        return stack.empty();
    }

    /** Get the current stack size. */
    std::size_t size() const noexcept
    {
        return stack.size();
    }

    /**
     * Push an i32 onto the stack.
     *
     * @param i The integer.
     */
    void push_i32(std::int32_t i)
    {
        stack.insert(stack.end(), reinterpret_cast<std::uint8_t*>(&i), reinterpret_cast<std::uint8_t*>(&i) + 4);
    }

    /**
     * Push an f32 onto the stack.
     *
     * @param f The floating-point value.
     */
    void push_f32(float f)
    {
        stack.insert(stack.end(), reinterpret_cast<std::uint8_t*>(&f), reinterpret_cast<std::uint8_t*>(&f) + 4);
    }

    /**
     * Push an address onto the stack.
     *
     * @param addr The address.
     */
    template<typename T>
    void push_addr(const T* addr)
    {
        stack.insert(stack.end(), reinterpret_cast<std::uint8_t*>(&addr), reinterpret_cast<std::uint8_t*>(&addr) + sizeof(const T*));
    }

    /**
     * Push another stack onto this stack.
     *
     * @param other The other stack.
     */
    void push_stack(const operand_stack& other)
    {
        stack.insert(stack.end(), other.stack.begin(), other.stack.end());
    }

    /** Pop an i32 from the stack. */
    std::int32_t pop_i32()
    {
        if(stack.size() < 4)
        {
            throw interpreter_error("Stack underflow.");
        }

        std::int32_t i = *reinterpret_cast<std::int32_t*>(&stack[stack.size() - 4]);
        stack.resize(stack.size() - 4);
        return i;
    }

    /** Pop an f32 from the stack. */
    float pop_f32()
    {
        if(stack.size() < 4)
        {
            throw interpreter_error("Stack underflow.");
        }

        float f = *reinterpret_cast<float*>(&stack[stack.size() - 4]);
        stack.resize(stack.size() - 4);
        return f;
    }

    /** Pop an address from the stack. */
    template<typename T>
    T* pop_addr()
    {
        if(stack.size() < sizeof(T*))
        {
            throw interpreter_error("Stack underflow.");
        }

        T* addr = *reinterpret_cast<T**>(&stack[stack.size() - sizeof(T*)]);
        stack.resize(stack.size() - sizeof(T*));
        return addr;
    }

    /** Disallow popping from stack for non-(int, float, std::string) types. */
    template<typename T>
    value pop_result()
    {
        static_assert(
          !std::is_same<T, int>::value
            && !std::is_same<T, float>::value
            && !std::is_same<T, std::string>::value,
          "operand_stack::pop_result is only available for int, float and std::string.");
    }

    /** Pop the call result from the stack. */
    template<>
    value pop_result<int>()
    {
        return {pop_i32()};
    }

    /** Pop the call result from the stack. */
    template<>
    value pop_result<float>()
    {
        return {pop_f32()};
    }

    /** Pop the call result from the stack. */
    template<>
    value pop_result<std::string>()
    {
        throw std::runtime_error("operand_stack::pop_result<std::string> not implemented.");
    }

    /**
     * Get a pointer to the end minus an offset.
     *
     * @param offset Offset to subtract from the end.
     * @throws Throws an `interpreter_error` if a stack underflow occurs.
     */
    std::uint8_t* end(std::size_t offset = 0)
    {
        if(offset > stack.size())
        {
            throw interpreter_error("Stack underflow");
        }
        return &stack[stack.size() - offset];
    }

    /**
     * Discard bytes.
     *
     * @param byte_count The count of bytes to discard.
     * @throws Throws an `interpreter_error` if a stack underflow occurs.
     */
    void discard(std::size_t byte_count)
    {
        if(byte_count > stack.size())
        {
            throw interpreter_error("Stack underflow");
        }
        stack.resize(stack.size() - byte_count);
    }
};

/** A function. */
class function
{
    /** Function signature. */
    function_signature signature;

    /** Whether this is a native function. */
    bool native;

    /** Entry point (offset into binary) or function pointer for native functions. */
    std::variant<std::size_t, std::function<void(operand_stack&)>> entry_point_or_function;

    /** Bytecode size for interpreted functions. */
    std::size_t size;

    /** Return opcode. */
    opcode ret_opcode;

public:
    /** Argument and locals size. Not serialized. */
    std::size_t locals_size = 0;

public:
    /** Defaulted and deleted constructors. */
    function() = default;
    function(const function&) = delete;
    function(function&&) = default;

    /** Default and deleted assignments. */
    function& operator=(const function&) = delete;
    function& operator=(function&&) = default;

    /**
     * Construct a function.
     *
     * @param signature The function's signature.
     * @param entry_point The function's entry point, given as an offset into a module binary.
     * @param size The bytecode size.
     * @param locals_size The arguments and locals size.
     */
    function(function_signature signature, std::size_t entry_point, std::size_t size, std::size_t locals_size);

    /**
     * Construct a native function.
     *
     * @param signature The function's signature.
     * @param func An std::function.
     */
    function(function_signature signature, std::function<void(operand_stack&)> func);

    /** Get the function signature. */
    const function_signature& get_signature() const
    {
        return signature;
    }

    /** Return whether this is a native function. */
    bool is_native() const
    {
        return native;
    }

    /** Get the function's entry point. */
    std::size_t get_entry_point() const
    {
        return std::get<std::size_t>(entry_point_or_function);
    }

    /** Get a std::function. */
    const std::function<void(operand_stack&)>& get_function() const
    {
        return std::get<std::function<void(operand_stack&)>>(entry_point_or_function);
    }

    /** Get the bytecode size. */
    std::size_t get_size() const
    {
        return size;
    }

    /** Get the return opcode. */
    opcode get_return_opcode() const
    {
        return ret_opcode;
    }
};

/** A stack frame. */
struct stack_frame
{
    /** String table reference. */
    const std::vector<std::string>& string_table;

    /** Locals and arguments. */
    std::vector<std::byte> locals;

    /** The operand stack. */
    operand_stack stack;

    /** Default constructors. */
    stack_frame() = delete;
    stack_frame(const stack_frame&) = delete;
    stack_frame(stack_frame&&) = default;

    /** Default assignments. */
    stack_frame& operator=(const stack_frame&) = delete;
    stack_frame& operator=(stack_frame&&) = delete;

    /**
     * Construct a stack frame.
     *
     * @param string_table Reference to the module string table.
     * @param locals_size Size to allocate for the locals.
     */
    stack_frame(const std::vector<std::string>& string_table, std::size_t locals_size)
    : string_table{string_table}
    , locals{locals_size}
    {
    }
};

/** Interpreter context. */
class context
{
    /** Loaded modules as `(name, decoded_module)`. */
    std::unordered_map<std::string, std::unique_ptr<language_module>> module_map;

    /** Functions, ordered by module and name. */
    std::unordered_map<std::string, std::unordered_map<std::string, function>> function_map;

    /** Native functions, ordered by module and name. */
    std::unordered_map<std::string, std::unordered_map<std::string, std::function<void(operand_stack&)>>> native_function_map;

    /** File manager reference. */
    file_manager& file_mgr;

    /**
     * Decode a module.
     *
     * @param mod The module.
     * @returns The decoded module.
     */
    std::unique_ptr<language_module> decode(const language_module& mod);

    /**
     * Decode the function's arguments and locals.
     *
     * @param desc The function descriptor.
     */
    void decode_locals(function_descriptor& desc) const;

    /**
     * Decode an instruction.
     *
     * @param mod The decoded module.
     * @param ar The archive to read from.
     * @param instr The instruction to decode.
     * @param details The function's details.
     * @param code Buffer to write the decoded bytes into.
     */
    void decode_instruction(language_module& mod,
                            archive& ar,
                            std::byte instr,
                            const function_details& details,
                            std::vector<std::byte>& code) const;

    /**
     * Execute a function.
     *
     * @param mod The decoded module.
     * @param f The function to execute.
     * @param args The function's arguments.
     * @return The function's return value.
     */
    value exec(const language_module& mod,
               const function& f,
               std::vector<value> args);

    /**
     * Execute a function.
     *
     * @param mod The module.
     * @param entry_point The function's entry point/offset in the binary buffer.
     * @param size The function's bytecode size.
     * @param frame The stack frame for the function.
     * @return THe function's return opcode.
     */
    opcode exec(const language_module& mod,
                std::size_t entry_point,
                std::size_t size,
                stack_frame& frame);

public:
    /** Default constructors. */
    context() = delete;
    context(const context&) = default;
    context(context&&) = default;

    /** Default assignments. */
    context& operator=(const context&) = delete;
    context& operator=(context&&) = delete;

    /**
     * Construct an interpreter context.
     *
     * @param file_mgr The file manager to use for module resolution.
     */
    context(file_manager& file_mgr)
    : file_mgr{file_mgr}
    {
    }

    /**
     * Register a native function to a module.
     *
     * @param mod_name The name of the module to bind to.
     * @param fn_name The function's name.
     * @param func The function.
     * @throws Throws a `codegen_error` if the function given by `mod_name` and `fn_name` is already registered.
     */
    void register_native_function(const std::string& mod_name, std::string fn_name, std::function<void(operand_stack&)> func);

    /**
     * Load a module.
     *
     * @param name The module name.
     * @param mod The module.
     */
    void load_module(const std::string& name, const language_module& mod);

    /**
     * Invoke a function from a module.
     *
     * @param module_name Name of the module of the function.
     * @param function_name The function's name.
     * @param args The function's arguments.
     * @returns The function's return value.
     */
    value invoke(const std::string& module_name, const std::string& function_name, std::vector<value> args);
};

}    // namespace slang::interpreter