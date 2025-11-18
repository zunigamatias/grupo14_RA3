#include "../include/monitor.h"
#include "../include/namespace.h"
#include "../include/cgroup.h"
#include <algorithm>
#include <chrono>
#include <cstring>
#include <fcntl.h>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <signal.h>  
#include <sys/resource.h>
#include <sys/mman.h>
#include <sys/mount.h>
#include <sys/stat.h> 
#include <sys/wait.h>
#include <sched.h>
#include <thread>
#include <unistd.h>
#include <vector>


// Simple CPU-intensive workload
void simulate_cpu_load() {
    double x = 0.0;
    for (long i = 0; i < 300000000; ++i) {
        x += i * 0.000001;
        if (i % 10000000 == 0) {
            volatile double prevent_optimization = x;
            (void)prevent_optimization;
        }
    }
    // Use the result to prevent the entire loop from being optimized away
    volatile double final_result = x;
    (void)final_result;
}

// Get CPU usage of a process using rusage
// Fixed: Remove unused parameter or mark it as unused
double get_cpu_usage(pid_t /*pid*/) {
    struct rusage usage;
    if (getrusage(RUSAGE_CHILDREN, &usage) == 0) {
        return (double)(usage.ru_utime.tv_sec + usage.ru_utime.tv_usec / 1000000.0) +
               (double)(usage.ru_stime.tv_sec + usage.ru_stime.tv_usec / 1000000.0);
    }
    return 0.0;
}

void experiment_1_monitoring_overhead() {
    std::cout << "\n" << std::string(70, '=') << "\n";
    std::cout << "EXPERIMENT 1: MONITORING OVERHEAD\n";
    std::cout << "Objective: Measure the impact of the profiler on the system\n";
    std::cout << std::string(70, '=') << "\n\n";

    const std::vector<int> intervals = {10, 50, 100, 500, 1000}; // milliseconds
    const int num_runs = 5; // More runs for better statistics

    std::cout << "Running " << num_runs << " iterations for each test...\n\n";

    // ========================================================================
    // STEP 1: Baseline - Run workload WITHOUT any monitoring
    // ========================================================================
    std::cout << "STEP 1: Measuring baseline performance (no monitoring)\n";
    std::cout << std::string(50, '-') << "\n";

    std::vector<long> baseline_times;

    for (int run = 0; run < num_runs; ++run) {
        std::cout << "  Baseline run " << (run + 1) << "/" << num_runs << "... ";
        std::cout.flush();

        // Measure execution time
        auto start_time = std::chrono::high_resolution_clock::now();
        
        pid_t pid = fork();
        if (pid == 0) {
            // Child: run the workload
            simulate_cpu_load();
            exit(0);
        } else {
            // Parent: just wait (no monitoring)
            int status;
            waitpid(pid, &status, 0);
        }

        auto end_time = std::chrono::high_resolution_clock::now();
        long duration_us = std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time).count();
        
        baseline_times.push_back(duration_us);
        std::cout << duration_us << " μs\n";
    }

    // Calculate baseline statistics
    long baseline_avg = 0, baseline_min = baseline_times[0], baseline_max = baseline_times[0];
    for (long time : baseline_times) {
        baseline_avg += time;
        if (time < baseline_min) baseline_min = time;
        if (time > baseline_max) baseline_max = time;
    }
    baseline_avg /= baseline_times.size();

    std::cout << "\nBaseline Results:\n";
    std::cout << "  Average time: " << baseline_avg << " μs (" << baseline_avg/1000.0 << " ms)\n";
    std::cout << "  Min time:     " << baseline_min << " μs\n";
    std::cout << "  Max time:     " << baseline_max << " μs\n";

    // ========================================================================
    // STEP 2: Test monitoring at different intervals
    // ========================================================================
    std::cout << "\n\nSTEP 2: Testing monitoring overhead at different intervals\n";
    std::cout << std::string(50, '-') << "\n";

    // Results table header
    std::cout << std::left 
              << std::setw(12) << "Interval" 
              << std::setw(15) << "Avg Time (μs)"
              << std::setw(15) << "Overhead (μs)"
              << std::setw(15) << "Overhead (%)"
              << std::setw(18) << "Avg Sampling (μs)"
              << std::setw(15) << "Samples Taken"
              << "\n";
    std::cout << std::string(90, '-') << "\n";

    for (int interval : intervals) {
        std::cout << "Testing interval: " << interval << "ms\n";

        std::vector<long> monitored_times;
        std::vector<long> all_sampling_latencies;
        std::vector<int> sample_counts;

        for (int run = 0; run < num_runs; ++run) {
            std::cout << "  Monitored run " << (run + 1) << "/" << num_runs << "... ";
            std::cout.flush();

            std::vector<long> sampling_latencies;
            
            auto start_time = std::chrono::high_resolution_clock::now();
            
            pid_t pid = fork();
            if (pid == 0) {
                // Child: run the workload
                simulate_cpu_load();
                exit(0);
            } else {
                // Parent: monitor the child process
                int status;
                bool process_running = true;

                while (process_running) {
                    // Check if process is still running
                    int wait_result = waitpid(pid, &status, WNOHANG);
                    if (wait_result != 0) {
                        process_running = false;
                        break;
                    }

                    // Measure sampling latency
                    auto sample_start = std::chrono::high_resolution_clock::now();
                    
                    process_stats_t stats;
                    int collect_result = monitor_collect(pid, &stats);
                    
                    auto sample_end = std::chrono::high_resolution_clock::now();
                    
                    if (collect_result == 0) {
                        long sample_latency = std::chrono::duration_cast<std::chrono::microseconds>(sample_end - sample_start).count();
                        sampling_latencies.push_back(sample_latency);
                        all_sampling_latencies.push_back(sample_latency);
                    }

                    // Wait for the specified interval
                    std::this_thread::sleep_for(std::chrono::milliseconds(interval));
                }

                // Make sure child is reaped
                if (waitpid(pid, &status, WNOHANG) == 0) {
                    waitpid(pid, &status, 0);
                }
            }

            auto end_time = std::chrono::high_resolution_clock::now();
            long duration_us = std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time).count();
            
            monitored_times.push_back(duration_us);
            sample_counts.push_back(sampling_latencies.size());
            
            std::cout << duration_us << " μs (" << sampling_latencies.size() << " samples)\n";
        }

        // Calculate statistics for this interval
        long monitored_avg = 0;
        for (long time : monitored_times) {
            monitored_avg += time;
        }
        monitored_avg /= monitored_times.size();

        long avg_sampling_latency = 0;
        if (!all_sampling_latencies.empty()) {
            for (long latency : all_sampling_latencies) {
                avg_sampling_latency += latency;
            }
            avg_sampling_latency /= all_sampling_latencies.size();
        }

        int avg_samples = 0;
        for (int count : sample_counts) {
            avg_samples += count;
        }
        avg_samples /= sample_counts.size();

        // Calculate overhead
        long overhead_us = monitored_avg - baseline_avg;
        double overhead_percent = ((double)overhead_us / baseline_avg) * 100.0;

        // Display results
        std::cout << std::left 
                  << std::setw(12) << (std::to_string(interval) + "ms")
                  << std::setw(15) << monitored_avg
                  << std::setw(15) << overhead_us
                  << std::setw(15) << std::fixed << std::setprecision(2) << overhead_percent
                  << std::setw(18) << avg_sampling_latency
                  << std::setw(15) << avg_samples
                  << "\n";
    }

    // ========================================================================
    // STEP 3: Summary and Analysis
    // ========================================================================
    std::cout << "\n\nSTEP 3: Analysis and Summary\n";
    std::cout << std::string(50, '-') << "\n";

    std::cout << "Key Findings:\n";
    std::cout << "1. Baseline execution time: " << baseline_avg/1000.0 << " ms (average)\n";
    std::cout << "2. Monitoring adds overhead that varies with sampling frequency\n";
    std::cout << "3. More frequent sampling = higher overhead but better time resolution\n";
    std::cout << "4. Sampling latency indicates the cost of each monitor_collect() call\n";
    
    std::cout << "\nRecommendations:\n";
    std::cout << "- For real-time monitoring: use 100-500ms intervals\n";
    std::cout << "- For detailed analysis: use 10-50ms intervals (accept higher overhead)\n";
    std::cout << "- For production systems: use 1000ms+ intervals\n";
    // ========================================================================
    // STEP 4: Data Export Demonstration
    // ========================================================================
    std::cout << "\n\nSTEP 4: Data export demonstration\n";
    std::cout << std::string(50, '-') << "\n";

    std::cout << "Demonstrating JSON/CSV export functionality...\n";
    
    // Create a demo process to monitor and export data
    pid_t demo_pid = fork();
    if (demo_pid == 0) {
        // Child: run a short workload for demonstration
        simulate_cpu_load();
        exit(0);
    } else {
        // Parent: monitor and export data
        std::cout << "Monitoring process " << demo_pid << " for export demonstration...\n";
        
        // Monitor for 5 seconds with 500ms intervals, export as JSON
        int samples_collected = monitor_process_with_export(demo_pid, 5, 500, "json");
        
        std::cout << "JSON export completed with " << samples_collected << " samples.\n";
        
        // Wait for demo process to complete
        int status;
        waitpid(demo_pid, &status, 0);
        
        // Also demonstrate CSV export with a quick example
        std::cout << "Creating CSV export example...\n";
        
        // Create sample data for CSV demo
        std::vector<process_stats_t> sample_data;
        for (int i = 0; i < 3; ++i) {
            process_stats_t sample = {};
            sample.cpu_percent = 25.5 + i * 10;
            sample.rss = (100 + i * 50) * 1024 * 1024; // MB to bytes
            sample.read_bytes = i * 1024 * 1024;
            sample.write_bytes = i * 512 * 1024;
            sample_data.push_back(sample);
        }
        
        if (export_to_csv(sample_data, demo_pid, "experiment1_demo.csv")) {
            std::cout << "CSV export demonstration completed: experiment1_demo.csv\n";
        }
        
        std::cout << "\nExported files can be used for further analysis with external tools.\n";
        std::cout << "JSON format is suitable for programmatic processing.\n";
        std::cout << "CSV format is suitable for spreadsheet applications and data visualization.\n";
    }

    std::cout << "\n" << std::string(70, '=') << "\n";
    std::cout << "EXPERIMENT 1 COMPLETED\n";
    std::cout << std::string(70, '=') << "\n\n";
}

void experiment_2_namespace_isolation() {
    std::cout << "\n" << std::string(70, '=') << "\n";
    std::cout << "EXPERIMENT 2: NAMESPACE ISOLATION\n";
    std::cout << "Objective: Validate effectiveness of isolation\n";
    std::cout << std::string(70, '=') << "\n\n";

    // Define namespace types and their unshare flags
    struct NamespaceTest {
        std::string name;
        int flag;
        std::string description;
    };

    std::vector<NamespaceTest> ns_tests = {
        {"UTS",    CLONE_NEWUTS,    "Hostname and domain name"},
        {"IPC",    CLONE_NEWIPC,    "Inter-process communication"},
        {"PID",    CLONE_NEWPID,    "Process IDs"},
        {"NET",    CLONE_NEWNET,    "Network interfaces"},
        {"MNT",    CLONE_NEWNS,     "Mount points"},
        {"USER",   CLONE_NEWUSER,   "User and group IDs"},
        {"CGROUP", CLONE_NEWCGROUP, "Control group hierarchy"}
    };

    // ========================================================================
    // STEP 1: Measure namespace creation overhead
    // ========================================================================
    std::cout << "STEP 1: Measuring namespace creation overhead\n";
    std::cout << std::string(50, '-') << "\n";

    std::cout << std::left 
              << std::setw(10) << "Type" 
              << std::setw(20) << "Description"
              << std::setw(15) << "Time (μs)"
              << std::setw(10) << "Success"
              << "\n";
    std::cout << std::string(65, '-') << "\n";

    std::vector<std::pair<std::string, long>> creation_times;

    for (const auto& test : ns_tests) {
        auto start_time = std::chrono::high_resolution_clock::now();
        
        pid_t pid = fork();
        if (pid == 0) {
            // Child: Try to create namespace
            int result = unshare(test.flag);
            exit(result == 0 ? 0 : 1);  // Exit with success/failure code
        } else {
            // Parent: Wait and measure time
            int status;
            waitpid(pid, &status, 0);
            
            auto end_time = std::chrono::high_resolution_clock::now();
            long creation_time = std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time).count();
            
            bool success = WIFEXITED(status) && WEXITSTATUS(status) == 0;
            creation_times.push_back({test.name, creation_time});
            
            std::cout << std::left 
                      << std::setw(10) << test.name
                      << std::setw(20) << test.description
                      << std::setw(15) << creation_time
                      << std::setw(10) << (success ? "YES" : "NO")
                      << "\n";
        }
    }

    // ========================================================================
    // STEP 2: Test isolation effectiveness
    // ========================================================================
    std::cout << "\n\nSTEP 2: Testing isolation effectiveness\n";
    std::cout << std::string(50, '-') << "\n";

    // Create reference process (no isolation)
    pid_t ref_pid = fork();
    if (ref_pid == 0) {
        // Reference child - no namespace changes
        std::this_thread::sleep_for(std::chrono::seconds(8));
        exit(0);
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(100)); // Let it start

    // Test different namespace combinations
    struct IsolationTest {
        std::string name;
        std::vector<int> flags;
    };

    std::vector<IsolationTest> isolation_tests = {
        {"UTS only",     {CLONE_NEWUTS}},
        {"IPC only",     {CLONE_NEWIPC}},
        {"PID only",     {CLONE_NEWPID}},
        {"UTS+IPC",      {CLONE_NEWUTS, CLONE_NEWIPC}},
        {"UTS+IPC+PID",  {CLONE_NEWUTS, CLONE_NEWIPC, CLONE_NEWPID}},
    };

    std::vector<pid_t> test_pids;

    std::cout << "Creating processes with different namespace combinations...\n";

    for (const auto& iso_test : isolation_tests) {
        pid_t pid = fork();
        if (pid == 0) {
            // Child: Create specified namespaces
            for (int flag : iso_test.flags) {
                if (unshare(flag) != 0) {
                    // If unshare fails, continue anyway for testing
                    continue;
                }
            }
            
            // Sleep to allow analysis
            std::this_thread::sleep_for(std::chrono::seconds(6));
            exit(0);
        } else {
            test_pids.push_back(pid);
            std::cout << "  Created " << iso_test.name << " process: PID " << pid << "\n";
        }
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(500)); // Let processes start

    // ========================================================================
    // STEP 3: Analyze namespace isolation
    // ========================================================================
    std::cout << "\n\nSTEP 3: Namespace isolation analysis\n";
    std::cout << std::string(50, '-') << "\n";

    // Get reference process namespaces
    namespace_info_t ref_ns;
    if (analyzer::list_namespaces(ref_pid, ref_ns)) {
        std::cout << "Reference process (" << ref_pid << ") namespaces:\n";
        for (const auto& ns : ref_ns.ns_map) {
            std::cout << "  " << std::left << std::setw(8) << ns.first << " -> " << ns.second << "\n";
        }
    }

    // Compare each test process with reference
    std::cout << "\nNamespace comparison table:\n";
    std::cout << std::left 
              << std::setw(12) << "Process"
              << std::setw(8) << "UTS"
              << std::setw(8) << "IPC" 
              << std::setw(8) << "PID"
              << std::setw(8) << "NET"
              << std::setw(8) << "MNT"
              << std::setw(8) << "USER"
              << std::setw(8) << "CGROUP"
              << "\n";
    std::cout << std::string(68, '-') << "\n";

    for (size_t i = 0; i < test_pids.size() && i < isolation_tests.size(); ++i) {
        pid_t test_pid = test_pids[i];
        std::string test_name = isolation_tests[i].name;
        
        namespace_info_t test_ns;
        if (analyzer::list_namespaces(test_pid, test_ns)) {
            auto comparison = analyzer::compare_namespaces(ref_pid, test_pid);
            
            std::cout << std::left << std::setw(12) << (test_name + "(" + std::to_string(test_pid) + ")");
            
            std::vector<std::string> ns_types = {"uts", "ipc", "pid", "net", "mnt", "user", "cgroup"};
            for (const std::string& ns_type : ns_types) {
                auto it = comparison.find(ns_type);
                if (it != comparison.end()) {
                    std::cout << std::setw(8) << (it->second ? "SAME" : "DIFF");
                } else {
                    std::cout << std::setw(8) << "N/A";
                }
            }
            std::cout << "\n";
        }
    }

    // ========================================================================
    // STEP 4: Count processes per namespace
    // ========================================================================
    std::cout << "\n\nSTEP 4: Process count analysis\n";
    std::cout << std::string(50, '-') << "\n";

    // Use reference process namespaces as baseline
    if (!ref_ns.ns_map.empty()) {
        std::cout << "Processes sharing namespaces with reference process:\n";
        
        for (const auto& ns_pair : ref_ns.ns_map) {
            const std::string& ns_type = ns_pair.first;
            const std::string& ns_inode = ns_pair.second;
            
            auto processes = analyzer::find_processes_in_namespace(ns_type, ns_inode);
            
            std::cout << "  " << std::left << std::setw(8) << ns_type 
                      << " (" << ns_inode << "): " 
                      << processes.size() << " processes";
            
            if (processes.size() <= 10) {  // Show PIDs if not too many
                std::cout << " [";
                for (size_t i = 0; i < processes.size(); ++i) {
                    if (i > 0) std::cout << ", ";
                    std::cout << processes[i];
                }
                std::cout << "]";
            }
            std::cout << "\n";
        }
    }

    // ========================================================================
    // STEP 5: Effectiveness summary
    // ========================================================================
    std::cout << "\n\nSTEP 5: Isolation effectiveness summary\n";
    std::cout << std::string(50, '-') << "\n";

    std::cout << "Creation overhead analysis:\n";
    for (const auto& timing : creation_times) {
        std::cout << "  " << std::left << std::setw(8) << timing.first 
                  << ": " << timing.second << " μs\n";
    }

    // Calculate average creation time
    long total_time = 0;
    for (const auto& timing : creation_times) {
        total_time += timing.second;
    }
    double avg_time = (double)total_time / creation_times.size();
    std::cout << "  Average creation time: " << std::fixed << std::setprecision(1) << avg_time << " μs\n";

    std::cout << "\nKey findings:\n";
    std::cout << "1. Namespace creation overhead varies by type\n";
    std::cout << "2. PID and NET namespaces typically have higher overhead\n";
    std::cout << "3. UTS and IPC namespaces are lightweight\n";
    std::cout << "4. Isolation effectiveness depends on namespace type and system state\n";

    std::cout << "\nLimitations in this test environment:\n";
    std::cout << "- Some namespace operations require root privileges\n";
    std::cout << "- Container runtime may affect results\n";
    std::cout << "- Network namespace isolation limited without additional setup\n";

    // ========================================================================
    // Cleanup
    // ========================================================================
    std::cout << "\nCleaning up test processes...\n";
    
    // Terminate reference process
    kill(ref_pid, SIGTERM);
    waitpid(ref_pid, nullptr, 0);
    
    // Terminate test processes
    for (pid_t pid : test_pids) {
        kill(pid, SIGTERM);
        waitpid(pid, nullptr, 0);
    }

    std::cout << "\n" << std::string(70, '=') << "\n";
    std::cout << "EXPERIMENT 2 COMPLETED\n";
    std::cout << std::string(70, '=') << "\n\n";
}

// ============================================================================
// EXPERIMENT 3: CPU THROTTLING
// ============================================================================
void experiment_3_cpu_throttling() {
    std::cout << "\n" << std::string(70, '=') << "\n";
    std::cout << "EXPERIMENT 3: CPU THROTTLING\n";
    std::cout << "Objective: Evaluate precision of CPU limitation via cgroups\n";
    std::cout << std::string(70, '=') << "\n\n";

    // CPU limits to test (percentage of one core)
    const std::vector<int> cpu_limits = {25, 50, 100, 200}; // 0.25, 0.5, 1.0, 2.0 cores
    const int test_duration = 8; // seconds
    const int monitoring_interval = 200; // milliseconds

    std::cout << "Testing CPU throttling with limits: 25%, 50%, 100%, 200% of one core\n";
    std::cout << "Test duration: " << test_duration << " seconds per configuration\n\n";

    // ========================================================================
    // STEP 1: Baseline - No CPU limits
    // ========================================================================
    std::cout << "STEP 1: Baseline measurement (no CPU limits)\n";
    std::cout << std::string(50, '-') << "\n";

    // Fixed workload with iteration counter - no volatile compound assignments
    auto cpu_intensive_workload_with_counter = [](int duration_sec, volatile long* counter) {
        auto end_time = std::chrono::steady_clock::now() + std::chrono::seconds(duration_sec);
        double x = 0.0;  // Removed volatile
        long local_counter = 0;  // Use local non-volatile counter

        while (std::chrono::steady_clock::now() < end_time) {
            for (int i = 0; i < 100000; ++i) {
                x += i * 0.000001;  // No longer volatile compound assignment
            }
            local_counter++;  // No longer volatile increment
        }
        
        // Copy final result to shared memory once
        *counter = local_counter;
        
        // Prevent optimization of the computation
        volatile double final_result = x;
        (void)final_result;
    };

    // Create shared memory for counter
    volatile long* shared_counter = (volatile long*)mmap(NULL, sizeof(long), 
                                    PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
    
    std::cout << "Running baseline test...\n";
    
    auto baseline_start = std::chrono::high_resolution_clock::now();
    
    pid_t baseline_pid = fork();
    if (baseline_pid == 0) {
        // Child: Run workload without limits
        cpu_intensive_workload_with_counter(test_duration, shared_counter);
        exit(0);
    }

    // Monitor baseline process
    std::vector<double> baseline_cpu_samples;
    auto monitor_end_time = std::chrono::steady_clock::now() + std::chrono::seconds(test_duration - 1);
    
    std::this_thread::sleep_for(std::chrono::milliseconds(500)); // Let process start
    
    while (std::chrono::steady_clock::now() < monitor_end_time) {
        process_stats_t stats;
        if (monitor_collect(baseline_pid, &stats) == 0) {
            baseline_cpu_samples.push_back(stats.cpu_percent);
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(monitoring_interval));
    }

    int status;
    waitpid(baseline_pid, &status, 0);
    
    auto baseline_end = std::chrono::high_resolution_clock::now();
    long baseline_iterations = *shared_counter;
    
    // Calculate baseline metrics
    double baseline_cpu_avg = 0;
    for (double sample : baseline_cpu_samples) {
        baseline_cpu_avg += sample;
    }
    baseline_cpu_avg /= baseline_cpu_samples.size();
    
    long baseline_duration_ms = std::chrono::duration_cast<std::chrono::milliseconds>(baseline_end - baseline_start).count();
    double baseline_throughput = (double)baseline_iterations / (baseline_duration_ms / 1000.0);

    std::cout << "Baseline Results:\n";
    std::cout << "  Average CPU usage: " << std::fixed << std::setprecision(1) << baseline_cpu_avg << "%\n";
    std::cout << "  Throughput: " << std::fixed << std::setprecision(0) << baseline_throughput << " iterations/sec\n";
    std::cout << "  Total iterations: " << baseline_iterations << "\n";
    std::cout << "  Duration: " << baseline_duration_ms << " ms\n\n";

    // ========================================================================
    // STEP 2: Test different CPU limits
    // ========================================================================
    std::cout << "STEP 2: Testing CPU throttling at different limits\n";
    std::cout << std::string(50, '-') << "\n";

    // Results table header
    std::cout << std::left 
              << std::setw(12) << "CPU Limit"
              << std::setw(15) << "Measured %"
              << std::setw(15) << "Deviation"
              << std::setw(18) << "Throughput/sec"
              << std::setw(15) << "Efficiency"
              << std::setw(12) << "Throttled"
              << "\n";
    std::cout << std::string(87, '-') << "\n";

    for (int limit : cpu_limits) {
        std::cout << "Testing " << limit << "% CPU limit...\n";

        // Create cgroup for this test
        std::string cg_path, err;
        std::string cg_name = "cpu_test_" + std::to_string(limit);
        
        if (!cgroup::create(cg_name, cg_path, err)) {
            std::cerr << "  Failed to create cgroup: " << err << "\n";
            continue;
        }

        // Set CPU limit
        if (!cgroup::set_cpu_limit(cg_path, limit, err)) {
            std::cerr << "  Failed to set CPU limit: " << err << "\n";
            continue;
        }

        // Reset counter
        *shared_counter = 0;

        // Start test process
        auto test_start = std::chrono::high_resolution_clock::now();
        
        pid_t test_pid = fork();
        if (test_pid == 0) {
            // Child: Move to cgroup and run workload
            if (!cgroup::move_pid(cg_path, getpid(), err)) {
                std::cerr << "  Failed to move to cgroup: " << err << "\n";
                exit(1);
            }
            
            cpu_intensive_workload_with_counter(test_duration, shared_counter);
            exit(0);
        }

        // Monitor the test process
        std::vector<double> cpu_samples;
        monitor_end_time = std::chrono::steady_clock::now() + std::chrono::seconds(test_duration - 1);
        
        std::this_thread::sleep_for(std::chrono::milliseconds(500)); // Let process start and enter cgroup
        
        while (std::chrono::steady_clock::now() < monitor_end_time) {
            process_stats_t stats;
            if (monitor_collect(test_pid, &stats) == 0) {
                cpu_samples.push_back(stats.cpu_percent);
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(monitoring_interval));
        }

        waitpid(test_pid, &status, 0);
        auto test_end = std::chrono::high_resolution_clock::now();

        // Get final cgroup metrics
        cgroup::Metrics cg_metrics;
        cgroup::read_metrics(cg_path, cg_metrics, err);

        // Calculate test metrics
        double cpu_avg = 0;
        for (double sample : cpu_samples) {
            cpu_avg += sample;
        }
        cpu_avg /= cpu_samples.size();

        long test_iterations = *shared_counter;
        long test_duration_ms = std::chrono::duration_cast<std::chrono::milliseconds>(test_end - test_start).count();
        double test_throughput = (double)test_iterations / (test_duration_ms / 1000.0);

        // Calculate deviation and efficiency
        double target_cpu = (double)limit;
        double deviation = ((cpu_avg - target_cpu) / target_cpu) * 100.0;
        double efficiency = (test_throughput / baseline_throughput) * 100.0;
        
        // Check if throttling occurred (from cgroup metrics)
        bool throttled = cg_metrics.cpu_usec > 0 && cpu_avg < (limit - 5); // 5% tolerance

        // Display results
        std::cout << std::left 
                  << std::setw(12) << (std::to_string(limit) + "%")
                  << std::setw(15) << std::fixed << std::setprecision(1) << cpu_avg
                  << std::setw(15) << std::fixed << std::setprecision(1) << deviation << "%"
                  << std::setw(18) << std::fixed << std::setprecision(0) << test_throughput
                  << std::setw(15) << std::fixed << std::setprecision(1) << efficiency << "%"
                  << std::setw(12) << (throttled ? "YES" : "NO")
                  << "\n";

        // Cleanup cgroup (optional - system will clean up on reboot)
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    // ========================================================================
    // STEP 3: Detailed Analysis
    // ========================================================================
    std::cout << "\n\nSTEP 3: Throttling effectiveness analysis\n";
    std::cout << std::string(50, '-') << "\n";

    std::cout << "Analysis of CPU throttling precision:\n\n";

    // Re-run one test with detailed monitoring for visualization
    int detailed_limit = 50; // Test 50% limit in detail
    std::cout << "Detailed analysis for " << detailed_limit << "% CPU limit:\n";
    
    std::string cg_path, err;
    std::string cg_name = "cpu_detailed_test";
    
    if (cgroup::create(cg_name, cg_path, err) && cgroup::set_cpu_limit(cg_path, detailed_limit, err)) {
        *shared_counter = 0;
        
        pid_t detailed_pid = fork();
        if (detailed_pid == 0) {
            cgroup::move_pid(cg_path, getpid(), err);
            cpu_intensive_workload_with_counter(6, shared_counter);
            exit(0);
        }

        // Monitor with higher frequency for detailed view
        std::cout << std::left 
                  << std::setw(8) << "Time(s)"
                  << std::setw(12) << "CPU %"
                  << std::setw(15) << "Iterations"
                  << std::setw(12) << "Rate/sec"
                  << "\n";
        std::cout << std::string(47, '-') << "\n";

        auto detailed_start = std::chrono::steady_clock::now();
        long prev_iterations = 0;
        
        for (int i = 0; i < 30; ++i) { // 6 seconds, 200ms intervals
            auto now = std::chrono::steady_clock::now();
            double elapsed = std::chrono::duration<double>(now - detailed_start).count();
            
            process_stats_t stats;
            monitor_collect(detailed_pid, &stats);
            
            long current_iterations = *shared_counter;
            double rate = (current_iterations - prev_iterations) / 0.2; // per 200ms -> per second
            prev_iterations = current_iterations;
            
            std::cout << std::left 
                      << std::setw(8) << std::fixed << std::setprecision(1) << elapsed
                      << std::setw(12) << std::fixed << std::setprecision(1) << stats.cpu_percent
                      << std::setw(15) << current_iterations
                      << std::setw(12) << std::fixed << std::setprecision(0) << rate * 5 // approximate per second
                      << "\n";
            
            std::this_thread::sleep_for(std::chrono::milliseconds(200));
        }

        waitpid(detailed_pid, &status, 0);
    }

    // ========================================================================
    // STEP 4: Summary
    // ========================================================================
    std::cout << "\n\nSTEP 4: Summary and recommendations\n";
    std::cout << std::string(50, '-') << "\n";

    std::cout << "Key findings:\n";
    std::cout << "1. CPU throttling precision varies with system load\n";
    std::cout << "2. Lower limits (25%, 50%) typically show better adherence\n";
    std::cout << "3. Limits above 100% depend on available CPU cores\n";
    std::cout << "4. Throttling introduces overhead affecting throughput\n";

    std::cout << "\nCgroup CPU throttling characteristics:\n";
    std::cout << "- Uses Completely Fair Scheduler (CFS) bandwidth control\n";
    std::cout << "- Quota/period mechanism enforces limits over time windows\n";
    std::cout << "- Some burst capability within quota periods\n";
    std::cout << "- Precision affected by scheduling granularity\n";

    // Cleanup shared memory
    munmap((void*)shared_counter, sizeof(long));

    std::cout << "\n" << std::string(70, '=') << "\n";
    std::cout << "EXPERIMENT 3 COMPLETED\n";
    std::cout << std::string(70, '=') << "\n\n";
}


void experiment_4_memory_limitation() {
    std::cout << "\n" << std::string(70, '=') << "\n";
    std::cout << "EXPERIMENT 4: MEMORY LIMITATION\n";
    std::cout << "Objective: Test behavior when reaching memory limits\n";
    std::cout << std::string(70, '=') << "\n\n";

    const uint64_t MEMORY_LIMIT = 100 * 1024 * 1024; // 100MB
    const uint64_t ALLOCATION_STEP = 5 * 1024 * 1024;  // 5MB per step
    const uint64_t MAX_ALLOCATION = 150 * 1024 * 1024; // Try up to 150MB

    std::cout << "Memory limit: " << (MEMORY_LIMIT / (1024*1024)) << " MB\n";
    std::cout << "Allocation step: " << (ALLOCATION_STEP / (1024*1024)) << " MB\n";
    std::cout << "Maximum test allocation: " << (MAX_ALLOCATION / (1024*1024)) << " MB\n\n";

    // ========================================================================
    // STEP 1: Create memory-limited cgroup
    // ========================================================================
    std::cout << "STEP 1: Setting up memory-limited cgroup\n";
    std::cout << std::string(50, '-') << "\n";

    std::string cg_path, err;
    if (!cgroup::create("memory_test", cg_path, err)) {
        std::cerr << "Failed to create cgroup: " << err << "\n";
        return;
    }

    if (!cgroup::set_mem_limit(cg_path, MEMORY_LIMIT, err)) {
        std::cerr << "Failed to set memory limit: " << err << "\n";
        return;
    }

    std::cout << "Created cgroup: " << cg_path << "\n";
    std::cout << "Memory limit set to: " << (MEMORY_LIMIT / (1024*1024)) << " MB\n\n";

    // ========================================================================
    // STEP 2: Incremental memory allocation test
    // ========================================================================
    std::cout << "STEP 2: Incremental memory allocation test\n";
    std::cout << std::string(50, '-') << "\n";

    // Shared memory for communication with child process
    struct AllocationResult {
        uint64_t max_allocated;
        uint64_t final_rss;
        int exit_reason; // 0=normal, 1=allocation_failed, 2=killed, 3=other
        bool oom_killed;
    };

    AllocationResult* result = (AllocationResult*)mmap(NULL, sizeof(AllocationResult),
                               PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
    
    // Initialize result
    result->max_allocated = 0;
    result->final_rss = 0;
    result->exit_reason = 0;
    result->oom_killed = false;

        // Memory allocation workload
    auto memory_allocation_workload = [](uint64_t /*limit_bytes*/, uint64_t step_bytes, 
                                        uint64_t max_bytes, AllocationResult* res) {
        std::vector<void*> allocations;
        uint64_t total_allocated = 0;
        bool allocation_failed = false;

        std::cout << "Starting incremental allocation...\n";
        std::cout << std::left 
                  << std::setw(12) << "Step"
                  << std::setw(15) << "Target (MB)"
                  << std::setw(15) << "Actual (MB)"
                  << std::setw(12) << "Status"
                  << "\n";
        std::cout << std::string(54, '-') << "\n";

        int step = 1;
        while (total_allocated < max_bytes && !allocation_failed) {
            uint64_t target_total = step * step_bytes;
            
            // Try to allocate the next chunk
            void* ptr = malloc(step_bytes);
            if (ptr == nullptr) {
                std::cout << std::left 
                          << std::setw(12) << step
                          << std::setw(15) << (target_total / (1024*1024))
                          << std::setw(15) << (total_allocated / (1024*1024))
                          << std::setw(12) << "MALLOC_FAIL"
                          << "\n";
                allocation_failed = true;
                res->exit_reason = 1; // allocation failed
                break;
            }

            allocations.push_back(ptr);

            // Touch all pages to ensure physical allocation
            memset(ptr, step % 256, step_bytes);
            total_allocated += step_bytes;

            std::cout << std::left 
                      << std::setw(12) << step
                      << std::setw(15) << (target_total / (1024*1024))
                      << std::setw(15) << (total_allocated / (1024*1024))
                      << std::setw(12) << "OK"
                      << "\n";

            // Small delay to allow monitoring
            std::this_thread::sleep_for(std::chrono::milliseconds(300));
            
            step++;
        }

        res->max_allocated = total_allocated;

        // Try to read current RSS before cleanup
        std::ifstream status_file("/proc/self/status");
        std::string line;
        while (std::getline(status_file, line)) {
            if (line.find("VmRSS:") == 0) {
                std::stringstream ss(line);
                std::string label, value, unit;
                ss >> label >> value >> unit;
                res->final_rss = std::stoull(value) * 1024; // Convert kB to bytes
                break;
            }
        }

        // Cleanup
        for (void* ptr : allocations) {
            free(ptr);
        }

        std::cout << "Allocation test completed. Max allocated: " 
                  << (total_allocated / (1024*1024)) << " MB\n";
    };

    // Start child process with memory workload
    pid_t test_pid = fork();
    if (test_pid == 0) {
        // Child: Move to cgroup and run memory allocation
        if (!cgroup::move_pid(cg_path, getpid(), err)) {
            std::cerr << "Failed to move to cgroup: " << err << "\n";
            exit(1);
        }

        memory_allocation_workload(MEMORY_LIMIT, ALLOCATION_STEP, MAX_ALLOCATION, result);
        exit(0);
    }

    // ========================================================================
    // STEP 3: Monitor memory usage and cgroup metrics
    // ========================================================================
    std::cout << "\n\nSTEP 3: Monitoring memory usage and cgroup behavior\n";
    std::cout << std::string(50, '-') << "\n";

    std::vector<uint64_t> memory_usage_samples;
    std::vector<uint64_t> cgroup_usage_samples;
    std::vector<uint64_t> failcnt_samples;

    std::cout << std::left 
              << std::setw(8) << "Time(s)"
              << std::setw(12) << "RSS (MB)"
              << std::setw(15) << "Cgroup (MB)"
              << std::setw(12) << "Limit (MB)"
              << std::setw(10) << "Failcnt"
              << "\n";
    std::cout << std::string(57, '-') << "\n";

    auto monitor_start = std::chrono::steady_clock::now();
    int status;
    bool process_running = true;

    while (process_running) {
        // Check if process is still running
        int wait_result = waitpid(test_pid, &status, WNOHANG);
        if (wait_result != 0) {
            process_running = false;
            
            // Check how the process exited
            if (WIFSIGNALED(status)) {
                int signal = WTERMSIG(status);
                if (signal == SIGKILL) {
                    result->oom_killed = true;
                    result->exit_reason = 2; // killed by OOM
                    std::cout << "Process was killed by signal " << signal << " (likely OOM killer)\n";
                }
            } else if (WIFEXITED(status)) {
                int exit_code = WEXITSTATUS(status);
                if (result->exit_reason == 0) {
                    result->exit_reason = exit_code;
                }
            }
            break;
        }

        auto now = std::chrono::steady_clock::now();
        double elapsed = std::chrono::duration<double>(now - monitor_start).count();

        // Get process memory usage
        process_stats_t stats;
        uint64_t rss_mb = 0;
        if (monitor_collect(test_pid, &stats) == 0) {
            rss_mb = stats.rss / (1024 * 1024);
            memory_usage_samples.push_back(stats.rss);
        }

        // Get cgroup metrics
        cgroup::Metrics cg_metrics;
        uint64_t cgroup_mb = 0;
        if (cgroup::read_metrics(cg_path, cg_metrics, err)) {
            cgroup_mb = cg_metrics.mem_current / (1024 * 1024);
            cgroup_usage_samples.push_back(cg_metrics.mem_current);
        }

        // Read memory.failcnt (if available)
        uint64_t failcnt = 0;
        std::ifstream failcnt_file(cg_path + "/memory.failcnt");
        if (failcnt_file) {
            failcnt_file >> failcnt;
            failcnt_samples.push_back(failcnt);
        }

        std::cout << std::left 
                  << std::setw(8) << std::fixed << std::setprecision(1) << elapsed
                  << std::setw(12) << rss_mb
                  << std::setw(15) << cgroup_mb
                  << std::setw(12) << (MEMORY_LIMIT / (1024*1024))
                  << std::setw(10) << failcnt
                  << "\n";

        std::this_thread::sleep_for(std::chrono::milliseconds(500));
    }

    // ========================================================================
    // STEP 4: Analyze results and behavior
    // ========================================================================
    std::cout << "\n\nSTEP 4: Analysis of memory limitation behavior\n";
    std::cout << std::string(50, '-') << "\n";

    // Final cgroup metrics
    cgroup::Metrics final_metrics;
    cgroup::read_metrics(cg_path, final_metrics, err);

    std::cout << "Memory allocation test results:\n";
    std::cout << "  Maximum allocated: " << (result->max_allocated / (1024*1024)) << " MB\n";
    std::cout << "  Final RSS: " << (result->final_rss / (1024*1024)) << " MB\n";
    std::cout << "  Memory limit: " << (MEMORY_LIMIT / (1024*1024)) << " MB\n";

    // Determine what happened
    std::string behavior_description;
    switch (result->exit_reason) {
        case 0:
            behavior_description = "Normal completion - stayed within limits";
            break;
        case 1:
            behavior_description = "Allocation failed - malloc() returned NULL";
            break;
        case 2:
            behavior_description = "Process killed by OOM killer";
            break;
        default:
            behavior_description = "Process exited with error code " + std::to_string(result->exit_reason);
    }

    std::cout << "  Exit behavior: " << behavior_description << "\n";
    std::cout << "  OOM killed: " << (result->oom_killed ? "YES" : "NO") << "\n";

    // Failure count analysis
    if (!failcnt_samples.empty()) {
        uint64_t max_failcnt = *std::max_element(failcnt_samples.begin(), failcnt_samples.end());
        std::cout << "  Memory failures: " << max_failcnt << " events\n";
    }

    // Peak memory usage
    if (!cgroup_usage_samples.empty()) {
        uint64_t peak_usage = *std::max_element(cgroup_usage_samples.begin(), cgroup_usage_samples.end());
        std::cout << "  Peak cgroup usage: " << (peak_usage / (1024*1024)) << " MB\n";
        double usage_efficiency = ((double)peak_usage / MEMORY_LIMIT) * 100.0;
        std::cout << "  Limit utilization: " << std::fixed << std::setprecision(1) << usage_efficiency << "%\n";
    }

    // ========================================================================
    // STEP 5: Summary and system behavior analysis
    // ========================================================================
    std::cout << "\n\nSTEP 5: Memory limitation effectiveness summary\n";
    std::cout << std::string(50, '-') << "\n";

    std::cout << "Observed system behavior:\n";

    if (result->max_allocated > MEMORY_LIMIT) {
        std::cout << "1. Process exceeded memory limit before being constrained\n";
        std::cout << "   - This indicates overcommit or delayed enforcement\n";
    } else {
        std::cout << "1. Process was constrained within memory limit\n";
        std::cout << "   - Memory limit enforcement was effective\n";
    }

    if (result->oom_killed) {
        std::cout << "2. OOM killer activated when limit was exceeded\n";
        std::cout << "   - Kernel enforced hard memory limit\n";
    } else if (result->exit_reason == 1) {
        std::cout << "2. malloc() failures occurred before OOM killing\n";
        std::cout << "   - Application-level memory allocation constraints\n";
    } else {
        std::cout << "2. Process completed without hitting memory constraints\n";
        std::cout << "   - Either limit was sufficient or allocation was conservative\n";
    }

    std::cout << "\nMemory cgroup characteristics:\n";
    std::cout << "- Uses page reclaim before OOM killing\n";
    std::cout << "- May allow brief excursions above limit during reclaim\n";
    std::cout << "- OOM killer targets memory-heavy processes in the cgroup\n";
    std::cout << "- memory.failcnt tracks allocation failure events\n";

    // Cleanup shared memory
    munmap(result, sizeof(AllocationResult));

    std::cout << "\n" << std::string(70, '=') << "\n";
    std::cout << "EXPERIMENT 4 COMPLETED\n";
    std::cout << std::string(70, '=') << "\n\n";
}


void experiment_5_io_limitation() {
    std::cout << "\n" << std::string(70, '=') << "\n";
    std::cout << "EXPERIMENT 5: I/O LIMITATION\n";
    std::cout << "Objective: Evaluate precision of I/O limitation\n";
    std::cout << std::string(70, '=') << "\n\n";

    const size_t IO_BLOCK_SIZE = 1024 * 1024;  // 1MB blocks
    const size_t TOTAL_DATA = 200 * 1024 * 1024;  // 200MB total
    const size_t NUM_BLOCKS = TOTAL_DATA / IO_BLOCK_SIZE;
    const std::string TEST_FILE = "/tmp/io_test_large.dat";

    std::cout << "I/O test parameters:\n";
    std::cout << "  Block size: " << (IO_BLOCK_SIZE / 1024) << " KB\n";
    std::cout << "  Total data: " << (TOTAL_DATA / (1024*1024)) << " MB\n";
    std::cout << "  Test file: " << TEST_FILE << "\n\n";

    // ========================================================================
    // STEP 1: Baseline I/O performance (no limits)
    // ========================================================================
    std::cout << "STEP 1: Baseline I/O performance (no limits)\n";
    std::cout << std::string(50, '-') << "\n";

    struct IOResult {
        double write_throughput_mbs;
        double read_throughput_mbs;
        double write_latency_ms;
        double read_latency_ms;
        long total_time_ms;
        uint64_t bytes_written;
        uint64_t bytes_read;
    };

    // I/O intensive workload
    auto io_workload = [](const std::string& filename, size_t block_size, 
                         size_t num_blocks, IOResult* result) {
        char* buffer = new char[block_size];
        memset(buffer, 0xAA, block_size);  // Fill with pattern

        auto total_start = std::chrono::high_resolution_clock::now();

        // Write test
        std::cout << "  Writing " << num_blocks << " blocks...\n";
        auto write_start = std::chrono::high_resolution_clock::now();
        
        std::ofstream outfile(filename, std::ios::binary);
        if (!outfile) {
            std::cerr << "Failed to create test file\n";
            delete[] buffer;
            return;
        }

        for (size_t i = 0; i < num_blocks; ++i) {
            outfile.write(buffer, block_size);
            if (!outfile) {
                std::cerr << "Write failed at block " << i << "\n";
                break;
            }
        }
        outfile.close();

        auto write_end = std::chrono::high_resolution_clock::now();
        long write_time_ms = std::chrono::duration_cast<std::chrono::milliseconds>(write_end - write_start).count();

        // Read test
        std::cout << "  Reading " << num_blocks << " blocks...\n";
        auto read_start = std::chrono::high_resolution_clock::now();

        std::ifstream infile(filename, std::ios::binary);
        if (!infile) {
            std::cerr << "Failed to open test file for reading\n";
            delete[] buffer;
            return;
        }

        size_t blocks_read = 0;
        while (infile.read(buffer, block_size) && blocks_read < num_blocks) {
            blocks_read++;
            // Prevent optimization
            volatile char dummy = buffer[0];
            (void)dummy;
        }
        infile.close();

        auto read_end = std::chrono::high_resolution_clock::now();
        long read_time_ms = std::chrono::duration_cast<std::chrono::milliseconds>(read_end - read_start).count();

        auto total_end = std::chrono::high_resolution_clock::now();
        long total_time_ms = std::chrono::duration_cast<std::chrono::milliseconds>(total_end - total_start).count();

        // Calculate results
        result->bytes_written = num_blocks * block_size;
        result->bytes_read = blocks_read * block_size;
        result->write_throughput_mbs = (double)(result->bytes_written) / (1024.0 * 1024.0) / (write_time_ms / 1000.0);
        result->read_throughput_mbs = (double)(result->bytes_read) / (1024.0 * 1024.0) / (read_time_ms / 1000.0);
        result->write_latency_ms = (double)write_time_ms / num_blocks;
        result->read_latency_ms = (double)read_time_ms / blocks_read;
        result->total_time_ms = total_time_ms;

        delete[] buffer;
        
        std::cout << "  Write completed: " << result->write_throughput_mbs << " MB/s\n";
        std::cout << "  Read completed: " << result->read_throughput_mbs << " MB/s\n";
    };

    // Shared memory for results
    IOResult* baseline_result = (IOResult*)mmap(NULL, sizeof(IOResult),
                                PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);

    std::cout << "Running baseline I/O test...\n";

    pid_t baseline_pid = fork();
    if (baseline_pid == 0) {
        // Child: Run I/O workload without limits
        io_workload(TEST_FILE, IO_BLOCK_SIZE, NUM_BLOCKS, baseline_result);
        exit(0);
    }

    // Monitor baseline process
    std::vector<uint64_t> baseline_read_samples, baseline_write_samples;
    uint64_t prev_read = 0, prev_write = 0;
    
    int status;
    while (waitpid(baseline_pid, &status, WNOHANG) == 0) {
        process_stats_t stats;
        if (monitor_collect(baseline_pid, &stats) == 0) {
            baseline_read_samples.push_back(stats.read_bytes - prev_read);
            baseline_write_samples.push_back(stats.write_bytes - prev_write);
            prev_read = stats.read_bytes;
            prev_write = stats.write_bytes;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
    }

    std::cout << "\nBaseline Results:\n";
    std::cout << "  Write throughput: " << std::fixed << std::setprecision(2) 
              << baseline_result->write_throughput_mbs << " MB/s\n";
    std::cout << "  Read throughput: " << baseline_result->read_throughput_mbs << " MB/s\n";
    std::cout << "  Write latency: " << baseline_result->write_latency_ms << " ms/block\n";
    std::cout << "  Read latency: " << baseline_result->read_latency_ms << " ms/block\n";
    std::cout << "  Total time: " << baseline_result->total_time_ms << " ms\n\n";

    // Cleanup baseline test file
    unlink(TEST_FILE.c_str());

    // ========================================================================
    // STEP 2: Test with I/O limitations (simulated via cgroup monitoring)
    // ========================================================================
    std::cout << "STEP 2: I/O testing with cgroup monitoring\n";
    std::cout << std::string(50, '-') << "\n";
    
    std::cout << "Note: Real I/O limits require device-specific cgroup configuration.\n";
    std::cout << "This test demonstrates I/O monitoring capabilities.\n\n";

    // Different test scenarios
    struct IOTestScenario {
        std::string name;
        size_t block_size;
        size_t delay_ms;  // Artificial delay to simulate throttling
    };

    std::vector<IOTestScenario> scenarios = {
        {"Fast I/O", IO_BLOCK_SIZE, 0},
        {"Throttled-Light", IO_BLOCK_SIZE, 10},
        {"Throttled-Medium", IO_BLOCK_SIZE, 25},
        {"Throttled-Heavy", IO_BLOCK_SIZE, 50}
    };

    std::cout << std::left 
              << std::setw(18) << "Scenario"
              << std::setw(15) << "Write MB/s"
              << std::setw(15) << "Read MB/s"
              << std::setw(15) << "Total Time"
              << std::setw(15) << "Efficiency"
              << "\n";
    std::cout << std::string(78, '-') << "\n";

    for (const auto& scenario : scenarios) {
        // Create cgroup for this test
        std::string cg_path, err;
        std::string cg_name = "io_test_" + scenario.name;
        std::replace(cg_name.begin(), cg_name.end(), ' ', '_');
        std::replace(cg_name.begin(), cg_name.end(), '-', '_');
        
        if (!cgroup::create(cg_name, cg_path, err)) {
            std::cerr << "Failed to create cgroup: " << err << "\n";
            continue;
        }

        // Modified workload with artificial throttling
        auto throttled_io_workload = [](const std::string& filename, size_t block_size, 
                                       size_t num_blocks, size_t delay_ms, IOResult* result) {
            char* buffer = new char[block_size];
            memset(buffer, 0xBB, block_size);

            auto total_start = std::chrono::high_resolution_clock::now();

            // Write test with throttling
            auto write_start = std::chrono::high_resolution_clock::now();
            std::ofstream outfile(filename, std::ios::binary);
            
            for (size_t i = 0; i < num_blocks; ++i) {
                outfile.write(buffer, block_size);
                if (delay_ms > 0) {
                    std::this_thread::sleep_for(std::chrono::milliseconds(delay_ms));
                }
            }
            outfile.close();
            auto write_end = std::chrono::high_resolution_clock::now();

            // Read test with throttling
            auto read_start = std::chrono::high_resolution_clock::now();
            std::ifstream infile(filename, std::ios::binary);
            
            size_t blocks_read = 0;
            while (infile.read(buffer, block_size) && blocks_read < num_blocks) {
                blocks_read++;
                if (delay_ms > 0) {
                    std::this_thread::sleep_for(std::chrono::milliseconds(delay_ms));
                }
                volatile char dummy = buffer[0];
                (void)dummy;
            }
            infile.close();
            auto read_end = std::chrono::high_resolution_clock::now();

            auto total_end = std::chrono::high_resolution_clock::now();

            // Calculate times
            long write_time_ms = std::chrono::duration_cast<std::chrono::milliseconds>(write_end - write_start).count();
            long read_time_ms = std::chrono::duration_cast<std::chrono::milliseconds>(read_end - read_start).count();
            long total_time_ms = std::chrono::duration_cast<std::chrono::milliseconds>(total_end - total_start).count();

            result->bytes_written = num_blocks * block_size;
            result->bytes_read = blocks_read * block_size;
            result->write_throughput_mbs = (double)(result->bytes_written) / (1024.0 * 1024.0) / (write_time_ms / 1000.0);
            result->read_throughput_mbs = (double)(result->bytes_read) / (1024.0 * 1024.0) / (read_time_ms / 1000.0);
            result->total_time_ms = total_time_ms;

            delete[] buffer;
        };

        IOResult* test_result = (IOResult*)mmap(NULL, sizeof(IOResult),
                                PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);

        std::string test_filename = TEST_FILE + "_" + scenario.name;
        
        pid_t test_pid = fork();
        if (test_pid == 0) {
            // Child: Move to cgroup and run I/O workload
            if (!cgroup::move_pid(cg_path, getpid(), err)) {
                std::cerr << "Failed to move to cgroup: " << err << "\n";
                exit(1);
            }

            throttled_io_workload(test_filename, scenario.block_size, NUM_BLOCKS, scenario.delay_ms, test_result);
            exit(0);
        }

        // Monitor the test process
        std::vector<double> io_rate_samples;
        prev_read = prev_write = 0;
        
        while (waitpid(test_pid, &status, WNOHANG) == 0) {
            process_stats_t stats;
            if (monitor_collect(test_pid, &stats) == 0) {
                uint64_t read_delta = stats.read_bytes - prev_read;
                uint64_t write_delta = stats.write_bytes - prev_write;
                double total_rate = (read_delta + write_delta) / (1024.0 * 1024.0) / 0.5; // MB/s over 0.5s interval
                io_rate_samples.push_back(total_rate);
                prev_read = stats.read_bytes;
                prev_write = stats.write_bytes;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
        }

        // Calculate efficiency compared to baseline
        double time_efficiency = (double)baseline_result->total_time_ms / test_result->total_time_ms * 100.0;
        
        std::cout << std::left 
                  << std::setw(18) << scenario.name
                  << std::setw(15) << std::fixed << std::setprecision(1) << test_result->write_throughput_mbs
                  << std::setw(15) << std::fixed << std::setprecision(1) << test_result->read_throughput_mbs
                  << std::setw(15) << test_result->total_time_ms << " ms"
                  << std::setw(15) << std::fixed << std::setprecision(1) << time_efficiency << "%"
                  << "\n";

        // Cleanup
        unlink(test_filename.c_str());
        munmap(test_result, sizeof(IOResult));
    }

    // ========================================================================
    // STEP 3: Detailed I/O analysis
    // ========================================================================
    std::cout << "\n\nSTEP 3: Detailed I/O pattern analysis\n";
    std::cout << std::string(50, '-') << "\n";

    std::cout << "Running detailed I/O monitoring test...\n";
    
    std::string cg_path, err;
    if (cgroup::create("io_detailed", cg_path, err)) {
        pid_t detailed_pid = fork();
        if (detailed_pid == 0) {
            cgroup::move_pid(cg_path, getpid(), err);
            
            // Run a smaller I/O test for detailed monitoring
            char* buffer = new char[IO_BLOCK_SIZE];
            memset(buffer, 0xCC, IO_BLOCK_SIZE);
            
            std::string detailed_file = "/tmp/io_detailed_test.dat";
            std::ofstream outfile(detailed_file, std::ios::binary);
            
            for (int i = 0; i < 50; ++i) {  // Smaller test
                outfile.write(buffer, IO_BLOCK_SIZE);
                outfile.flush();  // Force write to disk
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
            }
            outfile.close();
            
            delete[] buffer;
            unlink(detailed_file.c_str());
            exit(0);
        }

        // Monitor with high frequency
        std::cout << std::left 
                  << std::setw(8) << "Time(s)"
                  << std::setw(12) << "Write KB/s"
                  << std::setw(12) << "Read KB/s"
                  << std::setw(15) << "Total I/O KB"
                  << "\n";
        std::cout << std::string(47, '-') << "\n";

        auto monitor_start = std::chrono::steady_clock::now();
        prev_read = prev_write = 0;
        
        while (waitpid(detailed_pid, &status, WNOHANG) == 0) {
            auto now = std::chrono::steady_clock::now();
            double elapsed = std::chrono::duration<double>(now - monitor_start).count();
            
            process_stats_t stats;
            if (monitor_collect(detailed_pid, &stats) == 0) {
                uint64_t read_rate = (stats.read_bytes - prev_read) / 1024 / 0.2;  // KB/s
                uint64_t write_rate = (stats.write_bytes - prev_write) / 1024 / 0.2;  // KB/s
                uint64_t total_io = (stats.read_bytes + stats.write_bytes) / 1024;  // Total KB
                
                std::cout << std::left 
                          << std::setw(8) << std::fixed << std::setprecision(1) << elapsed
                          << std::setw(12) << write_rate
                          << std::setw(12) << read_rate
                          << std::setw(15) << total_io
                          << "\n";
                
                prev_read = stats.read_bytes;
                prev_write = stats.write_bytes;
            }
            
            std::this_thread::sleep_for(std::chrono::milliseconds(200));
        }
    }

    // ========================================================================
    // STEP 4: Summary
    // ========================================================================
    std::cout << "\n\nSTEP 4: I/O limitation analysis summary\n";
    std::cout << std::string(50, '-') << "\n";

    std::cout << "Key findings:\n";
    std::cout << "1. Baseline I/O performance: " << baseline_result->write_throughput_mbs << " MB/s write, "
              << baseline_result->read_throughput_mbs << " MB/s read\n";
    std::cout << "2. Artificial throttling demonstrates measurable impact on throughput\n";
    std::cout << "3. I/O monitoring provides detailed insights into access patterns\n";
    std::cout << "4. Real I/O limits require device-specific cgroup configuration\n";

    std::cout << "\nI/O cgroup limitations in test environment:\n";
    std::cout << "- Real I/O throttling requires blkio controller with device mapping\n";
    std::cout << "- Container environments may not expose block devices directly\n";
    std::cout << "- This test demonstrates monitoring capabilities and simulated throttling\n";

    std::cout << "\nFor production I/O limiting:\n";
    std::cout << "- Use blkio.throttle.read_bps_device and blkio.throttle.write_bps_device\n";
    std::cout << "- Requires major:minor device numbers\n";
    std::cout << "- Configure at cgroup creation time\n";

    // Cleanup
    munmap(baseline_result, sizeof(IOResult));
    unlink(TEST_FILE.c_str());

    std::cout << "\n" << std::string(70, '=') << "\n";
    std::cout << "EXPERIMENT 5 COMPLETED\n";
    std::cout << std::string(70, '=') << "\n\n";
}


int main() {
    std::cout << "LINUX RESOURCE MONITORING - NAMESPACE AND OVERHEAD ANALYSIS\n";
    
    // Check if we have necessary permissions
    if (getuid() != 0) {
        std::cout << "Note: Running as non-root user. Some namespace operations may be limited.\n";
    }
    
    experiment_1_monitoring_overhead();
    experiment_2_namespace_isolation();
    experiment_3_cpu_throttling();
    experiment_4_memory_limitation();
    experiment_5_io_limitation();
    
    return 0;
}