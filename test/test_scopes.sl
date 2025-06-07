import std;

const CONST: i32 = 123;

fn test() -> i32 {
    let CONST: i32 = 1;
    return CONST;
}

fn main(args: [str]) -> i32
{
    std::assert(test() == 1, "test() == 1");
    return 0;
}

