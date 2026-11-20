import std;

/*
 * struct creation and member access.
 */
struct S {
    c: i8,
    d: i8,
    i: i32,
    j: f32
};

fn init_struct(i: i32, j: f32) -> S {
    // omits j
    return S {
        c: 23i8,
        d: 100i8,
        i: i
    };
}
