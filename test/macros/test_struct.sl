struct T {
    i: i32
};

struct S {
    t: T
};

macro m! {
    () => {
        let t: T = T{12};
        t.i;
    };
}

macro n! {
    () => {
        let s: S = S{T{12}};
        s.t.i;
    };
}

fn main(args: str[]) -> i32
{
    return m!() + n!();
}
