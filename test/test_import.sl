import test_const_export;

fn main(args: [str]) -> i32
{
    return (test_const_export::NEGATIVE_PI as i32) == -3;
}
