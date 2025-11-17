#ifndef CGROUP_H
#define CGROUP_H

#include <string>
#include <map>

namespace cgroup {

// Base cgroup v2 path (usually /sys/fs/cgroup)
extern const std::string CGROUP_BASE;

// Struct holding all relevant metrics
struct cgroup_stats_t {
    uint64_t cpu_usage = 0;        // in nanoseconds
    uint64_t cpu_limit = 0;        // quota or max

    uint64_t memory_current = 0;   // in bytes
    uint64_t memory_max = 0;       // limit in bytes

    uint64_t io_read = 0;          // bytes
    uint64_t io_write = 0;         // bytes
};

// === Operations ===

// Read CPU, memory, and I/O metrics
bool read_stats(const std::string &cg, cgroup_stats_t &stats);

// Create a cgroup (cg must be relative, e.g. "testgroup")
bool create_cgroup(const std::string &cg);

// Move a process to a cgroup
bool move_process(const std::string &cg, int pid);

// Apply CPU limit (percentage, example: 50 = 50%)
bool set_cpu_limit(const std::string &cg, int percent);

// Apply memory limit in bytes
bool set_memory_limit(const std::string &cg, uint64_t bytes);

// Generate a text report
std::string generate_report(const std::string &cg);

}

#endif
