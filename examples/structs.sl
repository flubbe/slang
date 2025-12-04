struct S 
{
    i: i32,
    j: f32
};

fn init(i: i32, j: f32) -> S 
{
    return S{i: i, j: j};
}

fn main(args: [str]) -> i32
{
    let s: S = init(2, 3.141 as f32);
    return s.i + (s.j as i32);
}
