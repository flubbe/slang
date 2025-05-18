import std;
import test_macro_import;

fn main(args: [str]) -> i32 {
    std::assert(test_macro_import::two!() == 2, "test_macro_import::two!() == 2");

    let s: i32 = test_macros::sum!(1, 1); // should fail (implicit transitive import).

    return 0;
}
