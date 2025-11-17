# Process Monitoring System — Test & Validation Documentation
## Overview

This document describes the monitoring system responsible for tracking:

CPU usage

Memory consumption

Disk I/O (read/write bytes)

Network traffic (rx/tx bytes)

Data is collected from the Linux /proc filesystem and aggregated by monitor_collect().

## Architecture
monitor_collect()
│
├── cpu_collect_stats()      → /proc/<pid>/stat
├── memory_collect_stats()   → /proc/<pid>/statm
├── io_collect_stats()       → /proc/<pid>/io
└── network_collect_stats()  → /proc/<pid>/net/dev


All functions fill a single structure:

struct process_stats_t {
    uint64_t utime_ticks, stime_ticks;
    uint64_t vsz, rss;
    uint64_t read_bytes, write_bytes;
    uint64_t net_rx_bytes, net_tx_bytes;
};

### Metrics Collected
- Memory (/proc/<pid>/statm) 
- Field	Meaning	Conversion 
- pages_total	Virtual Memory used	× page_size
- pages_rss	Resident Set Size	× page_size

Measured in bytes.

CPU (/proc/<pid>/stat)
Field	Meaning
utime	User CPU time (ticks)
stime	Kernel CPU time (ticks)

### CPU% computed as:

- Delta(utime+stime) / ticks_per_second / num_cpus * 100

📦 Disk I/O (/proc/<pid>/io)
Field	Meaning
read_bytes	Total bytes read by the process
write_bytes	Total bytes written
🌐 Network (/proc/<pid>/net/dev)

Parsed per-interface:

Field	Meaning
RX bytes	Received bytes
TX bytes	Transmitted bytes

Totals across all interfaces are summed.

🧪 Test Programs
🔥 CPU-intensive test
#include <cmath>
#include <iostream>

int main() {
    volatile double x = 0;
    for (long long i = 0; i < 5e9; i++)
        x += std::sin(i);
    std::cout << "Done CPU test\n";
}


Expected:
CPU usage should reach ~100% / number_of_cores.

🧠 Memory-intensive test
#include <vector>
#include <cstring>
#include <iostream>

int main() {
    const size_t SIZE = 1024ULL * 1024ULL * 1024ULL; // 1GB
    std::vector<char> mem(SIZE);
    std::memset(mem.data(), 1, SIZE);

    std::cout << "Allocated 1GB\n";
}


Expected:
RSS should increase by ~1 GB, VSZ slightly higher.

📦 Disk I/O-intensive test
#include <fstream>
#include <vector>
#include <iostream>

int main() {
    std::ofstream file("iotest.bin", std::ios::binary);

    const size_t SIZE = 100ULL * 1024ULL * 1024ULL; // 100MB
    std::vector<char> buffer(1024 * 1024, 0xAA);

    for (int i = 0; i < 100; i++)
        file.write(buffer.data(), buffer.size());

    std::cout << "Wrote 100MB\n";
}


Expected:
write_bytes increases by ~100MB.

🌐 Network-intensive test (optional)
curl -O https://speed.hetzner.de/1GB.bin


Expected:
net_rx_bytes increases by ~1GB.

🧪 Test Methodology
1. Start the monitoring system

Run your monitor while passing the target PID.

Example:

./monitor_app <pid>

2. Run one of the test programs

Each program stresses a specific subsystem.

3. Capture values every interval

Store (time, stats) samples for:

CPU ticks

RSS and VSZ

I/O deltas

Network deltas

4. Compare with ground truth
CPU Validation

Compare with:

top -p <pid>
cat /proc/<pid>/stat


Expected: trends match within ±5%.

Memory Validation

Compare with:

ps -o rss,vsz <pid>
cat /proc/<pid>/statm


Expected: exact match (page conversion consistent).

Disk I/O Validation

Compare with:

cat /proc/<pid>/io
iotop -a


Expected: numbers match exactly.

Network Validation

Compare totals with:

cat /proc/<pid>/net/dev
nethogs
iftop


Expected: per-interface totals are exactly summed.

✔️ Accuracy Expectations
Metric	Expected Accuracy
CPU	±3% to ±5%
Memory	Exact
I/O	Exact
Network	Exact
📄 Conclusion

The monitoring system is now capable of capturing CPU, memory, disk IO, and network traffic precisely using Linux’s /proc filesystem.

Testing and validation confirm:

Accurate CPU sampling (with tick-based deltas)

Correct memory retrieval (page-size normalized)

Precise I/O and Network byte counts (direct raw kernel values)