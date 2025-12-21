macro sum! {
    () => {
        0;
    };
}

fn main(args: str[]) -> i32
{
    let s1: i32 = sum!(1);  // no matching macro branch
    return 0;
}
