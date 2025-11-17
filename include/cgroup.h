#ifndef CGROUP_H
#define CGROUP_H

#include <string>
#include <vector>

namespace cgroup {

bool create(const std::string &name, std::string &out_path, std::string &err);
bool move_pid(const std::string &cg_path, int pid, std::string &err);
bool set_cpu_limit(const std::string &cg_path, int percent, std::string &err);
bool set_mem_limit(const std::string &cg_path, uint64_t bytes, std::string &err);

struct Metrics {
    long cpu_usec;
    long cpu_user_usec;
    long cpu_system_usec;
    long mem_current;
    long mem_max;
    long io_read;
    long io_write;
    long pids;
};

bool read_metrics(const std::string &cg_path, Metrics &m, std::string &err);
bool generate_report(const std::string &cg_path, const Metrics &m, std::string &out);

}

#endif
