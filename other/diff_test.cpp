#include <cmath>
#include <iomanip>
#include <iostream>
#include <thread>
using namespace std;

inline double difficulty(const unsigned bits)
{
    const unsigned exponent_diff = 8 * (0x20 - ((bits >> 24) & 0xFF));
    const double significand = bits & 0xFFFFFF;
    return std::ldexp(0x0f0f0f / significand, exponent_diff);
}

int main()
{
    double last = 2;
    for (unsigned int i = 0x200f0f0f; i > 0; i--)
    {
        std::cout << std::hex << std::setw(6) << std::setfill('0') << i << ": "
                  << std::dec << difficulty(i) << std::endl;
        if (i < last) exit(1);
        last = difficulty(i);
    }
    cout << "Hello World";

    return 0;
}
