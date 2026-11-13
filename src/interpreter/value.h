/**
 * slang - a simple scripting language.
 *
 * value type.
 *
 * \author Felix Lubbe
 * \copyright Copyright (c) 2026
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
    void (value::*create_function)(std::byte*) const = nullptr;

    /** Pointer to the destroy function for this value. */
    void (value::*destroy_function)(std::byte*) const = nullptr;

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
        const T* ptr = std::any_cast<T>(&data);
        if(!ptr)
        {
            throw std::bad_any_cast();
        }

        // we can just copy the value.
        std::memcpy(memory, ptr, sizeof(T));
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
        if(!input_vec)
        {
            throw std::bad_any_cast();
        }

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
        if(!input_vec)
        {
            throw std::bad_any_cast();
        }

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
        void* const* addr = std::any_cast<void*>(&data);
        if(!addr)
        {
            throw std::bad_any_cast();
        }

        // we can just copy the value.
        std::memcpy(memory, addr, sizeof(void*));
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

    /** Disallow public default construction. */
    value() = default;

public:
    /** Copy constructor. */
    value(const value& other)
    : data{other.data}
    , size{other.size}
    , type{other.type}
    , create_function{other.create_function}
    , destroy_function{other.destroy_function}
    {
    }

    /** Move constructor. */
    value(value&& other)
    : data{std::move(other.data)}
    , size{std::exchange(other.size, 0)}
    , type{std::move(other.type)}
    , create_function{std::exchange(other.create_function, nullptr)}
    , destroy_function{std::exchange(other.destroy_function, nullptr)}
    {
    }

    /** Copy assignment. */
    value& operator=(const value& other)
    {
        data = other.data;
        size = other.size;
        type = other.type;
        create_function = other.create_function;
        destroy_function = other.destroy_function;
        return *this;
    }

    /** Move assignment. */
    value& operator=(value&& other)
    {
        data = std::move(other.data);
        size = std::exchange(other.size, 0);
        type = std::move(other.type);
        create_function = std::exchange(other.create_function, nullptr);
        destroy_function = std::exchange(other.destroy_function, nullptr);
        return *this;
    }

    /** Create a 'void' value. */
    static value make_void()
    {
        value v;
        v.data = nullptr;
        v.size = 0;
        v.type = {"void"};
        return v;
    }

    /** Create a 'null' value. */
    static value make_null()
    {
        value v;
        v.data = nullptr;
        v.size = sizeof(nullptr_t);
        v.type = {"null"};
        return v;
    }

    /**
     * Create an 'i32' value.
     *
     * @param i The value.
     * @return Returns a 'i32' value.
     */
    static value make_i32(int i)
    {
        value v;
        v.data = i;
        v.size = sizeof(std::int32_t);
        v.type = {"i32"};
        v.create_function = &value::create_primitive_type<std::int32_t>;
        return v;
    }

    /**
     * Create an 'f32' value.
     *
     * @param f The value.
     * @return Returns a 'f32' value.
     */
    static value make_f32(float f)
    {
        value v;
        v.data = f;
        v.size = sizeof(float);
        v.type = {"f32"};
        v.create_function = &value::create_primitive_type<float>;
        return v;
    }

    /**
     * Create a string value.
     *
     * @note The string is owned by this `value`. This is relevant when using `write`,
     *     which writes a pointer to the specified memory address.
     *
     * @param s The string.
     * @return Returns a string value.
     */
    static value make_string(std::string s)
    {
        value v;
        v.data = std::move(s);
        v.size = sizeof(std::string*);
        v.type = {"str"};
        v.create_function = &value::create_str;
        v.destroy_function = &value::destroy_str;
        return v;
    }

    /**
     * Create a string value.
     *
     * @param s The string.
     * @return Returns a string value.
     */
    static value make_string(const char* s)
    {
        value v;
        v.data = std::string{s};
        v.size = sizeof(std::string*);
        v.type = {"str"};
        v.create_function = &value::create_str;
        v.destroy_function = &value::destroy_str;
        return v;
    }

    /**
     * Create an integer array value.
     *
     * @param int_vec The integers.
     * @return Returns a string value.
     */
    static value make_i32_array(std::vector<std::int32_t> int_vec);

    /**
     * Create a floating point array value.
     *
     * @param float_vec The floating point values.
     * @return Returns a string value.
     */
    static value make_f32_array(std::vector<float> float_vec);

    /**
     * Construct a string array value.
     *
     * @note The strings are owned by this `value`. This is relevant when using `write`,
     * which writes a pointer to the specified memory address.
     *
     * @param string_vec The strings.
     */
    static value make_string_array(std::vector<std::string> string_vec);

    /**
     * Construct a value from an object reference.
     *
     * @param layout_id The type's layout id.
     * @param obj The object's address.
     */
    static value make_reference(std::size_t layout_id, void* addr)
    {
        value v;
        v.data = addr;
        v.size = sizeof(void*);
        v.type = {"ref", std::nullopt, layout_id, std::nullopt};
        v.create_function = &value::create_addr;
        v.destroy_function = &value::destroy_addr;
        return v;
    }

    /**
     * Create a value from an object reference.
     *
     * @param type The object's type.
     * @param obj The object's address.
     */
    static value make_reference(module_::variable_type type, void* addr)
    {
        value v;
        v.data = addr;
        v.size = sizeof(void*);
        v.type = {std::move(type)};
        v.create_function = &value::create_addr;
        v.destroy_function = &value::destroy_addr;
        return v;
    }

    /**
     * Create the value in memory.
     *
     * @param memory The memory to write to.
     * @returns Returns the value's size in bytes.
     */
    std::size_t create(std::byte* memory) const
    {
        if(create_function != nullptr)
        {
            (this->*create_function)(memory);
        }
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
        if(destroy_function != nullptr)
        {
            (this->*destroy_function)(memory);
        }
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
     * @returns Returns a pointer to the value. Returns `nullptr´ if the cast is invalid.
     */
    template<typename T>
    const T* get() const
    {
        return std::any_cast<T>(&data);
    }
};

inline value value::make_i32_array(
  std::vector<std::int32_t> int_vec)
{
    value v;
    v.data = std::move(int_vec);
    v.size = sizeof(void*);
    v.type = {"i32", 1};
    v.create_function = &value::create_vector_type<std::int32_t>;
    v.destroy_function = &value::destroy_vector_type<std::int32_t>;
    return v;
}

inline value value::make_f32_array(
  std::vector<float> float_vec)
{
    value v;
    v.data = std::move(float_vec);
    v.size = sizeof(void*);
    v.type = {"f32", 1};
    v.create_function = &value::create_vector_type<float>;
    v.destroy_function = &value::destroy_vector_type<float>;
    return v;
}

inline value value::make_string_array(
  std::vector<std::string> string_vec)
{
    value v;
    v.data = std::move(string_vec);
    v.size = sizeof(void*);
    v.type = {"str", 1};
    v.create_function = &value::create_string_vector_type;
    v.destroy_function = &value::destroy_string_vector_type;
    return v;
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

          (
            result.emplace_back(
              [&]() -> value
              {
                  using type = std::remove_cvref_t<decltype(elems)>;

                  if constexpr(std::is_same_v<type, std::int32_t>)
                  {
                      return value::make_i32(std::forward<decltype(elems)>(elems));
                  }
                  else if constexpr(std::is_same_v<type, float>)
                  {
                      return value::make_f32(std::forward<decltype(elems)>(elems));
                  }
                  else if constexpr(std::is_same_v<type, std::string>)
                  {
                      return value::make_string(std::forward<decltype(elems)>(elems));
                  }
                  else if constexpr(std::is_same_v<type, std::vector<std::int32_t>>)
                  {
                      return value::make_i32_array(std::forward<decltype(elems)>(elems));
                  }
                  else if constexpr(std::is_same_v<type, std::vector<float>>)
                  {
                      return value::make_f32_array(std::forward<decltype(elems)>(elems));
                  }
                  else if constexpr(std::is_same_v<type, std::vector<std::string>>)
                  {
                      return value::make_string_array(std::forward<decltype(elems)>(elems));
                  }
                  else if constexpr(std::is_same_v<type, value>)
                  {
                      return std::forward<decltype(elems)>(elems);
                  }
                  else
                  {
                      static_assert(!sizeof(type), "Unsupported native argument type");
                  }
              }()),
            ...);

          return result;
      },
      std::forward<Tuple>(tuple));
}

}    // namespace slang::interpreter
