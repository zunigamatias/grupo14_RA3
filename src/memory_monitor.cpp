#include "../include/monitor.h"
#include <fstream>
#include <string>
#include <unistd.h>

bool memory_collect_stats(int pid, process_stats_t *stats) {
    if (!stats) return false;

    std::string path = "/proc/" + std::to_string(pid) + "/statm";
    std::ifstream file(path);

    if (!file.is_open()) return false;

    long pages_total, pages_rss;
    if (!(file >> pages_total >> pages_rss)) return false;

    long page_size = sysconf(_SC_PAGESIZE);

    stats->vsz = pages_total * page_size;
    stats->rss = pages_rss * page_size;

    return true;
}
