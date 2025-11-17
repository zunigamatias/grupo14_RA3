#include <fstream>
#include <iostream>
#include <vector>

int main() {
    std::ofstream file("iotest.bin", std::ios::binary);

    const size_t SIZE = 100ULL * 1024ULL * 1024ULL; // 100MB
    std::vector<char> buffer(1024 * 1024, 0xAA);

    for (int i = 0; i < 100; i++)
        file.write(buffer.data(), buffer.size());

    file.close();
    std::cout << "Wrote 100MB\n";
    return 0;
}
