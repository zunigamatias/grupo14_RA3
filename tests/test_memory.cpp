#include <vector>
#include <iostream>
#include <cstring>

int main() {
    const size_t SIZE = 1024ULL * 1024ULL * 1024ULL; // 1GB
    std::vector<char> mem(SIZE);

    std::memset(mem.data(), 1, SIZE);

    std::cout << "Allocated and touched 1GB\n";
    return 0;
}
