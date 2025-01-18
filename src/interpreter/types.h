/**
 * slang - a simple scripting language.
 *
 * interpreter type definitions.
 *
 * \author Felix Lubbe
 * \copyright Copyright (c) 2024
 * \license Distributed under the MIT software license (see accompanying LICENSE.txt).
 */

#pragma once

#include <any>
#include <cstring>
#include <string>
#include <exception>
#include <functional>
#include <vector>

#include "module.h"
#include "opcodes.h"
#include "vector.h"

namespace slang::interpreter
{

/** Entries of a stack trace. */
struct stack_trace_entry
{
    /** Name of the module containing the function. */
    std::string mod_name;

    /** Function entry point. */
    std::size_t entry_point;

    /** Offset. */
    std::size_t offset;

    /** Defaulted constructors. */
    stack_trace_entry() = default;
    stack_trace_entry(const stack_trace_entry&) = default;
    stack_trace_entry(stack_trace_entry&&) = default;

    /**
     * Initializing constructor.
     *
     * @param mod_name The module name.
     * @param entry_point Entry point of the function for which the stack is unwound.
     * @param offset Offset in the function for which the stack is unwound.
     */
    stack_trace_entry(std::string mod_name, std::size_t entry_point, std::size_t offset)
    : mod_name{std::move(mod_name)}
    , entry_point{entry_point}
    , offset{offset}
    {
    }

    /** Defaulted assignments. */
    stack_trace_entry& operator=(const stack_trace_entry&) = default;
    stack_trace_entry& operator=(stack_trace_entry&&) = default;
};

/** Interpreter error. */
class interpreter_error : public std::runtime_error
{
    /** Call stack. */
    std::vector<stack_trace_entry> stack_trace;

public:
    /**
     * Construct a `interpreter_error`.
     *
     * @param message The error message.
     */
    explicit interpreter_error(const std::string& message)
    : std::runtime_error{message}
    {
    }

    /**
     * Construct a `interpreter_error`.
     *
     * @param message The error message.
     * @param stack_trace A stack trace to be passed along.
     */
    interpreter_error(const std::string& message, const std::vector<stack_trace_entry>& stack_trace)
    : std::runtime_error{message}
    , stack_trace{stack_trace}
    {
    }

    /**
     * Add a stack trace entry.
     *
     * @param mod_name The module name.
     * @param entry_point Entry point of the function for which the stack is unwound.
     * @param offset Offset in the function for which the stack is unwound.
     */
    void add_stack_trace_entry(std::string module_name, std::size_t entry_point, std::size_t offset)
    {
        stack_trace.emplace_back(module_name, entry_point, offset);
    }

    /** Get the stack trace. */
    const std::vector<stack_trace_entry>& get_stack_trace() const
    {
        return stack_trace;
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
    explicit operand_stack(std::size_t max_size)
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
            throw interpreter_error("Stack underflow.");
        }

        if(stack.size() + 4 > max_size)
        {
            throw interpreter_error("Stack overflow.");
        }

        stack.insert(stack.end(), stack.end() - 4, stack.end());
    }

    /** Duplicate the top f32 on the stack. */
    void dup_f32()
    {
        if(stack.size() < 4)
        {
            throw interpreter_error("Stack underflow.");
        }

        if(stack.size() + 4 > max_size)
        {
            throw interpreter_error("Stack overflow.");
        }

        stack.insert(stack.end(), stack.end() - 4, stack.end());
    }

    /** Duplicate the top address on the stack. */
    void dup_addr()
    {
        if(stack.size() < sizeof(void*))
        {
            throw interpreter_error("Stack underflow.");
        }

        if(stack.size() + sizeof(void*) > max_size)
        {
            throw interpreter_error("Stack overflow.");
        }

        stack.insert(stack.end(), stack.end() - sizeof(void*), stack.end());
    }

    /** Duplicate a memory block of size `size1` and insert it at `size1 + size2` from the top. */
    void dup_x1(std::size_t size1, std::size_t size2)
    {
        if(stack.size() < size1 + size2)
        {
            throw interpreter_error("Invalid stack access.");
        }

        std::vector<std::uint8_t> copy = {stack.end() - size1, stack.end()};
        stack.insert(stack.end() - size1 - size2, copy.begin(), copy.end());
    }

    /** Duplicate a memory block of size `size1` and insert it at `size1 + size2 + size3` from the top. */
    void dup_x2(std::size_t size1, std::size_t size2, std::size_t size3)
    {
        if(stack.size() < size1 + size2 + size3)
        {
            throw interpreter_error("Invalid stack access.");
        }

        std::vector<std::uint8_t> copy = {stack.end() - size1, stack.end()};
        stack.insert(stack.end() - size1 - size2 - size3, copy.begin(), copy.end());
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

        std::int32_t i;
        std::memcpy(&i, &stack[stack.size() - 4], 4);
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

        float f;
        std::memcpy(&f, &stack[stack.size() - 4], 4);
        stack.resize(stack.size() - 4);
        return f;
    }

    /** Modify the top value on the stack in-place. */
    template<typename T, typename U>
    void modify_top(std::function<U(T)> func)
    {
        static_assert(sizeof(T) == sizeof(U));
        static_assert(std::is_scalar_v<T> && std::is_scalar_v<U>);

        if(stack.size() < sizeof(T))
        {
            throw interpreter_error("Stack underflow");
        }

        T v;
        std::memcpy(&v, &stack[stack.size() - sizeof(T)], sizeof(T));
        U u = func(v);
        std::memcpy(&stack[stack.size() - sizeof(U)], &u, sizeof(U));
    }

    /** Pop an address from the stack. */
    template<typename T>
    T* pop_addr()
    {
        if(stack.size() < sizeof(T*))
        {
            throw interpreter_error("Stack underflow.");
        }

        T* addr;
        std::memcpy(&addr, &stack[stack.size() - sizeof(T*)], sizeof(T*));
        stack.resize(stack.size() - sizeof(T*));
        return addr;
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
    module_::function_signature signature;

    /** Whether this is a native function. */
    bool native;

    /** Entry point (offset into binary) or function pointer for native functions. */
    std::variant<std::size_t, std::function<void(operand_stack&)>> entry_point_or_function;

    /** Bytecode size for interpreted functions. */
    std::size_t size;

    /** Return opcode. */
    opcode ret_opcode;

    /** Locals. Not serialized. */
    std::vector<module_::variable_descriptor> locals;

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
    function(module_::function_signature signature,
             std::size_t entry_point,
             std::size_t size,
             std::vector<module_::variable_descriptor> locals,
             std::size_t locals_size,
             std::size_t stack_size);

    /**
     * Construct a native function.
     *
     * @param signature The function's signature.
     * @param func An std::function.
     */
    function(module_::function_signature signature, std::function<void(operand_stack&)> func);

    /** Get the function signature. */
    const module_::function_signature& get_signature() const
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
    const std::vector<module_::variable_descriptor>& get_locals() const
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
};

/** A stack frame. */
struct stack_frame
{
    /** Constant table reference. */
    const std::vector<module_::constant_table_entry>& constants;

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
     * @param constants Reference to the module constant table.
     * @param locals_size Size to allocate for the locals.
     * @param stack_size The operand stack size.
     */
    stack_frame(const std::vector<module_::constant_table_entry>& constants,
                std::size_t locals_size,
                std::size_t stack_size)
    : constants{constants}
    , locals{locals_size}
    , stack{stack_size}
    {
    }
};

/** Type properties. */
struct type_properties
{
    /** Type flags. */
    std::size_t flags{0};

    /** Type size. */
    std::size_t size{0};

    /** Type alignment. */
    std::size_t alignment{0};

    /** Type layout id (always `0` for non-struct types). */
    std::size_t layout_id{0};
};

/** Type properties. */
struct field_properties
{
    /** Field size. */
    std::size_t size{0};

    /** Field offset. */
    std::size_t offset{0};

    /** Whether this is a garbage collected field. */
    bool needs_gc{false};
};

/**
 * Result and argument type.
 *
 * @note A `value` is implemented using `std::any`. This means methods accessing
 * the data potentially throws a `std::bad_any_cast` (that is, all methods except
 * `value::get_size` and `value::get_type`).
 */
class value
{
    /** The stored value. */
    std::any data;

    /** Size of the value, in bytes. */
    std::size_t size;

    /** Type identifier, as `(type_name, is_array)`. */
    std::pair<std::string, bool> type;

    /** Layout id. */
    std::optional<std::size_t> layout_id;

    /** Pointer to the create function for this value. */
    void (value::*unbound_create_function)(std::byte*) const = nullptr;

    /** Pointer to the destroy function for this value. */
    void (value::*unbound_destroy_function)(std::byte*) const = nullptr;

    /** Create this value in memory. */
    std::function<void(std::byte*)> create_function;

    /** Destroy this value in memory. */
    std::function<void(std::byte*)> destroy_function;

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
        std::memcpy(memory, std::any_cast<T>(&data), sizeof(T));
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
                        || std::is_floating_point_v<std::remove_cv_t<std::remove_reference_t<T>>>,
                      "Vector type must be an integer type or a floating point type.");

        // we need to convert `std::vector` to a `fixed_vector<T>*`.
        auto input_vec = std::any_cast<std::vector<T>>(&data);
        auto vec = new fixed_vector<T>(input_vec->size());

        for(std::size_t i = 0; i < input_vec->size(); ++i)
        {
            (*vec)[i] = (*input_vec)[i];
        }

        std::memcpy(memory, &vec, sizeof(fixed_vector<T>*));
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
                        || std::is_floating_point_v<std::remove_cv_t<std::remove_reference_t<T>>>,
                      "Vector type must be an integer type or a floating point type.");

        fixed_vector<T>* vec;
        std::memcpy(&vec, memory, sizeof(fixed_vector<T>*));
        delete vec;

        std::memset(memory, 0, sizeof(fixed_vector<T>*));
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
        std::memcpy(memory, &s, sizeof(std::string*));
    }

    /**
     * Delete a string reference from memory.
     *
     * @param memory The memory to delete the reference from.
     */
    void destroy_str(std::byte* memory) const
    {
        std::memset(memory, 0, sizeof(std::string*));
    }

    /**
     * Create an address in memory.
     *
     * @param memory The memory to write into.
     */
    void create_addr(std::byte* memory) const
    {
        // we can just copy the value.
        std::memcpy(memory, std::any_cast<void*>(&data), sizeof(void*));
    }

    /**
     * Delete an address from memory.
     *
     * @param memory The memory to delete the address from.
     * @returns Returns `sizeof(void*)`.
     */
    void destroy_addr(std::byte* memory) const
    {
        std::memset(memory, 0, sizeof(void*));
    }

    /**
     * Bind the value creation and destruction methods.
     *
     * @param new_unbound_create_function The new (unbound) value creation method. Can be `nullptr`.
     * @param new_unbound_destroy_function The new (unbound) value destruction method. Can be `nullptr`.
     */
    void bind(
      void (value::*new_unbound_create_function)(std::byte*) const,
      void (value::*new_unbound_destroy_function)(std::byte*) const)
    {
        unbound_create_function = new_unbound_create_function;
        unbound_destroy_function = new_unbound_destroy_function;

        if(unbound_create_function != nullptr)
        {
            create_function = std::bind(unbound_create_function, this, std::placeholders::_1);
        }
        else
        {
            create_function = [](std::byte*) {}; /* no-op */
        }

        if(unbound_destroy_function != nullptr)
        {
            destroy_function = std::bind(unbound_destroy_function, this, std::placeholders::_1);
        }
        else
        {
            destroy_function = [](std::byte*) {}; /* no-op */
        }
    }

public:
    /** Default constructor. */
    value() = default;

    /** Copy constructor. */
    value(const value& other)
    : data{other.data}
    , size{other.size}
    , type{other.type}
    , layout_id{other.layout_id}
    {
        bind(other.unbound_create_function, other.unbound_destroy_function);
    }

    /** Move constructor. */
    value(value&& other)
    : data{std::move(other.data)}
    , size{other.size}
    , type{std::move(other.type)}
    , layout_id{other.layout_id}
    {
        bind(other.unbound_create_function, other.unbound_destroy_function);
    }

    /** Copy assignment. */
    value& operator=(const value& other)
    {
        data = other.data;
        size = other.size;
        type = other.type;
        layout_id = other.layout_id;
        bind(other.unbound_create_function, other.unbound_destroy_function);
        return *this;
    }

    /** Move assignment. */
    value& operator=(value&& other)
    {
        data = std::move(other.data);
        size = other.size;
        type = std::move(other.type);
        layout_id = other.layout_id;
        bind(other.unbound_create_function, other.unbound_destroy_function);
        return *this;
    }

    /**
     * Construct a integer value.
     *
     * @param i The integer.
     */
    explicit value(int i)
    : data{i}
    , size{sizeof(std::int32_t)}
    , type{"i32", false}
    , layout_id{std::nullopt}
    {
        bind(&value::create_primitive_type<std::int32_t>, nullptr);
    }

    /**
     * Construct a floating point value.
     *
     * @param f The floating point value.
     */
    explicit value(float f)
    : data{f}
    , size{sizeof(float)}
    , type{"f32", false}
    , layout_id{std::nullopt}
    {
        bind(&value::create_primitive_type<float>, nullptr);
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
    , size{sizeof(std::string*)}
    , type{"str", false}
    , layout_id{std::nullopt}
    {
        bind(&value::create_str, &value::destroy_str);
    }

    /**
     * Construct a string value.
     *
     * @param s The string.
     */
    explicit value(const char* s)
    : data{std::string{s}}
    , size{sizeof(std::string*)}
    , type{"str", false}
    , layout_id{std::nullopt}
    {
        bind(&value::create_str, &value::destroy_str);
    }

    /**
     * Construct an integer array value.
     *
     * @param int_vec The integers.
     */
    explicit value(std::vector<std::int32_t> int_vec);

    /**
     * Construct a floating point array value.
     *
     * @param float_vec The floating point values.
     */
    explicit value(std::vector<float> float_vec);

    /**
     * Construct a string array value.
     *
     * @note The strings are owned by this `value`. This is relevant when using `write`,
     * which writes a pointer to the specified memory address.
     *
     * @param string_vec The strings.
     */
    explicit value(std::vector<std::string> string_vec);

    /**
     * Construct a value from an object.
     *
     * @param layout_id The type's layout id.
     * @param obj The object.
     */
    explicit value(std::size_t layout_id, void* addr)
    : data{addr}
    , size{sizeof(void*)}
    , type{}
    , layout_id{layout_id}
    {
        bind(&value::create_addr, &value::destroy_addr);
    }

    /**
     * Construct a value from an object.
     *
     * @param type The object's type.
     * @param obj The object.
     */
    explicit value(std::pair<std::string, bool> type, void* addr)
    : data{addr}
    , size{sizeof(void*)}
    , type{std::move(type)}
    , layout_id{std::nullopt}
    {
        bind(&value::create_addr, &value::destroy_addr);
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

    /** Get the value type's layout id. */
    std::optional<std::size_t> get_layout_id() const
    {
        return layout_id;
    }

    /**
     * Access the data.
     *
     * @returns Retuens a pointer to the value. Returns `nullptr´ if the cast is invalid.
     */
    template<typename T>
    const T* get() const
    {
        return std::any_cast<T>(&data);
    }
};

template<>
inline void value::create_vector_type<std::string>(std::byte* memory) const
{
    // we need to convert `std::vector` to a `fixed_vector<T>*`.
    auto input_vec = std::any_cast<std::vector<std::string>>(&data);
    auto vec = new fixed_vector<std::string*>(input_vec->size());

    for(std::size_t i = 0; i < input_vec->size(); ++i)
    {
        (*vec)[i] = new std::string{(*input_vec)[i]};
    }

    std::memcpy(memory, &vec, sizeof(fixed_vector<std::string>*));
}

template<>
inline void value::destroy_vector_type<std::string>(std::byte* memory) const
{
    fixed_vector<std::string*>* vec;
    std::memcpy(&vec, memory, sizeof(fixed_vector<std::string*>*));

    for(auto& s: *vec)
    {
        delete s;
    }
    delete vec;

    std::memset(memory, 0, sizeof(fixed_vector<std::string*>*));
}

inline value::value(std::vector<std::int32_t> int_vec)
: data{std::move(int_vec)}
, size{sizeof(void*)}
, type{"i32", true}
, layout_id{std::nullopt}
{
    bind(&value::create_vector_type<std::int32_t>, &value::destroy_vector_type<std::int32_t>);
}

inline value::value(std::vector<float> float_vec)
: data{std::move(float_vec)}
, size{sizeof(void*)}
, type{"f32", true}
, layout_id{std::nullopt}
{
    bind(&value::create_vector_type<float>, &value::destroy_vector_type<float>);
}

inline value::value(std::vector<std::string> string_vec)
: data{std::move(string_vec)}
, size{sizeof(void*)}
, type{"str", true}
, layout_id{std::nullopt}
{
    bind(&value::create_vector_type<std::string>, &value::destroy_vector_type<std::string>);
}

}    // namespace slang::interpreter
