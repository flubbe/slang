import std;

fn test(i: i32, f: f32) -> i32
{
    return i + f as i32;
}

fn main(args: [str]) -> i32
{
    std::assert(test(1, -1.234) == 0, "test(1, -1.234) == 0");
    std::assert(-0.23 as i32 + 4 == 4, "-0.23 as i32 + 4 == 4");
    std::assert(-1 as f32 + 2.5 == 1.5, "-1 as f32 + 2.5 == 1.5");

    std::assert(std::parse_f32("10.1044") == 10.1044, "std::parse_f32");
    std::assert(std::parse_f32("-10.1044") == -10.1044, "std::parse_f32");

    std::assert(std::parse_i32("0") == 0, "std::parse_i32");
    std::assert(std::parse_i32("-12345") == -12345, "std::parse_i32");
    std::assert(std::parse_i32("234") == 234, "std::parse_i32");

    return 0;
}
