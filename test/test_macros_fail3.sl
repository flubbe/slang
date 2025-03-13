macro var_decl! {
    () => {
        let s: i32 = 3;
    };
}

fn main(args: [str]) -> i32
{
    var_decl!();
    s = 0; // s is not declared

    return 0;
}
