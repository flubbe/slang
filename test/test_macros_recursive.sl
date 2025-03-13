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

fn main(args: [str]) -> i32
{
    std::assert(rec!() == 1, "rec!() == 1");
    std::assert(rec!(1) == 2, "rec!(1) == 2");
    std::assert(rec!(1, 2) == 4, "rec!(1, 2) == 4");
    return 0;
}
