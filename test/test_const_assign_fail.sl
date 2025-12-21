const C: i32 = 123;

fn main(args: str[]) -> i32
{
    C = C + 1; // assignment to constant should fail
    return 0;
}
