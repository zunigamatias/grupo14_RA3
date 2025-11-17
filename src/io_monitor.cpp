#include "../include/monitor.h"
#include <fstream>
#include <string>

bool io_collect_stats(int pid, process_stats_t *stats) {
    if (!stats) return false;

    std::string path = "/proc/" + std::to_string(pid) + "/io";
    std::ifstream file(path);
    if (!file.is_open()) return false;

    std::string key;
    uint64_t val;

    while (file >> key >> val) {
        if (key == "read_bytes:")
            stats->read_bytes = val;
        else if (key == "write_bytes:")
            stats->write_bytes = val;
    }

    return true;
}
