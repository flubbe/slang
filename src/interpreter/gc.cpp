/**
 * slang - a simple scripting language.
 *
 * garbage collector.
 *
 * \author Felix Lubbe
 * \copyright Copyright (c) 2025
 * \license Distributed under the MIT software license (see accompanying LICENSE.txt).
 */

#include <algorithm>
#include <format>
#include <print>

#include "vector.h"
#include "gc.h"

#ifdef GC_DEBUG
#    define GC_LOG(...) std::print("GC: {}\n", std::format(__VA_ARGS__))
#else
#    define GC_LOG(...)
#endif

namespace si = slang::interpreter;

namespace slang::gc
{

void garbage_collector::mark_object(void* obj)
{
    auto it = objects.find(obj);
    if(it == objects.end())
    {
        auto it = persistent_objects.find(obj);
        if(it == persistent_objects.end())
        {
            GC_LOG("mark_object: object {} not part of GC set", obj);

            // nothing to mark.
            return;
        }

        if(it->second.reference_count == 0)
        {
            GC_LOG("mark_object {}\n", obj);
            throw gc_error("Cannot mark object: Reference count is zero.");
        }

        if(it->second.layout == nullptr)
        {
            GC_LOG("mark_object {}\n", obj);
            throw gc_error("Cannot mark object: Missing layout information.");
        }

        GC_LOG("mark_object {}: object layout", obj);
        void* mark_obj = *reinterpret_cast<void**>(static_cast<std::byte*>(obj));    // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast)
        for(auto offset: *it->second.layout)
        {
            mark_object(*reinterpret_cast<void**>(static_cast<std::byte*>(mark_obj) + offset));    // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast,cppcoreguidelines-pro-bounds-pointer-arithmetic)
        }

        return;
    }

    gc_object& obj_info = it->second;
    if((obj_info.flags & gc_object::of_reachable) != 0)
    {
        GC_LOG("mark_object: object {} unreachable", obj);

        return;
    }

    GC_LOG("mark_object {}", obj);
    obj_info.flags |= gc_object::of_reachable;

    if(obj_info.type == gc_object_type::array_str || obj_info.type == gc_object_type::array_aref)
    {
        auto* array = static_cast<si::fixed_vector<void*>*>(obj_info.addr);
        for(void*& ref: *array)
        {
            mark_object(ref);
        }
    }
    else if(obj_info.type == gc_object_type::obj)
    {
        if(obj_info.layout == nullptr)
        {
            throw gc_error("Cannot mark object: Missing layout information.");
        }

        GC_LOG("mark_object: object layout");
        for(auto offset: *obj_info.layout)
        {
            auto* obj = *reinterpret_cast<void**>(static_cast<std::byte*>(obj_info.addr) + offset);    // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast,cppcoreguidelines-pro-bounds-pointer-arithmetic)
            mark_object(obj);
        }
    }
}

/**
 * Delete an object and track the allocated bytes.
 *
 * @tparam T The object type.
 * @param obj The object to delete.
 * @param allocated_bytes The currently allocated bytes to by updated after the deletion.
 * @throws Throws a `gc_error` if the object size is larger than `allocated_bytes`.
 */
template<typename T>
void object_deleter(gsl::owner<void*> obj, std::size_t& allocated_bytes)
{
    delete static_cast<T*>(obj);

    if(sizeof(T) > allocated_bytes)
    {
        throw gc_error("Inconsistent allocation stats: sizeof(T) > allocated_bytes");
    }
    allocated_bytes -= sizeof(T);
}

void garbage_collector::delete_object(gc_object& obj_info)
{
    GC_LOG("delete_object {} (type {})", obj_info.addr, to_string(obj_info.type));

    if(obj_info.type == gc_object_type::str)
    {
        object_deleter<std::string>(obj_info.addr, allocated_bytes);
    }
    else if(obj_info.type == gc_object_type::obj)
    {
        ::operator delete(obj_info.addr, std::align_val_t{obj_info.alignment});

        if(obj_info.size > allocated_bytes)
        {
            throw gc_error("Inconsistent allocation stats: obj_info.size > allocated_bytes");
        }
        allocated_bytes -= obj_info.size;
    }
    else if(obj_info.type == gc_object_type::array_i32)
    {
        object_deleter<si::fixed_vector<std::int32_t>>(obj_info.addr, allocated_bytes);
    }
    else if(obj_info.type == gc_object_type::array_f32)
    {
        object_deleter<si::fixed_vector<float>>(obj_info.addr, allocated_bytes);
    }
    else if(obj_info.type == gc_object_type::array_str)
    {
        object_deleter<si::fixed_vector<std::string*>>(obj_info.addr, allocated_bytes);
    }
    else if(obj_info.type == gc_object_type::array_aref)
    {
        object_deleter<si::fixed_vector<void*>>(obj_info.addr, allocated_bytes);
    }
    else
    {
        throw gc_error(std::format("Invalid type '{}' for GC array.", static_cast<std::size_t>(obj_info.type)));
    }
}

void* garbage_collector::add_root(void* obj)
{
    GC_LOG("add root {}", obj);

    if(obj == nullptr)
    {
        throw gc_error("Cannot add nullptr to root set.");
    }

    auto it = root_set.find(obj);
    if(it == root_set.end())
    {
        root_set.insert({obj, 1});
    }
    else
    {
        ++it->second;
    }

    return obj;
}

void garbage_collector::remove_root(void* obj)
{
    GC_LOG("remove_root {}", obj);

    auto it = root_set.find(obj);
    if(it == root_set.end())
    {
        throw gc_error(std::format("Cannot remove root for object at {}, since it does not exist in the GC root set.", obj));
    }

    if(it->second == 0)
    {
        throw gc_error(std::format("Negative reference count for GC root {}", obj));
    }
    --it->second;

    GC_LOG("            ref_count {}", it->second);

    if(it->second == 0)
    {
        root_set.erase(it);
    }
}

void garbage_collector::run()
{
    GC_LOG("------- run -------");

    std::size_t object_set_size = objects.size();

    // collect roots.
    std::set<void*> current_root_set;
    for(auto& [obj, ref_count]: root_set)
    {
        current_root_set.insert(obj);
    }
    for(auto& [obj, props]: persistent_objects)
    {
        current_root_set.insert(obj);
    }
    for(auto& [obj, ref_count]: temporary_objects)
    {
        current_root_set.insert(obj);
    }

    // mark objects.
    for(void* obj: current_root_set)
    {
        mark_object(obj);
    }

    // free unreachable objects.
    for(auto& [obj, obj_info]: objects)
    {
        if((obj_info.flags & gc_object::of_reachable) != 0)
        {
            continue;
        }

        GC_LOG("collecting {}", obj);
        delete_object(obj_info);
    }

    for(auto it = objects.begin(); it != objects.end();)
    {
        if((it->second.flags & gc_object::of_reachable) == 0)
        {
            it = objects.erase(it);
        }
        else
        {
            ++it;
        }
    }

    for(auto& [obj, obj_info]: objects)
    {
        obj_info.flags &= ~gc_object::of_reachable;
    }

    if(object_set_size < objects.size())
    {
        throw gc_error(std::format("Object list grew during GC run: {} -> {}", object_set_size, objects.size()));
    }

#ifdef GC_DEBUG
    GC_LOG("run: {} -> {}, {} bytes allocated", object_set_size, objects.size(), allocated_bytes);
    GC_LOG("----- objects -----");
    for(auto& [obj, obj_info]: objects)
    {
        GC_LOG("     obj {}, type {}, flags {}", obj, to_string(obj_info.type), obj_info.flags);
    }
    GC_LOG("------ roots ------");
    for(auto& [obj, ref_count]: root_set)
    {
        GC_LOG("     obj {}, ref_count {}", obj, ref_count);
    }
    GC_LOG("--- temporaries ---");
    for(auto& [obj, ref_count]: temporary_objects)
    {
        GC_LOG("     obj {}, ref_count {}", obj, ref_count);
    }
    GC_LOG("-------------------");
#endif
}

void garbage_collector::reset()
{
#ifdef GC_DEBUG
    std::size_t object_count = objects.size();
#endif

    root_set.clear();
    temporary_objects.clear();

    for(auto& [obj, obj_info]: objects)
    {
        delete_object(obj_info);
    }
    objects.clear();

    GC_LOG("reset {} -> 0", object_count);
}

void* garbage_collector::add_persistent(void* obj, std::size_t layout_id)
{
    GC_LOG("add_persistent {} (layout id {})", obj, layout_id);

    if(obj == nullptr)
    {
        throw gc_error("Cannot add null object to persistent set.");
    }

    auto layout_it = type_layouts.find(layout_id);
    if(layout_it == type_layouts.end())
    {
        throw gc_error(std::format("No type for layout id {} registered.", layout_id));
    }

    auto it = persistent_objects.find(obj);
    if(it == persistent_objects.end())
    {
        persistent_objects.insert({obj, gc_persistent_object{&layout_it->second.second, 1}});
    }
    else
    {
        ++persistent_objects[obj].reference_count;
    }

    return obj;
}

void garbage_collector::remove_persistent(void* obj)
{
    GC_LOG("remove_persistent {}", obj);

    auto it = persistent_objects.find(obj);
    if(it == persistent_objects.end())
    {
        throw gc_error(std::format("Reference at {} does not exist in GC persistent object set.", obj));
    }

    --it->second.reference_count;
    if(it->second.reference_count == 0)
    {
        persistent_objects.erase(it);
    }
}

void* garbage_collector::add_temporary(void* obj)
{
    GC_LOG("add_temporary {}", obj);

    if(obj == nullptr)
    {
        return nullptr;
    }

    auto it = temporary_objects.find(obj);
    if(it == temporary_objects.end())
    {
        temporary_objects.insert({obj, 1});
    }
    else
    {
        ++it->second;
    }

    return obj;
}

void garbage_collector::remove_temporary(void* obj)
{
    GC_LOG("remove_temporary {}", obj);

    if(obj == nullptr)
    {
        return;
    }

    auto it = temporary_objects.find(obj);
    if(it == temporary_objects.end())
    {
        throw gc_error(std::format("Reference at {} does not exist in GC temporary object set.", obj));
    }

    if(it->second == 0)
    {
        throw gc_error(std::format("Temporary at {} has no references.", obj));
    }

    --it->second;
    if(it->second == 0)
    {
        temporary_objects.erase(it);
    }
}

gc_object_type garbage_collector::get_object_type(void* obj) const
{
    GC_LOG("get_object_type {}", obj);

    auto it = objects.find(obj);
    if(it == objects.end())
    {
        throw gc_error(std::format("Reference at {} does not exist in the GC object list.", obj));
    }

    return it->second.type;
}

std::size_t garbage_collector::register_type_layout(std::string name, std::vector<std::size_t> layout)
{
    // check if the layout already exists
    auto it = std::ranges::find_if(
      std::as_const(type_layouts),
      [&name](const std::pair<std::size_t, std::pair<std::string, std::vector<size_t>>>& t) -> bool
      {
          return t.second.first == name;
      });
    if(it != type_layouts.cend())
    {
        throw gc_error(std::format("Layout for type '{}' already registered.", name));
    }

    // find the first free identifier.
    std::size_t id = 0;
    for(; id < type_layouts.size(); ++id)
    {
        if(!type_layouts.contains(id))
        {
            break;
        }
    }

    type_layouts.insert({id, std::make_pair(std::move(name), std::move(layout))});
    return id;
}

std::size_t garbage_collector::check_type_layout(
  const std::string& name,
  const std::vector<std::size_t>& layout) const
{
    // check if the layout already exists
    auto it = std::ranges::find_if(
      std::as_const(type_layouts),
      [&name](const std::pair<std::size_t, std::pair<std::string, std::vector<size_t>>>& t) -> bool
      {
          return t.second.first == name;
      });
    if(it == type_layouts.cend())
    {
        throw gc_error(std::format("Layout for type '{}' not found.", name));
    }

    if(it->second.second != layout)
    {
        throw gc_error(std::format("A different layout was already registered for type '{}'.", name));
    }

    return it->first;
}

std::size_t garbage_collector::get_type_layout_id(const std::string& name) const
{
    auto it = std::ranges::find_if(
      std::as_const(type_layouts),
      [name](const std::pair<std::size_t, std::pair<std::string, std::vector<std::size_t>>>& layout) -> bool
      {
          return layout.second.first == name;
      });
    if(it == type_layouts.cend())
    {
        throw gc_error(std::format("No type layout for type '{}' registered.", name));
    }

    return it->first;
}

std::size_t garbage_collector::get_type_layout_id(void* obj) const
{
    auto obj_it = objects.find(obj);
    if(obj_it == objects.end())
    {
        throw gc_error(std::format("Reference at {} does not exist in the GC object list.", obj));
    }

    for(const auto& it: type_layouts)
    {
        if(&it.second.second == obj_it->second.layout)
        {
            return it.first;
        }
    }

    throw gc_error(std::format("No type layout for type '{}' registered.", obj));
}

std::string garbage_collector::layout_to_string(std::size_t layout_id) const
{
    for(const auto& it: type_layouts)
    {
        if(it.first == layout_id)
        {
            return it.second.first;
        }
    }

    throw gc_error(std::format("No type layout for id {} registered.", layout_id));
}

}    // namespace slang::gc
