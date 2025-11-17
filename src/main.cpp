#include <iostream>
#include <thread>
#include <chrono>
#include "../include/monitor.h"

int main(int argc, char** argv) {
    if (argc != 2) {
        std::cerr << "Usage: " << argv[0] << " <pid>\n";
        return 1;
    }

    int pid = std::stoi(argv[1]);

    std::cout << "Monitoring PID " << pid << "...\n";

    process_stats_t prev{};
    process_stats_t curr{};

    bool has_prev = false;

    while (true) {
        if (monitor_collect(pid, &curr) != 0) {
            std::cerr << "Failed to read stats for PID " << pid << "\n";
            return 1;
        }

        std::cout << "-----------------------------\n";
        std::cout << "CPU%: " << curr.cpu_percent << "\n";
        std::cout << "Memory RSS: " << curr.rss / 1024 << " KB\n";
        std::cout << "Memory VSZ: " << curr.vsz / 1024 << " KB\n";
        std::cout << "IO Read bytes: " << curr.read_bytes << "\n";
        std::cout << "IO Write bytes: " << curr.write_bytes << "\n";

        if (has_prev) {
            double dt = 1.0; // 1 second loop

            double read_rate  = (curr.read_bytes  - prev.read_bytes)  / dt;
            double write_rate = (curr.write_bytes - prev.write_bytes) / dt;

            std::cout << "IO Read/s: " << read_rate << "\n";
            std::cout << "IO Write/s: " << write_rate << "\n";
        }

        prev = curr;
        has_prev = true;

        std::this_thread::sleep_for(std::chrono::seconds(1));
    }

    return 0;
}
