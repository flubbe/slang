import std;

const ABS: f32 = 1e-6 as f32;

fn approx_eq(value: f32, expected: f32, message: str) -> void
{
    std::assert(std::abs(value - expected) < ABS, message);
}

fn test_trigonometric_functions() -> void
{
    approx_eq(std::sin(0.0 as f32), 0.0 as f32, "sin(0) == 0");
    approx_eq(std::sin((std::PI/2.0) as f32), 1.0 as f32, "sin(PI/2) == 1");
    approx_eq(std::sin(std::PI as f32), 0.0 as f32, "sin(PI) == 0");
    approx_eq(std::sin((3.0*std::PI/2.0) as f32), -1.0 as f32, "sin(3*PI/2) == -1");

    approx_eq(std::cos(0.0 as f32), 1.0 as f32, "cos(0) == 1");
    approx_eq(std::cos((std::PI/2.0) as f32), 0.0 as f32, "cos(PI/2) == 0");
    approx_eq(std::cos(std::PI as f32), -1.0 as f32, "cos(PI) == -1");
    approx_eq(std::cos((3.0*std::PI/2.0) as f32), 0.0 as f32, "cos(3*PI/2) == 0");

    approx_eq(std::tan(0.0 as f32), 0.0 as f32, "tan(0) == 0");
    approx_eq(std::tan((std::PI/4.0) as f32), 1.0 as f32, "tan(PI/4) == 1");
    approx_eq(std::tan((-std::PI/4.0) as f32), -1.0 as f32, "tan(-PI/4) == -1");

    approx_eq(std::asin(0.0 as f32), 0.0 as f32, "asin(0) == 0");
    approx_eq(std::asin(-1.0 as f32), (-std::PI/2.0) as f32, "asin(-1) == -PI/2");
    approx_eq(std::asin(1.0 as f32), (std::PI/2.0) as f32, "asin(1) == PI/2");

    approx_eq(std::acos(0.0 as f32), (std::PI/2.0) as f32, "acos(0) == PI/2");
    approx_eq(std::acos(-1.0 as f32), std::PI as f32, "acos(-1) == PI");
    approx_eq(std::acos(1.0 as f32), 0.0 as f32, "acos(1) == 0");

    approx_eq(std::atan(0.0 as f32), 0.0 as f32, "atan(0) == 0");
    approx_eq(std::atan(1.0 as f32), (std::PI/4.0) as f32, "atan(1) == PI/4");
    approx_eq(std::atan(-1.0 as f32), (-std::PI/4.0) as f32, "atan(-1) == -PI/4");

    approx_eq(std::atan2(0.0 as f32, 0.0 as f32), 0.0 as f32, "atan2(0, 0) == 0");
    approx_eq(std::atan2(7.0 as f32, 0.0 as f32), (std::PI/2.0) as f32, "atan2(7, 0) == PI/2");
    approx_eq(std::atan2(7.0 as f32, -0.0 as f32), (std::PI/2.0) as f32, "atan2(7, -0) == PI/2");
}

fn main(args: [str]) -> i32
{
    test_trigonometric_functions();

    approx_eq(std::abs(-1.0 as f32), 1.0 as f32, "abs(-1)==1");
    approx_eq(std::abs(0.0 as f32), 0.0 as f32, "abs(0)==0");
    approx_eq(std::abs(1.0 as f32), 1.0 as f32, "abs(1)==1");

    approx_eq(std::sqrt(0.0 as f32), 0.0 as f32, "sqrt(0)==0");
    approx_eq(std::sqrt(2.0 as f32), std::SQRT2 as f32, "sqrt(2)==1.4142135");

    approx_eq(std::ceil(2.4 as f32), 3.0 as f32, "ceil(2.4)==3");
    approx_eq(std::ceil(-2.4 as f32), -2.0 as f32, "ceil(-2.4)==-2");
    approx_eq(std::ceil(0.0 as f32), 0.0 as f32, "ceil(0)==0");

    approx_eq(std::floor(2.4 as f32), 2.0 as f32, "floor(2.4)==2");
    approx_eq(std::floor(-2.4 as f32), -3.0 as f32, "floor(-2.4)==-3");
    approx_eq(std::floor(0.0 as f32), 0.0 as f32, "floor(0)==0");

    approx_eq(std::trunc(2.7 as f32), 2.0 as f32, "trunc(2.7)==2");
    approx_eq(std::trunc(-2.9 as f32), -2.0 as f32, "trunc(-2.9)==-2");
    approx_eq(std::trunc(0.7 as f32), 0.0 as f32, "trunc(0.7)==0");
    approx_eq(std::trunc(-0.9 as f32), 0.0 as f32, "trunc(-0.9)==0");

    approx_eq(std::round(2.3 as f32), 2.0 as f32, "std::round(2.3)==2");
    approx_eq(std::round(2.5 as f32), 3.0 as f32, "std::round(2.5)==3");
    approx_eq(std::round(2.7 as f32), 3.0 as f32, "std::round(2.7)==3");
    approx_eq(std::round(-2.3 as f32), -2.0 as f32, "std::round(-2.3)==-2");
    approx_eq(std::round(-2.5 as f32), -3.0 as f32, "std::round(-2.5)==-3");
    approx_eq(std::round(-2.7 as f32), -3.0 as f32, "std::round(-2.7)==-3");

    return 0;
}
