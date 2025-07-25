import std;

macro sum! {
    () => {
        0;
    };

    ($a: expr) => {
        $a;
    };

    ($a: expr, $b: expr...) => {
        $a + sum!($b);
    };
}

fn ret_i32() -> i32 {
    return -123;
}

fn test_format() -> void {
    std::assert(
        std::string_equals(
            std::format!("Test"), 
            "Test"), 
        "Test");
    std::assert(
        std::string_equals(
            std::format!("{d} {d} {f} {s}", 12, 13, 3.141, "Test"),
            "12 13 3.141 Test"),
        "12 13 3.141 Test");

    let s: i32 = 42;
    std::assert(
        std::string_equals(
            std::format!("Test: {d}", s/2),
            "Test: 21"),
        "Test: 21");

    std::println(std::format!("{} test", 123));
    std::assert(
        std::string_equals(
            std::format!("{} test", 123),
            "123 test"
        ),
        "123 test");

    std::println(std::format!(" {} ", 3.141));
    std::assert(
        std::string_equals(
            std::format!(" {} ", 3.141),
            " 3.141 "
        ),
        "3.141");

    std::println(std::format!("str {}", "test"));
    std::assert(
        std::string_equals(
            std::format!("str {}", "test"),
            "str test"
        ),
        "str test");

    std::println(std::format!("sum {}", sum!(1, 2)));
    std::assert(
        std::string_equals(
            std::format!("sum {}", sum!(1, 2)),
            "sum 3"
        ),
        "sum 3");

    std::println(std::format!("{}", ret_i32() + sum!(1, 2)));
    std::assert(
        std::string_equals(
            std::format!("{}", ret_i32() + sum!(1, 2)),
            "-120"
        ),
        "-120");

    std::assert(
        std::string_equals(
            std::format!("{} {} {} {}", 12, 13, 3.141, "Test"),
            "12 13 3.141 Test"),
        "12 13 3.141 Test");
}

fn test_macro() -> void {
    let s1: i32 = sum!();
    std::assert(s1 == 0, "s1 == 0");

    let s2: i32 = sum!(1);
    std::assert(s2 == 1, "s2 == 1");

    let s3: i32 = sum!(1, 2);
    std::assert(s3 == 3, "s3 == 3");

    let s4: i32 = sum!(1, 2, 3);
    std::assert(s4 == 6, "s4 == 6");

    let s5: i32 = sum!(1, 1, sum!(1, 1), 1, 1);
    std::assert(s5 == 6, "s5 == 6");
}

fn main(args: [str]) -> i32
{
    test_format();
    test_macro();
    return 0;
}
