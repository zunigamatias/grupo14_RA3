#include "../include/cgroup.h"
#include "../include/monitor.h"
#include "../include/namespace.h"
#include <iostream>
#include <unistd.h>
#include <sys/wait.h>
#include <chrono>
#include <thread>

void simulate_cpu_load() {
    volatile double x = 0.0;
    for (long i = 0; i < 200000000; ++i) {
        x += i * 0.000001;
    }
}

int main() {
    std::string cg, err;
    cgroup::Metrics m;

    std::cout << "Creating cgroup 'testcgr'...\n";
    if (!cgroup::create("testcgr", cg, err)) {
        std::cerr << "Error: " << err << "\n";
        return 1;
    }

    std::cout << "Setting CPU = 20%...\n";
    if (!cgroup::set_cpu_limit(cg, 20, err)) {
        std::cerr << "Error: " << err << "\n";
        return 1;
    }

    std::cout << "Setting memory = 200MB...\n";
    if (!cgroup::set_mem_limit(cg, 200ull * 1024 * 1024, err)) {
        std::cerr << "Error: " << err << "\n";
        return 1;
    }

    // -------------------------------------------------------------
    // Spawn 2 child processes to move into the cgroup
    // -------------------------------------------------------------
    int pids[2];

    for (int i = 0; i < 2; i++) {
        pid_t pid = fork();

        if (pid == 0) {
            // Child
            std::string e2;

            if (!cgroup::move_pid(cg, getpid(), e2)) {
                std::cerr << "[Child] Failed to move into cgroup: " << e2 << "\n";
                exit(1);
            }

            simulate_cpu_load();
            exit(0);
        } 
        else {
            pids[i] = pid;
        }
    }

    // Wait for processes to run
    std::this_thread::sleep_for(std::chrono::seconds(2));

    // -------------------------------------------------------------
    // Read cgroup metrics
    // -------------------------------------------------------------
    if (!cgroup::read_metrics(cg, m, err)) {
        std::cerr << "Error reading cgroup metrics: " << err << "\n";
        return 1;
    }

    std::string report;
    cgroup::generate_report(cg, m, report);
    std::cout << report;

    // -------------------------------------------------------------
    // MONITOR each child process
    // -------------------------------------------------------------
    for (int i = 0; i < 2; i++) {
        process_stats_t stats;
        monitor_collect(pids[i], &stats);

        std::cout << "\n===== PROCESS MONITOR REPORT (PID: " << pids[i] << ") =====\n";
        std::cout << "CPU user ticks : " << stats.utime_ticks << "\n";
        std::cout << "CPU sys ticks  : " << stats.stime_ticks << "\n";
        std::cout << "CPU percent    : " << stats.cpu_percent << "%\n";
        std::cout << "VSZ            : " << stats.vsz << " bytes\n";
        std::cout << "RSS            : " << stats.rss << " bytes\n";
        std::cout << "IO read        : " << stats.read_bytes << " bytes\n";
        std::cout << "IO write       : " << stats.write_bytes << " bytes\n";
        std::cout << "Net RX         : " << stats.net_rx_bytes << " bytes\n";
        std::cout << "Net TX         : " << stats.net_tx_bytes << " bytes\n";
    }

    // -------------------------------------------------------------
    // NAMESPACE ANALYZER
    // -------------------------------------------------------------
    for (int i = 0; i < 2; i++) {
        namespace_info_t ns_info;

        std::cout << "\n===== NAMESPACE REPORT (PID " << pids[i] << ") =====\n";

        if (analyzer::list_namespaces(pids[i], ns_info)) {
            for (auto &p : ns_info.ns_map) {
                std::cout << p.first << " -> " << p.second << "\n";
            }
        } else {
            std::cout << "Failed to read namespaces.\n";
        }
    }

    // -------------------------------------------------------------
    // Cleanup
    // -------------------------------------------------------------
    for (int i = 0; i < 2; i++) {
        waitpid(pids[i], NULL, 0);
    }

    return 0;
}
