import std;

fn main(args: [str]) -> i32
{
    let s1: str = "Hello";
    let s2: str = "World";

    std::assert(std::string_equals("Hello", "Hello") == 1, "\"Hello\" == \"Hello\"");
    std::assert(std::string_equals("World", "Hello") == 0, "\"World\" != \"Hello\"");

    std::assert(std::string_equals(s1, "Hello"), "s1 == \"Hello\"");
    std::assert(std::string_equals(s2, "World"), "s2 == \"World\"");

    let s3: str = std::string_concat(std::string_concat(std::string_concat(s1, ", "), s2), "!");
    std::assert(std::string_equals(s3, "Hello, World!"), "s3 == \"Hello, World!\"");

    return 0;
}