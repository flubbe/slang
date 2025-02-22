import test_const_export;
import test_structs;
import std;

struct T {
    l: test_structs::L
};

fn main(args: [str]) -> i32
{
    let s: test_structs::L = test_structs::create_node("Test");
    std::println(s.data);

    return (test_const_export::NEGATIVE_PI as i32) == -3;
}
