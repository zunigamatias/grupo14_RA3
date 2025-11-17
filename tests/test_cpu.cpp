#include <cmath>
#include <iostream>

int main() {
    volatile double x = 0;
    for (long long i = 0; i < 5e9; i++) {
        x += std::sin(i);
    }
    std::cout << "Done CPU test\n";
    return 0;
}
