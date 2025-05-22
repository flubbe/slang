macro var_decl! {
    () => {
        let s: i32 = 3;
    };
}

macro var_use! {
    () => {
        s = 3; // s is not declared in the macro.
    };
}

fn main(args: [str]) -> i32
{
    var_decl!();
    var_use!();

    return 0;
}
