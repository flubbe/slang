/**
 * slang - a simple scripting language.
 *
 * value type.
 *
 * \author Felix Lubbe
 * \copyright Copyright (c) 2025
 * \license Distributed under the MIT software license (see accompanying LICENSE.txt).
 */

#pragma once

#include <any>

#include "vector.h"

namespace slang::interpreter
{

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
    std::size_t size{0};

    /** Type identifier. */
    module_::variable_type type;

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
        requires(std::is_integral_v<std::remove_cv_t<std::remove_reference_t<T>>>
                 || std::is_floating_point_v<std::remove_cv_t<std::remove_reference_t<T>>>)
    void create_primitive_type(std::byte* memory) const
    {
        // we can just copy the value.
        std::memcpy(memory, std::any_cast<T>(&data), sizeof(T));
    }

    /**
     * Create a vector of a primitive type.
     *
     * @param memory The memory to write into.
     */
    template<typename T>
        requires(std::is_integral_v<std::remove_cv_t<std::remove_reference_t<T>>>
                 || std::is_floating_point_v<std::remove_cv_t<std::remove_reference_t<T>>>)
    void create_vector_type(std::byte* memory) const
    {
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
     * Create a vector of a string in memory.
     *
     * @param memory The memory to write into.
     */
    void create_string_vector_type(std::byte* memory) const
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

    /**
     * Delete a vector of a primitive type.
     *
     * @param memory The memory to delete the vector from.
     */
    template<typename T>
        requires(std::is_integral_v<std::remove_cv_t<std::remove_reference_t<T>>>
                 || std::is_floating_point_v<std::remove_cv_t<std::remove_reference_t<T>>>)
    void destroy_vector_type(std::byte* memory) const
    {
        fixed_vector<T>* vec;
        std::memcpy(&vec, memory, sizeof(fixed_vector<T>*));
        delete vec;

        std::memset(memory, 0, sizeof(fixed_vector<T>*));
    }

    /**
     * Delete a vector of strings.
     *
     * @param memory The memory to delete the vector from.
     */
    void destroy_string_vector_type(std::byte* memory) const
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
    {
        bind(other.unbound_create_function, other.unbound_destroy_function);
    }

    /** Move constructor. */
    value(value&& other)
    : data{std::move(other.data)}
    , size{other.size}
    , type{std::move(other.type)}
    {
        bind(other.unbound_create_function, other.unbound_destroy_function);
    }

    /** Copy assignment. */
    value& operator=(const value& other)
    {
        data = other.data;
        size = other.size;
        type = other.type;
        bind(other.unbound_create_function, other.unbound_destroy_function);
        return *this;
    }

    /** Move assignment. */
    value& operator=(value&& other)
    {
        data = std::move(other.data);
        size = other.size;
        type = std::move(other.type);
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
    , type{"i32"}
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
    , type{"f32"}
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
    , type{"str"}
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
    , type{"str"}
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
    , type{"@addr", std::nullopt, layout_id, std::nullopt}
    {
        bind(&value::create_addr, &value::destroy_addr);
    }

    /**
     * Construct a value from an object.
     *
     * @param type The object's type.
     * @param obj The object.
     */
    explicit value(module_::variable_type type, void* addr)
    : data{addr}
    , size{sizeof(void*)}
    , type{std::move(type)}
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
    std::size_t get_size() const noexcept
    {
        return size;
    }

    /** Get the value's type. */
    const module_::variable_type& get_type() const noexcept
    {
        return type;
    }

    /** Get the value type's layout id. */
    std::optional<std::size_t> get_layout_id() const
    {
        return type.get_layout_id();
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

inline value::value(std::vector<std::int32_t> int_vec)
: data{std::move(int_vec)}
, size{sizeof(void*)}
, type{"i32", 1}
{
    bind(&value::create_vector_type<std::int32_t>, &value::destroy_vector_type<std::int32_t>);
}

inline value::value(std::vector<float> float_vec)
: data{std::move(float_vec)}
, size{sizeof(void*)}
, type{"f32", 1}
{
    bind(&value::create_vector_type<float>, &value::destroy_vector_type<float>);
}

inline value::value(std::vector<std::string> string_vec)
: data{std::move(string_vec)}
, size{sizeof(void*)}
, type{"str", 1}
{
    bind(&value::create_string_vector_type, &value::destroy_string_vector_type);
}

/**
 * Helper for moving a tuple into a `std::vector<value>`.
 *
 * @tparam Tuple Tuple type.
 * @param tuple The tuple.
 * @returns A `std::vector<value>` with the tuple's contents.
 */
template<typename Tuple>
std::vector<value> move_into_value_vector(Tuple&& tuple)
{
    return std::apply(
      [](auto&&... elems)
      {
          std::vector<value> result;
          result.reserve(sizeof...(elems));
          (result.emplace_back(value{std::move(elems)}), ...);
          return result;
      },
      std::forward<Tuple>(tuple));
}

}    // namespace slang::interpreter
