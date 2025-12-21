import std;

macro pi_2! {
    () => {
        std::PI / 2.0;
    };
}

const CONST: i32 = 123;

macro o! {
    () => {
        CONST;
    };
}

macro p! {
    () => {
        -CONST;
    };
}

macro q! {
    () => {
        let CONST: i32 = 1;
        CONST;
    };
}

fn main(args: str[]) -> i32
{
    std::println(std::format!("pi_2: {f}", pi_2!()));
    
    let x: i32 = o!() + p!();
    std::assert(x == 0, "x == 0");

    let y: i32 = q!();
    std::assert(y == 1, "y == 1");

    return 0;
}
