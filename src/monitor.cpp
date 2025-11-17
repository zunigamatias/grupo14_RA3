#include "../include/monitor.h"

bool cpu_collect_stats(int pid, process_stats_t *stats);
bool memory_collect_stats(int pid, process_stats_t *stats);
bool io_collect_stats(int pid, process_stats_t *stats);
bool network_collect_stats(int pid, process_stats_t *stats);

int monitor_collect(int pid, process_stats_t *stats) {
    if (!stats) return -1;

    *stats = process_stats_t();

    if (!cpu_collect_stats(pid, stats)) return -1;

    memory_collect_stats(pid, stats);
    io_collect_stats(pid, stats);
    network_collect_stats(pid, stats);

    return 0;
}
