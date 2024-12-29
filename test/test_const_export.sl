import std;

// pi with 32 bit accuracy.
const PI: f32 = 3.1415927;

// negative pi with 32 bit accuracy
const NEGATIVE_PI: f32 = -3.1415927;

// constant evaluation test.
const T: i32 = 1 + 3 * 2;
const T_TEST: i32 = T == 7;
const S: i32 = 1 << 2;
const S_TEST: i32 = S == 4;

fn main(args: [str]) -> i32
{
    std::assert(T_TEST == 1, "T_TEST == 1");
    std::assert(S_TEST == 1, "S_TEST == 1");

    return 0;
}
