import std;
import test_macros;

macro two! {
    () => {
        test_macros::sum!(1, 1);
    };
}

macro test! {
    () => {
        test_macros::test_format();
    };
}

fn main(args: [str]) -> i32 {
    let s1: i32 = test_macros::sum!();
    std::assert(s1 == 0, "s1 == 0");

    let s2: i32 = test_macros::sum!(1);
    std::assert(s2 == 1, "s2 == 1");

    let s3: i32 = test_macros::sum!(1, 2);
    std::assert(s3 == 3, "s3 == 3");

    let s4: i32 = test_macros::sum!(1, 2, 3);
    std::assert(s4 == 6, "s4 == 6");

    let t: i32 = two!();
    std::assert(t == 2, "t == 2");

    return 0;
}
