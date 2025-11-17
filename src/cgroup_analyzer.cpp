#include "../include/cgroup.h"
#include <filesystem>
#include <fstream>
#include <sstream>
#include <iostream>

namespace fs = std::filesystem;

namespace cgroup {

const std::string CGROUP_BASE = "/sys/fs/cgroup";

static bool write_file(const fs::path &path, const std::string &value) {
    std::ofstream f(path);
    if (!f.is_open()) return false;
    f << value;
    return true;
}

static bool read_file(const fs::path &path, uint64_t &out) {
    std::ifstream f(path);
    if (!f.is_open()) return false;
    f >> out;
    return true;
}

bool create_cgroup(const std::string &cg) {
    fs::path p = fs::path(CGROUP_BASE) / cg;
    if (fs::exists(p)) return true;

    return fs::create_directory(p);
}

bool move_process(const std::string &cg, int pid) {
    fs::path tasks = fs::path(CGROUP_BASE) / cg / "cgroup.procs";
    return write_file(tasks, std::to_string(pid));
}

bool set_cpu_limit(const std::string &cg, int percent) {
    fs::path cpu_path = fs::path(CGROUP_BASE) / cg / "cpu.max";

    if (percent <= 0) {
        return write_file(cpu_path, "max");
    }

    uint64_t period = 100000; // 100ms
    uint64_t quota = (period * percent) / 100;

    std::stringstream ss;
    ss << quota << " " << period;
    return write_file(cpu_path, ss.str());
}

bool set_memory_limit(const std::string &cg, uint64_t bytes) {
    fs::path mem_path = fs::path(CGROUP_BASE) / cg / "memory.max";
    return write_file(mem_path, std::to_string(bytes));
}

bool read_stats(const std::string &cg, cgroup_stats_t &stats) {
    fs::path base = fs::path(CGROUP_BASE) / cg;

    // CPU
    read_file(base / "cpu.stat", stats.cpu_usage); // usage_usec in v2

    // CPU limit
    {
        std::ifstream f(base / "cpu.max");
        if (f.is_open()) {
            std::string quota, period;
            f >> quota >> period;

            if (quota == "max") stats.cpu_limit = 0;
            else stats.cpu_limit = std::stoull(quota);
        }
    }

    // Memory
    read_file(base / "memory.current", stats.memory_current);
    read_file(base / "memory.max", stats.memory_max);

    // IO (blkio)
    {
        std::ifstream f(base / "io.stat");
        if (f.is_open()) {
            std::string dev, key;
            uint64_t val;

            while (f >> dev >> key >> val) {
                if (key == "rbytes") stats.io_read += val;
                if (key == "wbytes") stats.io_write += val;
            }
        }
    }

    return true;
}

std::string generate_report(const std::string &cg) {
    cgroup_stats_t st;
    read_stats(cg, st);

    std::stringstream out;

    out << "=== Cgroup Report: " << cg << " ===\n";

    out << "CPU Usage (ns): " << st.cpu_usage << "\n";
    out << "CPU Limit (quota): " << st.cpu_limit << "\n\n";

    out << "Memory Current: " << st.memory_current / 1024 << " KB\n";
    out << "Memory Max: " << st.memory_max / 1024 << " KB\n\n";

    out << "IO Read: " << st.io_read << " bytes\n";
    out << "IO Write: " << st.io_write << " bytes\n";

    return out.str();
}

}
