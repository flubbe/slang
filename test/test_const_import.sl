import std;

const PI_2: f32 = std::PI / 2.0;

fn main(args: [str]) -> i32
{
    std::println(std::f32_to_string(std::PI));
    std::println(std::f32_to_string(PI_2));
    return 0;
}
