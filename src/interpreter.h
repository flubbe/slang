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
using value = std::variant<int, float, std::string>;

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

public:
    /** Argument and locals size. Not serialized. */
    std::size_t locals_size = 0;

public:
    /** Default constructors. */
    function() = default;
    function(const function&) = default;
    function(function&&) = default;

    /** Default assignments. */
    function& operator=(const function&) = default;
    function& operator=(function&&) = default;

    /**
     * Construct a function.
     *
     * @param signature The function's signature.
     * @param entry_point The function's entry point, given as an offset into a module binary.
     * @param size The bytecode size.
     * @param locals_size The arguments and locals size.
     */
    function(function_signature signature, std::size_t entry_point, std::size_t size, std::size_t locals_size)
    : signature{std::move(signature)}
    , native{false}
    , entry_point_or_function{entry_point}
    , size{size}
    , locals_size{locals_size}
    {
    }

    /**
     * Construct a native function.
     *
     * @param signature The function's signature.
     * @param func An std::function.
     */
    function(function_signature signature, std::function<void(operand_stack&)> func)
    : signature{std::move(signature)}
    , native{true}
    , entry_point_or_function{std::move(func)}
    {
    }

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
               const std::vector<value>& args);

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
    value invoke(const std::string& module_name, const std::string& function_name, const std::vector<value>& args);
};

}    // namespace slang::interpreter