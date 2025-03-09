import std;

macro sum! {
    () => {
        return 0;
    };

    ($a: expr) => {
        return $a;
    };

    ($a: expr, $b: expr...) => {
        return $a + sum!($b);
    };
}

fn main(args: [str]) -> i32
{
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

    return 0;
}
