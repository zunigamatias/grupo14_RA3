#include "../include/cgroup.h"
#include <iostream>
#include <unistd.h>

int main() {
    std::string cg, err;
    cgroup::Metrics m;

    std::cout << "Creating cgroup 'testcg'...\n";
    if (!cgroup::create("testcg", cg, err)) {
        std::cerr << "Error: " << err << "\n";
        return 1;
    }

    std::cout << "Moving self into cgroup...\n";
    if (!cgroup::move_pid(cg, getpid(), err)) {
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

    // Simulate workload
    std::cout << "Simulating load for 2 seconds...\n";
    for (int i = 0; i < 50000000; i++); // busy loop
    sleep(2);

    if (!cgroup::read_metrics(cg, m, err)) {
        std::cerr << "Error: " << err << "\n";
        return 1;
    }

    std::string report;
    cgroup::generate_report(cg, m, report);
    std::cout << report;

    return 0;
}
