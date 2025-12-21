import std;
import test_const_export;
import test_structs;
import example_package::hello;

struct T {
    l: test_structs::L
};

fn main(args: str[]) -> i32
{
    // Test imported types and functions.
    let s: test_structs::L = test_structs::create_node("Test");
    std::assert(std::string_equals(s.data, "Test"), "s.data == \"Test\"");

    // Test imported constants.
    std::assert((test_const_export::NEGATIVE_PI as i32) == -3, "(test_const_export::NEGATIVE_PI as i32) == -3");

    // Function call.
    std::assert(std::string_equals(example_package::hello::return_hello(), "Hello"), "example_package::hello::return_hello() == \"Hello\"");

    return 0;
}
