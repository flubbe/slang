import std;
import gc;

fn print_info() -> void
{
    std::println(
        std::format!(
            "object count:             {}",
            gc::object_count()));
    
    std::println(
        std::format!(
            "root set size:            {}",
            gc::root_set_size()));

    std::println(
        std::format!(
            "allocated bytes:          {}",
            gc::allocated_bytes()));

    std::println(
        std::format!(
            "allocated bytes since gc: {}",
            gc::allocated_bytes_since_gc()));

    std::println(
        std::format!(
            "min threshold:            {}",
            gc::min_threshold_bytes()));

    std::println(
        std::format!(
            "threshold:                {}",
            gc::threshold_bytes()));

    std::println(
        std::format!(
            "growth_factor:            {}",
            gc::growth_factor()));

    std::println(
        std::format!(
            "object count:             {}",
            gc::object_count()));
}

fn test_gc_run() -> void
{
    gc::run();
    std::assert(gc::object_count() == 0, "gc::object_count() == 0");

    std::println(
        std::format!(
            "object count (after):     {}",
            gc::object_count()));

    std::println(
        std::format!(
            "allocated bytes (after):  {}",
            gc::allocated_bytes()));
}

struct S{
    s: str
};

struct T{
    s: S
};

fn main(args: str[]) -> i32
{
    print_info();

    test_gc_run();

    let last_bytes: i32 = gc::allocated_bytes();
    let last_object_count: i32 = gc::object_count();

    let i: i32 = 0;
    let t: T;
    while(i < 500000) 
    {
        t = T{s:S{"Test"}};
        ++i;

        let cur_bytes: i32 = gc::allocated_bytes();
        let cur_object_count: i32 = gc::object_count();
        if(cur_bytes < last_bytes)
        {
            std::println(
                std::format!(
                    "GC iteration {}: {} -> {} objs ({} -> {} bytes)",
                    i,
                    last_object_count,
                    cur_object_count,
                    last_bytes,
                    cur_bytes));
        }
        last_bytes = cur_bytes;
        last_object_count = cur_object_count;
    }

    print_info();

    return 0;
}
