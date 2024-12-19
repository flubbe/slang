/**
 * slang - a simple scripting language.
 *
 * garbage collector.
 *
 * \author Felix Lubbe
 * \copyright Copyright (c) 2024
 * \license Distributed under the MIT software license (see accompanying LICENSE.txt).
 */

#pragma once

#include <set>
#include <unordered_map>

#include "vector.h"
#include "module.h"

namespace slang::gc
{

namespace si = slang::interpreter;

/*
 * Verify size assumptions for arrays.
 */

static_assert(sizeof(si::fixed_vector<std::int32_t>) == sizeof(void*));
static_assert(sizeof(si::fixed_vector<float>) == sizeof(void*));
static_assert(sizeof(si::fixed_vector<std::string*>) == sizeof(void*));
static_assert(sizeof(si::fixed_vector<void*>) == sizeof(void*));

/** Garbage collection error. */
class gc_error : public std::runtime_error
{
public:
    /**
     * Construct a `gc_error`.
     *
     * @param message The error message.
     */
    explicit gc_error(const std::string& message)
    : std::runtime_error{message}
    {
    }
};

/**
 * Garbage collector object type.
 */
enum class gc_object_type : std::uint8_t
{
    str,
    obj,
    array_i32,
    array_f32,
    array_str,
    array_aref,
};

/** Convert GC object type to string. */
inline std::string to_string(gc_object_type type)
{
    switch(type)
    {
    case gc_object_type::str: return "str";
    case gc_object_type::obj: return "obj";
    case gc_object_type::array_i32: return "array_i32";
    case gc_object_type::array_f32: return "array_f32";
    case gc_object_type::array_str: return "array_str";
    case gc_object_type::array_aref: return "array_aref";
    }

    return "unknown";
}

/** Garbage collector object. */
struct gc_object
{
    /** Flags. */
    enum gc_flags : std::uint8_t
    {
        of_none = 0,      /** No flags. */
        of_reachable = 1, /** Reachable from root set. */
        of_temporary = 2, /** A temporary object (i.e., not stored in a variable). */
    };

    /** Object type. */
    gc_object_type type;

    /**
     * Type layout (offsets of references inside this object).
     *
     * @note Not used for arrays, since we don't want to create a new
     * layout for arrays of a different sizes.
     */
    std::vector<std::size_t>* layout = nullptr;

    /** Object size. */
    std::size_t size;

    /** Memory alignment. */
    std::size_t alignment;

    /** Flags. */
    std::uint8_t flags{of_none};

    /** Object address. */
    void* addr{nullptr};

    /** Create an object from a type. */
    template<typename T>
    static gc_object from(
      [[maybe_unused]] T* obj,
      [[maybe_unused]] std::uint8_t flags = of_none,
      [[maybe_unused]] std::vector<std::size_t>* layout = nullptr)
    {
        static_assert(
          std::is_same_v<T, std::string>
            || std::is_same_v<T, si::fixed_vector<std::int32_t>>
            || std::is_same_v<T, si::fixed_vector<float>>
            || std::is_same_v<T, si::fixed_vector<std::string*>>
            || std::is_same_v<T, si::fixed_vector<void*>>,
          "Cannot create GC object from type.");

        return {};    // unreachable
    }

    /** Create an object with a given size. */
    static gc_object from(void* obj, std::size_t size, std::size_t alignment, std::uint8_t flags = of_none, std::vector<std::size_t>* layout = nullptr)
    {
        return {gc_object_type::obj, layout, size, alignment, flags, obj};
    }
};

template<>
inline gc_object gc_object::from<std::string>(
  std::string* obj, std::uint8_t flags, std::vector<std::size_t>* layout)
{
    if(layout != nullptr)
    {
        throw gc_error("Invalid function call: Tried to create string with a type layout.");
    }

    return {gc_object_type::str,
            nullptr,
            sizeof(std::string),
            std::alignment_of_v<std::string>,
            flags, obj};
}

template<>
inline gc_object gc_object::from<si::fixed_vector<std::int32_t>>(
  si::fixed_vector<std::int32_t>* obj, std::uint8_t flags, std::vector<std::size_t>* layout)
{
    if(layout != nullptr)
    {
        throw gc_error("Invalid function call: Tried to create i32 array with a type layout.");
    }

    return {gc_object_type::array_i32,
            nullptr,
            sizeof(si::fixed_vector<std::int32_t>),
            std::alignment_of_v<si::fixed_vector<std::int32_t>>,
            flags, obj};
}

template<>
inline gc_object gc_object::from<si::fixed_vector<float>>(
  si::fixed_vector<float>* obj, std::uint8_t flags, std::vector<std::size_t>* layout)
{
    if(layout != nullptr)
    {
        throw gc_error("Invalid function call: Tried to create f32 array with a type layout.");
    }

    return {gc_object_type::array_f32,
            nullptr,
            sizeof(si::fixed_vector<float>),
            std::alignment_of_v<si::fixed_vector<float>>,
            flags, obj};
}

template<>
inline gc_object gc_object::from<si::fixed_vector<std::string*>>(
  si::fixed_vector<std::string*>* obj, std::uint8_t flags, std::vector<std::size_t>* layout)
{
    if(layout != nullptr)
    {
        throw gc_error("Invalid function call: Tried to create string array with a type layout.");
    }

    return {gc_object_type::array_str,
            nullptr,
            sizeof(si::fixed_vector<std::string*>),
            std::alignment_of_v<si::fixed_vector<std::string*>>,
            flags, obj};
}

template<>
inline gc_object gc_object::from<si::fixed_vector<void*>>(
  si::fixed_vector<void*>* obj, std::uint8_t flags, std::vector<std::size_t>* layout)
{
    if(layout != nullptr)
    {
        throw gc_error("Invalid function call: Tried to create object array with a type layout.");
    }

    return {gc_object_type::array_aref,
            nullptr,
            sizeof(si::fixed_vector<void*>),
            std::alignment_of_v<si::fixed_vector<void*>>,
            flags, obj};
}

/** Garbage collector. */
class garbage_collector
{
    /** All allocated objects. */
    std::unordered_map<void*, gc_object> objects;

    /** Roots. */
    std::unordered_map<void*, std::size_t> root_set;

    /** Reference-counted temporary objects. */
    std::unordered_map<void*, std::size_t> temporary_objects;

    /** Allocated bytes. */
    std::size_t allocated_bytes{0};

    /** Type layouts. */
    std::unordered_map<std::size_t, std::pair<std::string, std::vector<std::size_t>>> type_layouts;

    /**
     * Mark object as reachable.
     *
     * @note No-op if `obj` is not in the object list.
     *
     * @param obj The object to mark.
     */
    void mark_object(void* obj);

    /**
     * Delete an object.
     *
     * @param obj_info Info for fhe object to delete.
     */
    void delete_object(gc_object& obj_info);

public:
    /** Defaulted and deleted constructors. */
    garbage_collector() = default;
    garbage_collector(const garbage_collector&) = delete;
    garbage_collector(garbage_collector&&) = default;

    /** Defaulted and deleted assignment operators. */
    garbage_collector& operator=(const garbage_collector&) = delete;
    garbage_collector& operator=(garbage_collector&&) = default;

    /** Destructor. */
    virtual ~garbage_collector()
    {
        reset();
    }

    /** Run garbage collector. */
    void run();

    /** Reset the garbage collector and free all allocated memory. */
    void reset();

    /**
     * Add an object to the root set.
     *
     * @param obj The object.
     * @returns Returns the input object.
     */
    void* add_root(void* obj);

    /**
     * Remove an object from the root set.
     *
     * @param obj The object.
     * @throws Throws a `gc_error` if the object is not a root.
     */
    void remove_root(void* obj);

    /**
     * Allocate a new garbage collected variable and optionally add it to the root set or temporary set.
     *
     * @param flags Flags.
     * @param add Whether to add the object to the root set or temporary set (depending on the flags).
     * @returns Returns a (pointer to a) garbage collected variable.
     */
    template<typename T>
    T* gc_new(std::uint32_t flags = gc_object::of_none, bool add = true)
    {
        allocated_bytes += sizeof(T);

        T* obj = new T;
        if(objects.find(obj) != objects.end())
        {
            throw gc_error("Allocated object already exists.");
        }
        objects.insert({obj, gc_object::from(obj, flags, nullptr)});

        if(add)
        {
            if(flags & gc_object::of_temporary)
            {
                return static_cast<T*>(add_temporary(obj));
            }
            return static_cast<T*>(add_root(obj));
        }

        return obj;
    }

    /**
     * Allocate a new garbage collected variable of a given size and alignment,
     * and optionally add it to the root set or temporary set.
     *
     * @param layout_id The type layout id, as returned by `register_layout`.
     * @param size Size of the object, in bytes.
     * @param alignment Byte-alignment of the object.
     * @param flags Flags.
     * @param add Whether to add the object to the root set or temporary set (depending on the flags).
     * @returns Returns a (pointer to a) garbage collected variable.
     */
    void* gc_new(std::size_t layout_id,
                 std::size_t size,
                 std::size_t alignment,
                 std::uint32_t flags = gc_object::of_none,
                 bool add = true)
    {
        allocated_bytes += size;

        void* obj = new(std::align_val_t{alignment}) std::byte[size]();    // This also performs zero-initialization.
        if(objects.find(obj) != objects.end())
        {
            throw gc_error("Allocated object already exists.");
        }
        auto layout_it = type_layouts.find(layout_id);
        if(layout_it == type_layouts.end())
        {
            throw gc_error("Tried to create object with unknown type layout index.");
        }
        // note: references to elements remain valid after insert/emplace (but may invalidate iterators).

        objects.insert({obj, gc_object::from(obj, size, alignment, flags, &layout_it->second.second)});

        if(add)
        {
            if(flags & gc_object::of_temporary)
            {
                return add_temporary(obj);
            }
            return add_root(obj);
        }

        return obj;
    }

    /**
     * Allocate a new garbage collected array and add it to the root set.
     *
     * @param size The array size.
     * @param flags Flags.
     * @returns Returns a (pointer to a) garbage collected array.
     */
    template<typename T>
    si::fixed_vector<T>* gc_new_array(std::size_t size, std::uint32_t flags = gc_object::of_none)
    {
        auto array = new si::fixed_vector<T>(size);
        assert(array->size() == size);
        objects.insert({array, gc_object::from(array, flags, nullptr)});

        allocated_bytes += sizeof(si::fixed_vector<T>);

        if(flags & gc_object::of_temporary)
        {
            return static_cast<si::fixed_vector<T>*>(add_temporary(array));
        }
        return static_cast<si::fixed_vector<T>*>(add_root(array));
    }

    /**
     * Add a temporary object. If the object already exists in the
     * temporary objects set, its reference count is increased.
     *
     * @param obj The object.
     * @returns The object.
     * @throws Throws a `gc_error` if the object is not found in the object list.
     */
    void* add_temporary(void* obj);

    /**
     * Remove a temporary object (or decrease it's reference count).
     *
     * @note This needs to be called on `string` and `array` types, that are
     *     1. passed to native functions, or
     *     2. returned from `invoke` to native code.
     * @note If `nullptr` is passed, the function is a no-op.
     *
     * @param obj The object.
     * @throws Throws a `gc_error` if the object is not found in the object list.
     */
    void remove_temporary(void* obj);

    /**
     * Check if an object is in the root set.
     *
     * @param obj The object.
     * @returns True if the object is found in the root set and false otherwise.
     */
    bool is_root(void* obj) const
    {
        return root_set.find(obj) != root_set.end();
    }

    /**
     * Check if an object is in the temporary set.
     *
     * @param obj The object.
     * @returns True if the object is found in the temporary set and false otherwise.
     */
    bool is_temporary(void* obj) const
    {
        return temporary_objects.find(obj) != temporary_objects.end();
    }

    /**
     * Return the object type.
     *
     * @param obj The object.
     * @returns The object type.
     * @throws Throws a `gc_error` if the object is not in the objects list.
     */
    gc_object_type get_object_type(void* obj) const;

    /**
     * Register a new type layout.
     *
     * @param name Name of the type.
     * @param layout The type layout, given as a list of offsets of pointers.
     * @returns Returns a layout identifier.
     */
    std::size_t register_type_layout(std::string name, std::vector<std::size_t> layout);

    /**
     * Get a type layout id from the type's name.
     *
     * @param name Name of the type.
     * @returns Returns the layout identifier.
     * @throws Throws a ´gc_error` if the name was not found.
     */
    std::size_t get_type_layout_id(const std::string& name) const;

    /** Get allocated object count. */
    std::size_t object_count() const
    {
        return objects.size();
    }

    /** Get the root set size. */
    std::size_t root_set_size() const
    {
        return root_set.size();
    }

    /** Get allocated bytes. */
    std::size_t byte_size() const
    {
        return allocated_bytes;
    }
};

}    // namespace slang::gc
