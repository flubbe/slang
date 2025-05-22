import std;
import test_import;

fn main(args: [str]) -> i32 {
    std::assert(test_import::two!() == 2, "test_import::two!() == 2");

    test_import::test!();

    return 0;
}
