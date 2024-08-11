/**
 * slang - a simple scripting language.
 *
 * interpreter.
 *
 * \author Felix Lubbe
 * \copyright Copyright (c) 2024
 * \license Distributed under the MIT software license (see accompanying LICENSE.txt).
 */

#include <any>
#include <functional>
#include <stdexcept>
#include <unordered_map>
#include <utility>
#include <variant>
#include <vector>

#include "filemanager.h"
#include "gc.h"
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

/**
 * Result and argument type.
 *
 * @note A `value` is implemented using `std::any`. This means methods accessing
 * the data potentially throw a `std::bad_any_cast` (that is, all methods except
 * `value::get_size` and `value::get_type`).
 */
class value
{
    /** The stored value. */
    std::any data;

    /** Create this value in memory. */
    std::function<void(std::byte*)> create_function;

    /** Destroy this value in memory. */
    std::function<void(std::byte*)> destroy_function;

    /** Size of the value, in bytes. */
    std::size_t size;

    /** Type identifier, as `(type_name, is_array)`. */
    std::pair<std::string, bool> type;

    /**
     * Create a primitive type value in memory.
     *
     * @param memory The memory to write into.
     */
    template<typename T>
    void create_primitive_type(std::byte* memory) const
    {
        static_assert(std::is_integral_v<std::remove_cv_t<std::remove_reference_t<T>>>
                        || std::is_floating_point_v<std::remove_cv_t<std::remove_reference_t<T>>>,
                      "Primitive type must be an integer or a floating point type.");

        // we can just copy the value.
        *reinterpret_cast<T*>(memory) = std::any_cast<T>(data);
    }

    /**
     * Create a vector of a primitive type or a string in memory.
     *
     * @param memory The memory to write into.
     */
    template<typename T>
    void create_vector_type(std::byte* memory) const
    {
        static_assert(std::is_integral_v<std::remove_cv_t<std::remove_reference_t<T>>>
                        || std::is_floating_point_v<std::remove_cv_t<std::remove_reference_t<T>>>
                        || std::is_same_v<std::remove_cv_t<std::remove_reference_t<T>>, std::string>,
                      "Vector type must be an integer type, a floating point type, or a string.");

        // we need to convert `std::vector` to a `fixed_vector<T>*`.
        auto input_vec = std::any_cast<std::vector<T>>(&data);
        auto vec = new fixed_vector<T>(input_vec->size());

        for(std::size_t i = 0; i < input_vec->size(); ++i)
        {
            (*vec)[i] = (*input_vec)[i];
        }

        *reinterpret_cast<const fixed_vector<T>**>(memory) = vec;
    }

    /**
     * Delete a vector type from memory.
     *
     * @param memory The memory to delete the vector type from.
     */
    template<typename T>
    void destroy_vector_type(std::byte* memory) const
    {
        static_assert(std::is_integral_v<std::remove_cv_t<std::remove_reference_t<T>>>
                        || std::is_floating_point_v<std::remove_cv_t<std::remove_reference_t<T>>>
                        || std::is_same_v<std::remove_cv_t<std::remove_reference_t<T>>, std::string>,
                      "Vector type must be an integer type, a floating point type, or a string.");

        auto vec = *reinterpret_cast<fixed_vector<T>**>(memory);
        delete vec;

        *reinterpret_cast<const fixed_vector<T>**>(memory) = nullptr;
    }

    /**
     * Create a string reference in memory.
     *
     * @note The string is owned by `v`.
     *
     * @param memory The memory to write into.
     */
    void create_str(std::byte* memory) const
    {
        // we can re-use the string memory managed by this class.
        const std::string* s = std::any_cast<std::string>(&data);
        if(!s)
        {
            throw std::bad_any_cast();
        }
        *reinterpret_cast<const std::string**>(memory) = s;
    }

    /**
     * Delete a string reference from memory.
     *
     * @param memory The memory to delete the reference from.
     */
    void destroy_str(std::byte* memory) const
    {
        *reinterpret_cast<const std::string**>(memory) = nullptr;
    }

    /**
     * Create an address in memory.
     *
     * @param memory The memory to write into.
     */
    void create_addr(std::byte* memory) const
    {
        // we can just copy the value.
        *reinterpret_cast<const void**>(memory) = std::any_cast<void*>(data);
    }

    /**
     * Delete an address from memory.
     *
     * @param memory The memory to delete the address from.
     * @returns Returns `sizeof(void*)`.
     */
    void destroy_addr(std::byte* memory) const
    {
        *reinterpret_cast<const void**>(memory) = nullptr;
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
    explicit value(int i)
    : data{i}
    , create_function{std::bind(&value::create_primitive_type<std::int32_t>, this, std::placeholders::_1)}
    , destroy_function{[](std::byte*) {}} /* no-op */
    , size{sizeof(std::int32_t)}
    , type{"i32", false}
    {
    }

    /**
     * Construct a floating point value.
     *
     * @param f The floating point value.
     */
    explicit value(float f)
    : data{f}
    , create_function{std::bind(&value::create_primitive_type<float>, this, std::placeholders::_1)}
    , destroy_function{[](std::byte*) {}} /* no-op */
    , size{sizeof(float)}
    , type{"f32", false}
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
    explicit value(std::string s)
    : data{std::move(s)}
    , create_function{std::bind(&value::create_str, this, std::placeholders::_1)}
    , destroy_function{std::bind(&value::destroy_str, this, std::placeholders::_1)}
    , size{sizeof(std::string*)}
    , type{"str", false}
    {
    }

    /**
     * Construct a string value.
     *
     * @param s The string.
     */
    explicit value(const char* s)
    : data{std::string{s}}
    , create_function{std::bind(&value::create_str, this, std::placeholders::_1)}
    , destroy_function{std::bind(&value::destroy_str, this, std::placeholders::_1)}
    , size{sizeof(std::string*)}
    , type{"str", false}
    {
    }

    /**
     * Construct an integer array value.
     *
     * @param int_vec The integers.
     */
    explicit value(std::vector<std::int32_t> int_vec)
    : data{std::move(int_vec)}
    , create_function{std::bind(&value::create_vector_type<std::int32_t>, this, std::placeholders::_1)}
    , destroy_function{std::bind(&value::destroy_vector_type<std::int32_t>, this, std::placeholders::_1)}
    , size{sizeof(void*)}
    , type{"i32", true}
    {
    }

    /**
     * Construct a floating point array value.
     *
     * @param float_vec The floating point values.
     */
    explicit value(std::vector<float> float_vec)
    : data{std::move(float_vec)}
    , create_function{std::bind(&value::create_vector_type<float>, this, std::placeholders::_1)}
    , destroy_function{std::bind(&value::destroy_vector_type<float>, this, std::placeholders::_1)}
    , size{sizeof(void*)}
    , type{"f32", true}
    {
    }

    /**
     * Construct a string array value.
     *
     * @note The strings are owned by this `value`. This is relevant when using `write`,
     * which writes a pointer to the specified memory address.
     *
     * @param string_vec The strings.
     */
    explicit value(std::vector<std::string> string_vec)
    : data{std::move(string_vec)}
    , create_function{std::bind(&value::create_vector_type<std::string>, this, std::placeholders::_1)}
    , destroy_function{std::bind(&value::destroy_vector_type<std::string>, this, std::placeholders::_1)}
    , size{sizeof(void*)}
    , type{"str", true}
    {
    }

    /**
     * Construct an address value.
     *
     * @param addr The address.
     */
    explicit value(void* addr)
    : data{addr}
    , create_function{std::bind(&value::create_addr, this, std::placeholders::_1)}
    , destroy_function{std::bind(&value::destroy_addr, this, std::placeholders::_1)}
    , size{sizeof(void*)}
    , type{"addr", false}
    {
    }

    /**
     * Create the value in memory.
     *
     * @param memory The memory to write to.
     * @returns Returns the value's size in bytes.
     */
    std::size_t create(std::byte* memory) const
    {
        create_function(memory);
        return size;
    }

    /**
     * Destroy a value.
     *
     * @param memory The memory to delete the value from.
     * @returns Returns the value's size in bytes.
     */
    std::size_t destroy(std::byte* memory) const
    {
        destroy_function(memory);
        return size;
    }

    /** Get the value's size. */
    std::size_t get_size() const
    {
        return size;
    }

    /** Get the value's type. */
    const std::pair<std::string, bool>& get_type() const
    {
        return type;
    }

    /** Access the data. */
    template<typename T>
    const T* get() const
    {
        return std::any_cast<T>(&data);
    }
};

/** Operand stack. */
class operand_stack
{
protected:
    /** The stack. */
    std::vector<std::uint8_t> stack;

    /** Maximal size. */
    std::size_t max_size;

public:
    /** Defaulted and deleted constructors. */
    operand_stack() = delete;
    operand_stack(const operand_stack&) = default;
    operand_stack(operand_stack&&) = default;

    /** Default assignments. */
    operand_stack& operator=(const operand_stack&) = default;
    operand_stack& operator=(operand_stack&&) = default;

    /**
     * Construct a stack with a maximal size.
     *
     * @param max_size The maximal stack size.
     */
    operand_stack(std::size_t max_size)
    : max_size{max_size}
    {
        stack.reserve(max_size);
    }

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

    /** Get the maximal stack size. */
    std::size_t get_max_size() const noexcept
    {
        return max_size;
    }

    /** Duplicate the top i32 on the stack. */
    void dup_i32()
    {
        if(stack.size() < 4)
        {
            throw interpreter_error("Stack underflow");
        }

        if(stack.size() + 4 > max_size)
        {
            throw interpreter_error("Stack overflow");
        }

        stack.insert(stack.end(), stack.end() - 4, stack.end());
    }

    /** Duplicate the top f32 on the stack. */
    void dup_f32()
    {
        if(stack.size() < 4)
        {
            throw interpreter_error("Stack underflow");
        }

        if(stack.size() + 4 > max_size)
        {
            throw interpreter_error("Stack overflow");
        }

        stack.insert(stack.end(), stack.end() - 4, stack.end());
    }

    /** Duplicate the top address on the stack. */
    void dup_addr()
    {
        if(stack.size() < sizeof(void*))
        {
            throw interpreter_error("Stack underflow");
        }

        if(stack.size() + sizeof(void*) > max_size)
        {
            throw interpreter_error("Stack overflow");
        }

        stack.insert(stack.end(), stack.end() - sizeof(void*), stack.end());
    }

    /**
     * Push an i32 onto the stack.
     *
     * @param i The integer.
     */
    void push_i32(std::int32_t i)
    {
        if(stack.size() + 4 > max_size)
        {
            throw interpreter_error("Stack overflow.");
        }
        stack.insert(stack.end(), reinterpret_cast<std::uint8_t*>(&i), reinterpret_cast<std::uint8_t*>(&i) + 4);
    }

    /**
     * Push an f32 onto the stack.
     *
     * @param f The floating-point value.
     */
    void push_f32(float f)
    {
        if(stack.size() + 4 > max_size)
        {
            throw interpreter_error("Stack overflow.");
        }
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
        if(stack.size() + sizeof(const T*) > max_size)
        {
            throw interpreter_error("Stack overflow.");
        }
        stack.insert(stack.end(), reinterpret_cast<std::uint8_t*>(&addr), reinterpret_cast<std::uint8_t*>(&addr) + sizeof(const T*));
    }

    /**
     * Push another stack onto this stack.
     *
     * @param other The other stack.
     */
    void push_stack(const operand_stack& other)
    {
        if(stack.size() + other.stack.size() > max_size)
        {
            throw interpreter_error("Stack overflow.");
        }
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

        return {};    // unreachable
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

/*
 * pop_result specializations.
 */

template<>
inline value operand_stack::pop_result<int>()
{
    return value{pop_i32()};
}

template<>
inline value operand_stack::pop_result<float>()
{
    return value{pop_f32()};
}

template<>
inline value operand_stack::pop_result<std::string>()
{
    throw std::runtime_error("operand_stack::pop_result<std::string> not implemented.");
}

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

    /** Returned array length. A length of `0` means "no array". */
    std::int64_t ret_array_length = 0;

    /** Locals. Not serialized. */
    std::vector<variable> locals;

    /** Argument and locals size. Not serialized. */
    std::size_t locals_size = 0;

    /** Stack size. Not serialized. */
    std::size_t stack_size = 0;

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
     * @param locals Local variables.
     * @param locals_size The arguments and locals size.
     * @param stack_size The operand stack size.
     */
    function(function_signature signature, std::size_t entry_point, std::size_t size, std::vector<variable> locals, std::size_t locals_size, std::size_t stack_size);

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

    /** Get the function's locals. */
    const std::vector<variable>& get_locals() const
    {
        return locals;
    }

    /** Get the local's size. */
    std::size_t get_locals_size() const
    {
        return locals_size;
    }

    /** Get the operand stack size. */
    std::size_t get_stack_size() const
    {
        return stack_size;
    }

    /** Get the return opcode. */
    opcode get_return_opcode() const
    {
        return ret_opcode;
    }

    /** Return whether the returned value is an array. */
    bool returns_array() const
    {
        return ret_array_length != 0;
    }

    /** Get the returned array's length. */
    std::int64_t get_returned_array_length() const
    {
        return ret_array_length;
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
     * @param stack_size The operand stack size.
     */
    stack_frame(const std::vector<std::string>& string_table, std::size_t locals_size, std::size_t stack_size)
    : string_table{string_table}
    , locals{locals_size}
    , stack{stack_size}
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

    /** Maximum call stack depth. */
    unsigned int max_call_stack_depth;

    /** The current call stack level. */
    unsigned int call_stack_level = 0;

    /** Garbage collector. */
    gc::garbage_collector gc;

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
    void decode_locals(function_descriptor& desc);

    /**
     * Decode an instruction.
     *
     * @param mod The decoded module.
     * @param ar The archive to read from.
     * @param instr The instruction to decode.
     * @param details The function's details.
     * @param code Buffer to write the decoded bytes into.
     * @return Delta (in bytes) by which the instruction changes the stack size.
     */
    std::int32_t decode_instruction(language_module& mod,
                                    archive& ar,
                                    std::byte instr,
                                    const function_details& details,
                                    std::vector<std::byte>& code) const;

    /**
     * Resolve a native function.
     *
     * @param name The function's name.
     * @param library_name The function's library name.
     * @returns Returns the resolved function.
     * @throws Throws an `interpreter_error` if the resolution failed.
     */
    std::function<void(operand_stack&)> resolve_native_function(const std::string& name, const std::string& library_name) const;

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
     * @param locals The locals for the function.
     * @param frame The stack frame for the function.
     * @return The function's return opcode.
     */
    opcode exec(const language_module& mod,
                std::size_t entry_point,
                std::size_t size,
                const std::vector<variable>& locals,
                stack_frame& frame);

public:
    /** Default constructors. */
    context() = delete;
    context(const context&) = delete;
    context(context&&) = default;

    /** Default assignments. */
    context& operator=(const context&) = delete;
    context& operator=(context&&) = delete;

    /**
     * Construct an interpreter context.
     *
     * @param file_mgr The file manager to use for module resolution.
     * @param max_call_stack_depth The maximum allowed function call stack depth.
     */
    context(file_manager& file_mgr, unsigned int max_call_stack_depth = 500)
    : file_mgr{file_mgr}
    , max_call_stack_depth{max_call_stack_depth}
    {
    }

    /** Default destructor. */
    virtual ~context() = default;

    /**
     * Register a native function to a module.
     *
     * @param mod_name The name of the module to bind to.
     * @param fn_name The function's name.
     * @param func The function.
     * @throws Throws a `interpreter_error` if the function given by `mod_name` and `fn_name` is already registered,
     *         or if the function `func` is `nullptr`.
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

    /** Reset the interpreter. Needs to be called when an exception was thrown and caught. */
    void reset()
    {
        call_stack_level = 0;
        gc.reset();
    }

    /** Garbage collector access. */
    gc::garbage_collector& get_gc()
    {
        return gc;
    }
};

}    // namespace slang::interpreter