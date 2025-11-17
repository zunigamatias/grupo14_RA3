#include "../include/monitor.h"
#include <fstream>
#include <sstream>
#include <string>

bool network_collect_stats(int pid, process_stats_t *stats) {
    if (!stats) return false;

    std::string path = "/proc/" + std::to_string(pid) + "/net/dev";
    std::ifstream file(path);
    if (!file.is_open()) return false;

    std::string line;

    // Skip the first two header lines
    std::getline(file, line);
    std::getline(file, line);

    uint64_t rx_total = 0;
    uint64_t tx_total = 0;

    while (std::getline(file, line)) {
        std::stringstream ss(line);

        std::string iface;
        ss >> iface;

        if (iface.back() == ':')
            iface.pop_back();

        uint64_t rx_bytes, tx_bytes;

        // Format:
        // iface: RXbytes ... (8 fields total) TXbytes ...
        ss >> rx_bytes;

        // Skip next 7 columns
        for (int i = 0; i < 7; i++) {
            uint64_t tmp;
            ss >> tmp;
        }

        ss >> tx_bytes;

        rx_total += rx_bytes;
        tx_total += tx_bytes;
    }

    stats->net_rx_bytes = rx_total;
    stats->net_tx_bytes = tx_total;

    return true;
}
