#include "../include/cgroup.h"
#include <filesystem>
#include <fstream>
#include <sstream>
#include <cstring>
#include <unistd.h>

namespace fs = std::filesystem;

namespace cgroup {

static const std::string BASE = "/sys/fs/cgroup";

bool create(const std::string &name, std::string &out_path, std::string &err) {
    out_path = BASE + "/" + name;

    try {
        if (!fs::exists(out_path))
            fs::create_directory(out_path);
    } catch (const fs::filesystem_error &e) {
        err = e.what();
        return false;
    }
    return true;
}

bool move_pid(const std::string &cg_path, int pid, std::string &err) {
    std::string file = cg_path + "/cgroup.procs";
    std::ofstream f(file);
    if (!f) { err = strerror(errno); return false; }
    f << pid;
    return true;
}

// CPU.max works with: "max" or "<quota> <period>"
bool set_cpu_limit(const std::string &cg_path, int percent, std::string &err) {
    std::string file = cg_path + "/cpu.max";
    std::ofstream f(file);
    if (!f) { err = strerror(errno); return false; }

    long period = 100000;                    // 100ms
    long quota  = (period * percent) / 100;  // percentage

    f << quota << " " << period;
    return true;
}

bool set_mem_limit(const std::string &cg_path, uint64_t bytes, std::string &err) {
    std::string file = cg_path + "/memory.max";
    std::ofstream f(file);
    if (!f) { err = strerror(errno); return false; }

    f << bytes;
    return true;
}

// Read CPU, memory, IO, PID count
bool read_metrics(const std::string &cg_path, Metrics &m, std::string &err) {
    auto read_long = [&](const std::string &path, long &out) {
        std::ifstream f(path);
        if (!f) return false;
        f >> out;
        return true;
    };

    // CPU stat
    {
        std::ifstream f(cg_path + "/cpu.stat");
        if (!f) { err = "Failed to read cpu.stat"; return false; }

        std::string key;
        long value;

        while (f >> key >> value) {
            if (key == "usage_usec") m.cpu_usec = value;
            if (key == "user_usec")  m.cpu_user_usec = value;
            if (key == "system_usec") m.cpu_system_usec = value;
        }
    }

    read_long(cg_path + "/memory.current",  m.mem_current);
    read_long(cg_path + "/memory.max",      m.mem_max);

    // I/O stat (cgroup v2 unified IO)
    {
        std::ifstream f(cg_path + "/io.stat");
        m.io_read = 0;
        m.io_write = 0;

        if (f) {
            std::string line;
            while (std::getline(f, line)) {
                if (line.find("rbytes=") != std::string::npos) {
                    auto pos = line.find("rbytes=");
                    m.io_read = std::stol(line.substr(pos + 7));
                }
                if (line.find("wbytes=") != std::string::npos) {
                    auto pos = line.find("wbytes=");
                    m.io_write = std::stol(line.substr(pos + 7));
                }
            }
        }
    }

    // PIDs
    {
        std::ifstream f(cg_path + "/pids.current");
        if (f) f >> m.pids;
    }

    return true;
}

bool generate_report(const std::string &cg_path, const Metrics &m, std::string &out) {
    std::ostringstream ss;

    ss << "===== CGROUP REPORT =====\n";
    ss << "Path: " << cg_path << "\n";
    ss << "CPU total    : " << m.cpu_usec << " usec\n";
    ss << "CPU user     : " << m.cpu_user_usec << " usec\n";
    ss << "CPU system   : " << m.cpu_system_usec << " usec\n";
    ss << "Memory curr  : " << m.mem_current << " bytes\n";
    ss << "Memory max   : " << m.mem_max << " bytes\n";
    ss << "I/O read     : " << m.io_read << " bytes\n";
    ss << "I/O write    : " << m.io_write << " bytes\n";
    ss << "PIDs         : " << m.pids << "\n";

    out = ss.str();
    return true;
}

} // namespace cgroup
