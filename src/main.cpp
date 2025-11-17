#include "../include/cgroup.h"
#include <iostream>
#include <unistd.h>

int main() {
    std::string cg = "testgroup";

    std::cout << "Creating cgroup...\n";
    cgroup::create_cgroup(cg);

    std::cout << "Moving self to cgroup...\n";
    cgroup::move_process(cg, getpid());

    std::cout << "Applying CPU limit 20%...\n";
    cgroup::set_cpu_limit(cg, 20);

    std::cout << "Applying memory limit 200MB...\n";
    cgroup::set_memory_limit(cg, 200ull * 1024 * 1024);

    std::cout << "\nReading stats...\n";
    std::cout << cgroup::generate_report(cg) << "\n";

    return 0;
}
