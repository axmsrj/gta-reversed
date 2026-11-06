#include "Maths.h"

#include <cassert>
#include <cmath>

int main() {
    CMaths::InitMathsTables();
    constexpr float pi=3.14159265358979323846f;
    assert(std::abs(CMaths::GetSinFast(0.0f))<1.0e-6f);
    assert(std::abs(CMaths::GetSinFast(pi/2.0f)-1.0f)<1.0e-6f);
    assert(std::abs(CMaths::GetCosFast(0.0f)-1.0f)<1.0e-6f);
    assert(std::abs(CMaths::GetSinFast(pi))<1.0e-5f);
}
