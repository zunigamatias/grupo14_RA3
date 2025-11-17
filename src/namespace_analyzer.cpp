#include "../include/namespace.h"
#include <filesystem>
#include <fstream>
#include <sstream>
#include <iostream>
#include <algorithm>

namespace fs = std::filesystem;

namespace analyzer {

bool list_namespaces(int pid, namespace_info_t &info) {
    info.ns_map.clear();
    std::string base = "/proc/" + std::to_string(pid) + "/ns";

    if (!fs::exists(base)) return false;

    for (const auto &entry : fs::directory_iterator(base)) {
        std::string name = entry.path().filename().string();

        std::error_code ec;
        std::string target = fs::read_symlink(entry.path(), ec).string();
        if (ec) continue;

        info.ns_map[name] = target;
    }

    return true;
}

std::vector<int> find_processes_in_namespace(const std::string &ns_type, const std::string &inode) {
    std::vector<int> result;

    for (const auto &entry : fs::directory_iterator("/proc")) {
        if (!entry.is_directory()) continue;

        std::string pid_str = entry.path().filename().string();
        if (!std::all_of(pid_str.begin(), pid_str.end(), ::isdigit)) continue;

        int pid = std::stoi(pid_str);

        namespace_info_t info;
        if (!list_namespaces(pid, info)) continue;

        auto it = info.ns_map.find(ns_type);
        if (it != info.ns_map.end() && it->second == inode) {
            result.push_back(pid);
        }
    }

    return result;
}

std::map<std::string, bool> compare_namespaces(int pid1, int pid2) {
    namespace_info_t ns1, ns2;
    std::map<std::string, bool> comparison;

    if (!list_namespaces(pid1, ns1)) return comparison;
    if (!list_namespaces(pid2, ns2)) return comparison;

    for (auto &p : ns1.ns_map) {
        const std::string &ns = p.first;
        const std::string &inode1 = p.second;

        auto it = ns2.ns_map.find(ns);
        if (it != ns2.ns_map.end()) {
            comparison[ns] = (inode1 == it->second);
        }
    }

    return comparison;
}

std::string generate_system_report() {
    std::stringstream report;

    report << "=== Namespace Isolation Report ===\n\n";

    for (const auto &entry : fs::directory_iterator("/proc")) {
        if (!entry.is_directory()) continue;

        std::string pid_str = entry.path().filename().string();
        if (!std::all_of(pid_str.begin(), pid_str.end(), ::isdigit)) continue;

        int pid = std::stoi(pid_str);
        namespace_info_t ns_info;

        if (list_namespaces(pid, ns_info)) {
            report << "Process " << pid << ":\n";
            for (auto &p : ns_info.ns_map) {
                report << "  " << p.first << " -> " << p.second << "\n";
            }
            report << "\n";
        }
    }

    return report.str();
}

}
