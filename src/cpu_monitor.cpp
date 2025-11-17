#include "../include/monitor.h"
#include <fstream>
#include <string>
#include <unistd.h>
#include <unordered_map>

// Cache previous values per PID
static std::unordered_map<int, std::pair<uint64_t,uint64_t>> prev_cpu;

bool cpu_collect_stats(int pid, process_stats_t *stats) {
    if (!stats) return false;

    std::string path = "/proc/" + std::to_string(pid) + "/stat";
    std::ifstream file(path);
    if (!file.is_open()) return false;

    std::string tmp;
    uint64_t utime = 0, stime = 0;

    // Skip fields 1–13
    for (int i = 1; i <= 13; i++)
        file >> tmp;

    // Read utime and stime (fields 14, 15)
    file >> utime >> stime;

    stats->utime_ticks = utime;
    stats->stime_ticks = stime;

    // Compute CPU %
    long ticks = sysconf(_SC_CLK_TCK);
    long ncpus = sysconf(_SC_NPROCESSORS_ONLN);

    uint64_t prev_u = prev_cpu[pid].first;
    uint64_t prev_s = prev_cpu[pid].second;

    prev_cpu[pid] = {utime, stime};

    // First sample → no CPU%
    if (prev_u == 0 && prev_s == 0) {
        stats->cpu_percent = 0.0;
        return true;
    }

    double delta = (double)((utime - prev_u) + (stime - prev_s));
    double percent = (delta * 100.0) / (ticks * ncpus);

    if (percent < 0) percent = 0.0;

    stats->cpu_percent = percent;
    return true;
}
