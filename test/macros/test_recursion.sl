import std;

macro rec! {
    () => {
        1;
    };

    ($a: expr) => {
        rec!() + $a;
    };

    ($a: expr, $b: expr) => {
        rec!($a) + $b;
    };
}

macro rec2! {
    () => {
        rec!(1, 2);
    };
    
    ($a: expr) => {
        rec2!() + $a - 1;
    };
}

fn main(args: str[]) -> i32
{
    std::assert(rec!() == 1, "rec!() == 1");
    std::assert(rec!(1) == 2, "rec!(1) == 2");
    std::assert(rec!(1, 2) == 4, "rec!(1, 2) == 4");
    std::assert(rec!(rec!()) == 2, "rec!(rec!()) == 2");

    std::assert(rec2!() == 4, "rec2!() == 4");
    std::assert(rec2!(1) == 4, "rec2!() == 4");

    return 0;
}
