#ifndef NAMESPACE_H
#define NAMESPACE_H

#include <string>
#include <vector>
#include <map>

// Structure to hold all namespace inodes of a process
struct namespace_info_t {
    std::map<std::string, std::string> ns_map;  // namespace -> inode
};

namespace analyzer {

    // List all namespace symlinks of a process
    bool list_namespaces(int pid, namespace_info_t &info);

    // Find all processes that share a specific namespace inode
    std::vector<int> find_processes_in_namespace(const std::string &ns_type, const std::string &inode);

    // Compare namespaces between two processes
    std::map<std::string, bool> compare_namespaces(int pid1, int pid2);

    // Generate a full report of namespaces for all processes
    std::string generate_system_report();

}

#endif
