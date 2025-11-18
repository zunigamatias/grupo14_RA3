#include "../include/monitor.h"
#include <fstream>
#include <iomanip>
#include <ctime>
#include <vector>        // Missing include
#include <iostream>      // Missing include
#include <chrono>        // Missing include
#include <thread> 

bool cpu_collect_stats(int pid, process_stats_t *stats);
bool memory_collect_stats(int pid, process_stats_t *stats);
bool io_collect_stats(int pid, process_stats_t *stats);
bool network_collect_stats(int pid, process_stats_t *stats);

int monitor_collect(int pid, process_stats_t *stats) {
    if (!stats) return -1;

    *stats = process_stats_t();

    if (!cpu_collect_stats(pid, stats)) return -1;

    memory_collect_stats(pid, stats);
    io_collect_stats(pid, stats);
    network_collect_stats(pid, stats);

    return 0;
}

bool export_to_json(const std::vector<process_stats_t>& data, int pid, const std::string& filename) {
    std::ofstream file(filename);
    if (!file.is_open()) return false;
    
    file << "{\n";
    file << "  \"process_id\": " << pid << ",\n";
    file << "  \"collection_time\": \"" << std::time(nullptr) << "\",\n";
    file << "  \"samples\": [\n";
    
    for (size_t i = 0; i < data.size(); ++i) {
        const auto& stats = data[i];
        file << "    {\n";
        file << "      \"timestamp\": " << i << ",\n";
        file << "      \"cpu_percent\": " << std::fixed << std::setprecision(2) << stats.cpu_percent << ",\n";
        file << "      \"utime_ticks\": " << stats.utime_ticks << ",\n";
        file << "      \"stime_ticks\": " << stats.stime_ticks << ",\n";
        file << "      \"rss_bytes\": " << stats.rss << ",\n";
        file << "      \"vsz_bytes\": " << stats.vsz << ",\n";
        file << "      \"read_bytes\": " << stats.read_bytes << ",\n";
        file << "      \"write_bytes\": " << stats.write_bytes << ",\n";
        file << "      \"net_rx_bytes\": " << stats.net_rx_bytes << ",\n";
        file << "      \"net_tx_bytes\": " << stats.net_tx_bytes << "\n";
        file << "    }";
        if (i < data.size() - 1) file << ",";
        file << "\n";
    }
    
    file << "  ]\n";
    file << "}\n";
    
    return true;
}


bool export_to_csv(const std::vector<process_stats_t>& data, int pid, const std::string& filename) {
    std::ofstream file(filename);
    if (!file.is_open()) return false;
    
    // CSV header
    file << "timestamp,pid,cpu_percent,utime_ticks,stime_ticks,rss_bytes,vsz_bytes,read_bytes,write_bytes,net_rx_bytes,net_tx_bytes\n";
    
    // Data rows
    for (size_t i = 0; i < data.size(); ++i) {
        const auto& stats = data[i];
        file << i << "," << pid << "," 
             << std::fixed << std::setprecision(2) << stats.cpu_percent << ","
             << stats.utime_ticks << ","
             << stats.stime_ticks << ","
             << stats.rss << ","
             << stats.vsz << ","
             << stats.read_bytes << ","
             << stats.write_bytes << ","
             << stats.net_rx_bytes << ","
             << stats.net_tx_bytes << "\n";
    }
    
    return true;
}


// Enhanced monitoring with data collection and export
int monitor_process_with_export(int pid, int duration_seconds, int interval_ms, 
                               const std::string& output_format) {
    std::vector<process_stats_t> collected_data;
    
    auto end_time = std::chrono::steady_clock::now() + std::chrono::seconds(duration_seconds);
    
    std::cout << "Monitoring PID " << pid << " for " << duration_seconds << " seconds...\n";
    std::cout << std::left << std::setw(8) << "Time" << std::setw(10) << "CPU%" 
              << std::setw(12) << "RSS(MB)" << std::setw(12) << "Read(KB/s)" 
              << std::setw(12) << "Write(KB/s)" << "\n";
    std::cout << std::string(54, '-') << "\n";
    
    process_stats_t prev_stats = {};
    bool first_sample = true;
    int sample_count = 0;
    
    while (std::chrono::steady_clock::now() < end_time) {
        process_stats_t stats;
        
        if (monitor_collect(pid, &stats) != 0) {
            std::cerr << "Failed to collect stats for PID " << pid 
                      << " (process may have exited)\n";
            break;
        }
        
        // Calculate I/O rates
        double read_rate = 0, write_rate = 0;
        if (!first_sample) {
            read_rate = (double)(stats.read_bytes - prev_stats.read_bytes) / 1024.0 / (interval_ms / 1000.0);
            write_rate = (double)(stats.write_bytes - prev_stats.write_bytes) / 1024.0 / (interval_ms / 1000.0);
        }
        
        // Display current stats
        std::cout << std::left << std::setw(8) << sample_count
                  << std::setw(10) << std::fixed << std::setprecision(1) << stats.cpu_percent
                  << std::setw(12) << (stats.rss / (1024*1024))
                  << std::setw(12) << std::fixed << std::setprecision(1) << read_rate
                  << std::setw(12) << std::fixed << std::setprecision(1) << write_rate
                  << "\n";
        
        collected_data.push_back(stats);
        prev_stats = stats;
        first_sample = false;
        sample_count++;
        
        std::this_thread::sleep_for(std::chrono::milliseconds(interval_ms));
    }
    
    // Export data
    std::string filename;
    bool export_success = false;
    
    if (output_format == "json") {
        filename = "monitor_pid_" + std::to_string(pid) + "_" + std::to_string(std::time(nullptr)) + ".json";
        export_success = export_to_json(collected_data, pid, filename);
    } else if (output_format == "csv") {
        filename = "monitor_pid_" + std::to_string(pid) + "_" + std::to_string(std::time(nullptr)) + ".csv";
        export_success = export_to_csv(collected_data, pid, filename);
    }
    
    if (export_success) {
        std::cout << "\nData exported to: " << filename << "\n";
    } else {
        std::cerr << "Failed to export data to " << filename << "\n";
    }
    
    return collected_data.size();
}
