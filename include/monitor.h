#ifndef MONITOR_H
#define MONITOR_H

#include <cstdint>
#include <string>      
#include <vector>     


struct process_stats_t {
    // CPU
    uint64_t utime_ticks = 0;
    uint64_t stime_ticks = 0;
    double cpu_percent = 0.0;

    // Memory
    uint64_t vsz = 0;
    uint64_t rss = 0;

    // I/O
    uint64_t read_bytes = 0;
    uint64_t write_bytes = 0;

    // Network
    uint64_t net_rx_bytes = 0;
    uint64_t net_tx_bytes = 0;
};

/* CPU */
bool cpu_collect_stats(int pid, process_stats_t *stats);

/* Memory */
bool memory_collect_stats(int pid, process_stats_t *stats);

/* IO */
bool io_collect_stats(int pid, process_stats_t *stats);

/* Network */
bool network_collect_stats(int pid, process_stats_t *stats);

/* Combined monitor */
int monitor_collect(int pid, process_stats_t *stats);

bool export_to_json(const std::vector<process_stats_t>& data, int pid, const std::string& filename);
bool export_to_csv(const std::vector<process_stats_t>& data, int pid, const std::string& filename);
int monitor_process_with_export(int pid, int duration_seconds, int interval_ms, const std::string& output_format = "json");

#endif
