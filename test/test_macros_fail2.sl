macro sum! {
    () => {
        0;
    };

    ($a: expr) => {
        $a;
    };

    ($b: expr) => { // generates same matching score as previous branch
        $b;
    };
}

fn main(args: [str]) -> i32
{
    let s: i32 = sum!(1);
    return 0;
}
