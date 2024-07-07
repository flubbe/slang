/**
 * slang - a simple scripting language.
 *
 * garbage collector.
 *
 * \author Felix Lubbe
 * \copyright Copyright (c) 2024
 * \license Distributed under the MIT software license (see accompanying LICENSE.txt).
 */

#include <fmt/core.h>

#include "gc.h"

#ifdef GC_DEBUG
#    define GC_LOG(...) fmt::print("GC: {}\n", fmt::format(__VA_ARGS__))
#else
#    define GC_LOG(...)
#endif

namespace slang::gc
{

void garbage_collector::mark_object(void* obj)
{
    if(objects.find(obj) == objects.end())
    {
        // nothing to mark.
        return;
    }

    gc_object& obj_info = objects[obj];
    if(obj_info.flags & (gc_object::of_never_collect | gc_object::of_reachable))
    {
        return;
    }

    GC_LOG("mark_object {}", obj);
    obj_info.flags |= gc_object::of_reachable;

    if(obj_info.type == gc_object_type::array_str)
    {
        auto array = static_cast<std::vector<std::string*>*>(obj_info.addr);
        for(std::string*& s: *array)
        {
            mark_object(static_cast<void*>(s));
        }
    }
    else if(obj_info.type == gc_object_type::array_aref)
    {
        auto array = static_cast<std::vector<void*>*>(obj_info.addr);
        for(void*& ref: *array)
        {
            mark_object(ref);
        }
    }
}

void garbage_collector::delete_object(gc_object& obj_info)
{
    GC_LOG("delete_object {}", obj_info.addr);

    if(obj_info.type == gc_object_type::str)
    {
        auto str = static_cast<std::string*>(obj_info.addr);
        delete str;
        if(sizeof(std::string) > allocated_bytes)
        {
            throw gc_error("Inconsistent allocation stats: sizeof(std::string) > allocated_bytes");
        }
        allocated_bytes -= sizeof(std::string);
    }
    else if(obj_info.type == gc_object_type::array_i32)
    {
        auto array = static_cast<std::vector<std::int32_t>*>(obj_info.addr);
        delete array;
        if(sizeof(std::vector<std::int32_t>) > allocated_bytes)
        {
            throw gc_error("Inconsistent allocation stats: sizeof(std::vector<std::int32_t>) > allocated_bytes");
        }
        allocated_bytes -= sizeof(std::vector<std::int32_t>);
    }
    else if(obj_info.type == gc_object_type::array_f32)
    {
        auto array = static_cast<std::vector<float>*>(obj_info.addr);
        delete array;
        if(sizeof(std::vector<float>) > allocated_bytes)
        {
            throw gc_error("Inconsistent allocation stats: sizeof(std::vector<float>) > allocated_bytes");
        }
        allocated_bytes -= sizeof(std::vector<float>);
    }
    else if(obj_info.type == gc_object_type::array_str)
    {
        auto array = static_cast<std::vector<std::string*>*>(obj_info.addr);
        delete array;
        if(sizeof(std::vector<std::string*>) > allocated_bytes)
        {
            throw gc_error("Inconsistent allocation stats: sizeof(std::vector<std::string*>) > allocated_bytes");
        }
        allocated_bytes -= sizeof(std::vector<std::string*>);
    }
    else if(obj_info.type == gc_object_type::array_aref)
    {
        auto array = static_cast<std::vector<void*>*>(obj_info.addr);
        delete array;
        if(sizeof(std::vector<void*>) > allocated_bytes)
        {
            throw gc_error("Inconsistent allocation stats: sizeof(std::vector<void*>) > allocated_bytes");
        }
        allocated_bytes -= sizeof(std::vector<void*>);
    }
    else
    {
        throw gc_error(fmt::format("Invalid type '{}' for GC array.", static_cast<std::size_t>(obj_info.type)));
    }
}

void garbage_collector::add_array_root(void* array, array_type type, std::uint32_t flags)
{
    GC_LOG("add_array {}, type {}, flags {}", array, to_string(type), flags);

    if(objects.find(array) == objects.end())
    {
        throw gc_error(fmt::format("Array at {} does not exists in GC object set.", array));
    }

    if(root_set.find(array) != root_set.end())
    {
        throw gc_error(fmt::format("Array at {} already exists in GC root set.", array));
    }

    if(type == array_type::i32)
    {
        root_set.insert({array, 1});
    }
    else if(type == array_type::f32)
    {
        root_set.insert({array, 1});
    }
    else if(type == array_type::str)
    {
        root_set.insert({array, 1});
    }
    else if(type == array_type::ref)
    {
        root_set.insert({array, 1});
    }
    else
    {
        throw gc_error(fmt::format("Invalid type '{}' for GC array.", static_cast<std::size_t>(type)));
    }
}

void garbage_collector::set_flags(void* obj, std::uint32_t flags, bool propagate)
{
    GC_LOG("set_flags {}, flags {}, propagate {}", obj, flags, propagate);

    auto it = objects.find(static_cast<void*>(obj));
    if(it == objects.end())
    {
        throw gc_error(fmt::format("Cannot set flags for object at {}, since it is not in the GC object set.", static_cast<void*>(obj)));
    }

    if(it->second.flags & gc_object::gc_flags::of_visited)
    {
        return;
    }

    it->second.flags |= flags;

    // propagate flags.
    if(propagate)
    {
        it->second.flags |= gc_object::gc_flags::of_visited;
        if(it->second.type == gc_object_type::array_str)
        {
            auto array = static_cast<std::vector<std::string*>*>(obj);
            for(std::string*& s: *array)
            {
                set_flags(s, flags, true);
            }
        }
        else if(it->second.type == gc_object_type::array_aref)
        {
            auto array = static_cast<std::vector<void*>*>(obj);
            for(void*& obj: *array)
            {
                set_flags(obj, flags, true);
            }
        }
        it->second.flags &= ~gc_object::gc_flags::of_visited;
    }
}

void garbage_collector::clear_flags(void* obj, std::uint32_t flags, bool propagate)
{
    GC_LOG("clear_flags {}, flags {}, propagate {}", obj, flags, propagate);

    auto it = objects.find(static_cast<void*>(obj));
    if(it == objects.end())
    {
        throw gc_error(fmt::format("Cannot clear flags for object at {}, since it is not in the GC object set.", static_cast<void*>(obj)));
    }

    if(it->second.flags & gc_object::gc_flags::of_visited)
    {
        return;
    }

    it->second.flags &= ~flags;

    // propagate flags.
    if(propagate)
    {
        it->second.flags |= gc_object::gc_flags::of_visited;
        if(it->second.type == gc_object_type::array_str)
        {
            auto array = static_cast<std::vector<std::string*>*>(obj);
            for(std::string*& s: *array)
            {
                clear_flags(s, flags, true);
            }
        }
        else if(it->second.type == gc_object_type::array_aref)
        {
            auto array = static_cast<std::vector<void*>*>(obj);
            for(void*& obj: *array)
            {
                clear_flags(obj, flags, true);
            }
        }
        it->second.flags &= ~gc_object::gc_flags::of_visited;
    }
}

void* garbage_collector::add_root(void* obj, std::uint32_t flags)
{
    GC_LOG("add root {}, flags {}", obj, flags);

    if(root_set.find(obj) != root_set.end())
    {
        throw gc_error(fmt::format("String at {} already exists in GC root set.", obj));
    }
    root_set.insert({obj, 1});
    return obj;
}

std::string* garbage_collector::add_root(std::string* s, std::uint32_t flags)
{
    return static_cast<std::string*>(add_root(static_cast<void*>(s), flags));
}

void garbage_collector::remove_root(void* obj)
{
    GC_LOG("remove_root {}", obj);

    auto it = root_set.find(obj);
    if(it == root_set.end())
    {
        throw gc_error(fmt::format("Cannot remove root for object at {}, since it does not exist in the GC root set.", obj));
    }

    if(it->second == 0)
    {
        throw gc_error(fmt::format("Negative reference count for GC root {}", obj));
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
    std::size_t object_set_size = objects.size();

    // collect roots.
    std::set<void*> current_root_set;
    for(auto& [obj, ref_count]: root_set)
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
    constexpr std::uint32_t skip_flags = gc_object::of_reachable | gc_object::of_never_collect;

    for(auto& [obj, obj_info]: objects)
    {
        if(obj_info.flags & skip_flags)
        {
            continue;
        }

        GC_LOG("collecting {}", obj);
        delete_object(obj_info);
    }

    for(auto it = objects.begin(); it != objects.end();)
    {
        if(!(it->second.flags & skip_flags))
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
        throw gc_error(fmt::format("Object list grew during GC run: {} -> {}", object_set_size, objects.size()));
    }

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
}

void garbage_collector::reset()
{
    GC_LOG("reset");

    // delete all.
    root_set.clear();
    temporary_objects.clear();
    run();
}

void* garbage_collector::add_temporary(void* obj)
{
    GC_LOG("add_temporary {}", obj);

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

    auto it = temporary_objects.find(obj);
    if(it == temporary_objects.end())
    {
        throw gc_error(fmt::format("Reference at {} does not exists in GC temporary object set.", obj));
    }

    if(it->second == 0)
    {
        throw gc_error(fmt::format("Temporary at {} has no references.", obj));
    }

    --it->second;
    if(it->second == 0)
    {
        temporary_objects.erase(it);
    }
}

void garbage_collector::explicit_delete(void* obj, bool recursive)
{
    GC_LOG("explicit_delete {}, recursive {}", obj, recursive);

    auto it = objects.find(obj);
    if(it == objects.end())
    {
        throw gc_error(fmt::format("Reference at {} does not exists in GC object set.", obj));
    }

    if(!(it->second.flags & gc_object::of_never_collect))
    {
        throw gc_error(fmt::format("Reference at {} must be marked as gc_object::of_never_collect for explicit deletion.", obj));
    }

    auto obj_info = it->second;

    // remove object from root set
    if(is_root(obj))
    {
        root_set.erase(obj);
    }

    // remove object from temporaries set
    if(is_temporary(obj))
    {
        temporary_objects.erase(obj);
    }

    // remove object from map
    objects.erase(obj);

    // delete object
    delete_object(obj_info);
}

}    // namespace slang::gc
