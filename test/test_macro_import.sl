import std;
import test_macros;

fn main(args: [str]) -> i32 {
    let s1: i32 = test_macros::sum!();
    std::assert(s1 == 0, "s1 == 0");

    return 0;
}
