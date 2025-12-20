import std;
import gc;

fn main(args: [str]) -> i32
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
            "object count (before):    {}",
            gc::object_count()));

    gc::run();
    std::assert(gc::object_count() == 0, "gc::object_count() == 0");

    std::println(
        std::format!(
            "object count (after):     {}",
            gc::object_count()));

    return 0;
}