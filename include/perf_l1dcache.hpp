/*
 * L1-dcache Performance Counter Wrapper
 *
 * RAII wrapper for Linux perf_event counters measuring L1 data cache
 * loads and misses. Used to demonstrate SBBF's cache efficiency
 * during spatial traversals.
 *
 * Requirements:
 *   - Linux with perf_event support
 *   - kernel.perf_event_paranoid <= 1 (or root)
 *     Run: sudo sysctl kernel.perf_event_paranoid=1
 *
 * Usage:
 *   L1DCachePerfCounters counters;
 *   if (!counters.init()) {
 *       std::cerr << "Failed to init perf counters\n";
 *       return;
 *   }
 *   counters.reset();
 *   counters.start();
 *   // ... code to measure ...
 *   counters.stop();
 *   auto result = counters.read();
 *   std::cout << "L1D hit rate: " << result.hit_rate_pct() << "%\n";
 */

#pragma once

#include <cstdint>
#include <cstring>
#include <iostream>
#include <string>

#ifdef __linux__
#include <linux/perf_event.h>
#include <sys/ioctl.h>
#include <sys/syscall.h>
#include <unistd.h>
#endif

namespace perf {

struct L1DCacheCounters {
    uint64_t loads = 0;   // Total L1D cache read accesses
    uint64_t misses = 0;  // L1D cache read misses

    double hit_rate_pct() const {
        if (loads == 0) return 0.0;
        return (double)(loads - misses) / loads * 100.0;
    }

    double miss_rate_pct() const {
        if (loads == 0) return 0.0;
        return (double)misses / loads * 100.0;
    }

    uint64_t hits() const {
        return loads > misses ? loads - misses : 0;
    }
};

#ifdef __linux__

class L1DCachePerfCounters {
public:
    L1DCachePerfCounters() = default;

    ~L1DCachePerfCounters() {
        if (fd_loads_ >= 0) close(fd_loads_);
        if (fd_misses_ >= 0) close(fd_misses_);
    }

    // Non-copyable
    L1DCachePerfCounters(const L1DCachePerfCounters&) = delete;
    L1DCachePerfCounters& operator=(const L1DCachePerfCounters&) = delete;

    // Movable
    L1DCachePerfCounters(L1DCachePerfCounters&& other) noexcept
        : fd_loads_(other.fd_loads_), fd_misses_(other.fd_misses_) {
        other.fd_loads_ = -1;
        other.fd_misses_ = -1;
    }

    L1DCachePerfCounters& operator=(L1DCachePerfCounters&& other) noexcept {
        if (this != &other) {
            if (fd_loads_ >= 0) close(fd_loads_);
            if (fd_misses_ >= 0) close(fd_misses_);
            fd_loads_ = other.fd_loads_;
            fd_misses_ = other.fd_misses_;
            other.fd_loads_ = -1;
            other.fd_misses_ = -1;
        }
        return *this;
    }

    bool init() {
        // Check perf_event_paranoid
        if (!check_paranoid_level()) {
            return false;
        }

        // Open L1D cache loads counter
        fd_loads_ = open_cache_counter(
            PERF_COUNT_HW_CACHE_L1D,
            PERF_COUNT_HW_CACHE_OP_READ,
            PERF_COUNT_HW_CACHE_RESULT_ACCESS
        );
        if (fd_loads_ < 0) {
            std::cerr << "perf_l1dcache: Failed to open L1D loads counter\n";
            return false;
        }

        // Open L1D cache misses counter (grouped with loads)
        fd_misses_ = open_cache_counter(
            PERF_COUNT_HW_CACHE_L1D,
            PERF_COUNT_HW_CACHE_OP_READ,
            PERF_COUNT_HW_CACHE_RESULT_MISS,
            fd_loads_  // group leader
        );
        if (fd_misses_ < 0) {
            std::cerr << "perf_l1dcache: Failed to open L1D misses counter\n";
            close(fd_loads_);
            fd_loads_ = -1;
            return false;
        }

        return true;
    }

    bool is_valid() const {
        return fd_loads_ >= 0 && fd_misses_ >= 0;
    }

    void start() {
        if (!is_valid()) return;
        ioctl(fd_loads_, PERF_EVENT_IOC_ENABLE, PERF_IOC_FLAG_GROUP);
    }

    void stop() {
        if (!is_valid()) return;
        ioctl(fd_loads_, PERF_EVENT_IOC_DISABLE, PERF_IOC_FLAG_GROUP);
    }

    void reset() {
        if (!is_valid()) return;
        ioctl(fd_loads_, PERF_EVENT_IOC_RESET, PERF_IOC_FLAG_GROUP);
    }

    L1DCacheCounters read() {
        L1DCacheCounters result;
        if (!is_valid()) return result;

        if (::read(fd_loads_, &result.loads, sizeof(result.loads)) != sizeof(result.loads)) {
            result.loads = 0;
        }
        if (::read(fd_misses_, &result.misses, sizeof(result.misses)) != sizeof(result.misses)) {
            result.misses = 0;
        }

        return result;
    }

    static std::string get_requirements_message() {
        return "L1-dcache counters require:\n"
               "  1. Linux kernel with perf_event support\n"
               "  2. kernel.perf_event_paranoid <= 1\n"
               "     Run: sudo sysctl kernel.perf_event_paranoid=1\n"
               "  3. CPU with hardware performance counters\n";
    }

private:
    int fd_loads_ = -1;
    int fd_misses_ = -1;

    static long perf_event_open(struct perf_event_attr* attr, pid_t pid,
                                int cpu, int group_fd, unsigned long flags) {
        return syscall(__NR_perf_event_open, attr, pid, cpu, group_fd, flags);
    }

    int open_cache_counter(uint64_t cache_id, uint64_t cache_op,
                           uint64_t cache_result, int group_fd = -1) {
        struct perf_event_attr pea;
        memset(&pea, 0, sizeof(pea));

        pea.type = PERF_TYPE_HW_CACHE;
        pea.size = sizeof(pea);
        pea.config = cache_id |
                     (cache_op << 8) |
                     (cache_result << 16);
        pea.disabled = 1;
        pea.exclude_kernel = 1;  // Only measure user-space
        pea.exclude_hv = 1;      // Exclude hypervisor

        int fd = perf_event_open(&pea, 0, -1, group_fd, 0);
        return fd;
    }

    bool check_paranoid_level() {
        FILE* f = fopen("/proc/sys/kernel/perf_event_paranoid", "r");
        if (!f) {
            std::cerr << "perf_l1dcache: Cannot read perf_event_paranoid\n";
            return false;
        }
        int level = 3;
        if (fscanf(f, "%d", &level) != 1) {
            level = 3;
        }
        fclose(f);

        if (level > 1) {
            std::cerr << "perf_l1dcache: kernel.perf_event_paranoid=" << level
                      << " (need <= 1)\n"
                      << "Run: sudo sysctl kernel.perf_event_paranoid=1\n";
            return false;
        }
        return true;
    }
};

#else  // Non-Linux systems

class L1DCachePerfCounters {
public:
    bool init() {
        std::cerr << "perf_l1dcache: Only supported on Linux\n";
        return false;
    }

    bool is_valid() const { return false; }
    void start() {}
    void stop() {}
    void reset() {}
    L1DCacheCounters read() { return {}; }

    static std::string get_requirements_message() {
        return "L1-dcache counters only supported on Linux.\n";
    }
};

#endif  // __linux__

// RAII helper for scoped measurement
class ScopedL1DCacheMeasurement {
public:
    explicit ScopedL1DCacheMeasurement(L1DCachePerfCounters& counters)
        : counters_(counters) {
        counters_.reset();
        counters_.start();
    }

    ~ScopedL1DCacheMeasurement() {
        counters_.stop();
    }

    // Non-copyable, non-movable
    ScopedL1DCacheMeasurement(const ScopedL1DCacheMeasurement&) = delete;
    ScopedL1DCacheMeasurement& operator=(const ScopedL1DCacheMeasurement&) = delete;

private:
    L1DCachePerfCounters& counters_;
};

}  // namespace perf
