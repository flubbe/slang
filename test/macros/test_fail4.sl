macro var_use! {
    () => {
        s = 3; // s is not declared in the macro.
    };
}

fn main(args: [str]) -> i32
{
    let s: i32 = 0;
    var_use!();

    return 0;
}
