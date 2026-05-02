import std;

macro test! {
    () => {
        1;
        2;
    };
}

fn main(args: str[]) -> i32 {
    let x: i32 = 1;
    x;
    1;
    -1;
    +2;
    1 == 1;
    x * 2;
    1 + 2 * x;

    std::assert(test!() == 2, "test!() == 2");

    return 0;
}
