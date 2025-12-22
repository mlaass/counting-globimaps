/*
 * Hash Function Benchmark
 *
 * Compares hash function performance:
 * - MurmurHash3 (current default)
 * - XXH3 (vectorized, 30+ GB/s)
 * - wyhash (simple, 25 GB/s)
 *
 * Key metrics (via hardware performance counters):
 * - Nanoseconds per hash
 * - CPU cycles per hash
 * - Instructions per hash
 * - Throughput (GB/s)
 * - Instructions per cycle (IPC)
 *
 * Requirements:
 * - Linux with perf_event support
 * - May need: sudo sysctl kernel.perf_event_paranoid=1
 */

#define ANKERL_NANOBENCH_IMPLEMENT
#include "external/nanobench.h"

// Hash function implementations
#include "murmur.hpp"
#define XXH_INLINE_ALL
#include "xxhash.h"
#include "wyhash.h"

#include <chrono>
#include <cmath>
#include <cstring>
#include <iomanip>
#include <iostream>
#include <random>
#include <string>
#include <vector>

// ============================================================================
// Configuration
// ============================================================================

struct BenchmarkConfig {
    size_t epochs = 11;
    size_t min_iters = 10000;      // More iterations for fast hash functions
    size_t warmup = 1000;
    size_t epoch_time_ms = 100;
    std::string scenario = "all";
    size_t data_size = 100000;

    void print() const {
        std::cout << "Benchmark configuration:\n";
        std::cout << "  Epochs: " << epochs << "\n";
        std::cout << "  Min iterations/epoch: " << min_iters << "\n";
        std::cout << "  Warmup iterations: " << warmup << "\n";
        std::cout << "  Max epoch time: " << epoch_time_ms << " ms\n";
        std::cout << "  Data size: " << data_size << "\n";
        std::cout << "  Scenario: " << scenario << "\n";
    }
};

BenchmarkConfig g_config;

void print_usage(const char* prog) {
    std::cout << "Usage: " << prog << " [options]\n\n";
    std::cout << "Options:\n";
    std::cout << "  --epochs N        Number of epochs per benchmark (default: 11)\n";
    std::cout << "  --min-iters N     Minimum iterations per epoch (default: 10000)\n";
    std::cout << "  --warmup N        Warmup iterations (default: 1000)\n";
    std::cout << "  --epoch-time MS   Max time per epoch in ms (default: 100)\n";
    std::cout << "  --data-size N     Number of data points (default: 100000)\n";
    std::cout << "  --scenario S      Run only: hash, sizes, all (default: all)\n";
    std::cout << "  --help            Show this help\n";
}

BenchmarkConfig parse_args(int argc, char* argv[]) {
    BenchmarkConfig config;

    for (int i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "--help") == 0) {
            print_usage(argv[0]);
            exit(0);
        } else if (strcmp(argv[i], "--epochs") == 0 && i + 1 < argc) {
            config.epochs = std::stoul(argv[++i]);
        } else if (strcmp(argv[i], "--min-iters") == 0 && i + 1 < argc) {
            config.min_iters = std::stoul(argv[++i]);
        } else if (strcmp(argv[i], "--warmup") == 0 && i + 1 < argc) {
            config.warmup = std::stoul(argv[++i]);
        } else if (strcmp(argv[i], "--epoch-time") == 0 && i + 1 < argc) {
            config.epoch_time_ms = std::stoul(argv[++i]);
        } else if (strcmp(argv[i], "--data-size") == 0 && i + 1 < argc) {
            config.data_size = std::stoul(argv[++i]);
        } else if (strcmp(argv[i], "--scenario") == 0 && i + 1 < argc) {
            config.scenario = argv[++i];
        } else {
            std::cerr << "Unknown option: " << argv[i] << "\n";
            print_usage(argv[0]);
            exit(1);
        }
    }

    return config;
}

// ============================================================================
// Result Storage
// ============================================================================

struct HashResult {
    std::string name;
    size_t input_bytes;
    double ns_per_hash;
    uint64_t instructions;
    uint64_t cycles;
    double throughput_gbps;
    double ipc;
};

std::vector<HashResult> g_results;

// ============================================================================
// Data Generation
// ============================================================================

struct Point2D {
    uint64_t x, y;
};

struct Point3D {
    uint64_t x, y, z;
};

struct Point4D {
    uint64_t x, y, z, w;
};

std::vector<Point2D> generate_2d(size_t n, uint64_t seed) {
    std::mt19937_64 rng(seed);
    std::vector<Point2D> points;
    points.reserve(n);
    for (size_t i = 0; i < n; ++i) {
        points.push_back({rng(), rng()});
    }
    return points;
}

std::vector<Point3D> generate_3d(size_t n, uint64_t seed) {
    std::mt19937_64 rng(seed);
    std::vector<Point3D> points;
    points.reserve(n);
    for (size_t i = 0; i < n; ++i) {
        points.push_back({rng(), rng(), rng()});
    }
    return points;
}

std::vector<Point4D> generate_4d(size_t n, uint64_t seed) {
    std::mt19937_64 rng(seed);
    std::vector<Point4D> points;
    points.reserve(n);
    for (size_t i = 0; i < n; ++i) {
        points.push_back({rng(), rng(), rng(), rng()});
    }
    return points;
}

// ============================================================================
// Hash Function Wrappers
// ============================================================================

static constexpr uint64_t SEED1 = 0x9e3779b97f4a7c15ULL;
static constexpr uint64_t SEED2 = 0x85ebca6b517d3b25ULL;

// MurmurHash3
inline void hash_murmur(const void* data, size_t len, uint64_t* h1, uint64_t* h2) {
    uint64_t out[2];
    murmur::MurmurHash3_x64_128(data, len, *h1, out);
    *h1 = out[0];
    *h2 = out[1];
}

// XXH3 (128-bit)
inline void hash_xxh3(const void* data, size_t len, uint64_t* h1, uint64_t* h2) {
    XXH128_hash_t h = XXH3_128bits_withSeed(data, len, *h1);
    *h1 = h.low64;
    *h2 = h.high64;
}

// wyhash (two 64-bit hashes)
inline void hash_wyhash(const void* data, size_t len, uint64_t* h1, uint64_t* h2) {
    *h1 = wyhash(data, len, *h1, _wyp);
    *h2 = wyhash(data, len, *h2, _wyp);
}

// ============================================================================
// Benchmark Functions
// ============================================================================

template<typename PointType, typename HashFn>
void benchmark_hash(ankerl::nanobench::Bench& bench,
                    const std::string& name,
                    const std::vector<PointType>& data,
                    HashFn hash_fn) {
    size_t idx = 0;
    bench.run(name, [&]() {
        uint64_t h1 = SEED1, h2 = SEED2;
        const auto& p = data[idx++ % data.size()];
        hash_fn(&p, sizeof(PointType), &h1, &h2);
        ankerl::nanobench::doNotOptimizeAway(h1);
        ankerl::nanobench::doNotOptimizeAway(h2);
    });

    // Extract results
    auto& res = bench.results().back();
    HashResult hr;
    hr.name = name;
    hr.input_bytes = sizeof(PointType);
    hr.ns_per_hash = res.median(ankerl::nanobench::Result::Measure::elapsed) * 1e9;
    hr.instructions = static_cast<uint64_t>(
        res.median(ankerl::nanobench::Result::Measure::instructions));
    hr.cycles = static_cast<uint64_t>(
        res.median(ankerl::nanobench::Result::Measure::cpucycles));
    hr.throughput_gbps = (sizeof(PointType) / hr.ns_per_hash);  // bytes/ns = GB/s
    hr.ipc = hr.cycles > 0 ? static_cast<double>(hr.instructions) / hr.cycles : 0;

    g_results.push_back(hr);
}

// ============================================================================
// Table Printing
// ============================================================================

void print_hash_table() {
    if (g_results.empty()) return;

    std::cout << "\n";
    std::cout << std::left << std::setw(20) << "Hash Function"
              << std::right
              << std::setw(10) << "Input"
              << std::setw(12) << "ns/hash"
              << std::setw(10) << "ins"
              << std::setw(10) << "cyc"
              << std::setw(10) << "GB/s"
              << std::setw(8) << "IPC" << "\n";
    std::cout << std::string(80, '-') << "\n";

    for (const auto& r : g_results) {
        std::cout << std::left << std::setw(20) << r.name
                  << std::right
                  << std::setw(8) << r.input_bytes << " B"
                  << std::setw(10) << std::fixed << std::setprecision(2) << r.ns_per_hash
                  << std::setw(10) << r.instructions
                  << std::setw(10) << r.cycles
                  << std::setw(10) << std::setprecision(2) << r.throughput_gbps
                  << std::setw(8) << std::setprecision(2) << r.ipc << "\n";
    }
    std::cout << "\n";
}

void print_size_comparison_table() {
    // Group results by hash function
    std::cout << "\n";
    std::cout << "Input Size Sensitivity (ns/hash):\n";
    std::cout << std::string(60, '-') << "\n";
    std::cout << std::left << std::setw(15) << "Function"
              << std::right
              << std::setw(12) << "16 B"
              << std::setw(12) << "24 B"
              << std::setw(12) << "32 B" << "\n";
    std::cout << std::string(60, '-') << "\n";

    // Find results for each hash function
    auto find_result = [](const std::string& prefix, size_t bytes) -> double {
        for (const auto& r : g_results) {
            if (r.name.find(prefix) == 0 && r.input_bytes == bytes) {
                return r.ns_per_hash;
            }
        }
        return 0.0;
    };

    for (const auto& prefix : {"MurmurHash3", "XXH3", "wyhash"}) {
        std::cout << std::left << std::setw(15) << prefix
                  << std::right << std::fixed << std::setprecision(2)
                  << std::setw(10) << find_result(prefix, 16) << " ns"
                  << std::setw(10) << find_result(prefix, 24) << " ns"
                  << std::setw(10) << find_result(prefix, 32) << " ns" << "\n";
    }
    std::cout << "\n";
}

// ============================================================================
// Benchmark Scenarios
// ============================================================================

void run_hash_comparison() {
    std::cout << "\n===== Hash Function Comparison (16-byte input) =====\n";

    auto data = generate_2d(g_config.data_size, 42);

    ankerl::nanobench::Bench bench;
    bench.performanceCounters(true)
         .epochs(g_config.epochs)
         .warmup(g_config.warmup)
         .minEpochIterations(g_config.min_iters)
         .maxEpochTime(std::chrono::milliseconds(g_config.epoch_time_ms))
         .relative(true);

    benchmark_hash(bench, "MurmurHash3", data, hash_murmur);
    benchmark_hash(bench, "XXH3", data, hash_xxh3);
    benchmark_hash(bench, "wyhash", data, hash_wyhash);

    print_hash_table();
    g_results.clear();
}

void run_size_comparison() {
    std::cout << "\n===== Input Size Sensitivity =====\n";

    auto data_2d = generate_2d(g_config.data_size, 42);
    auto data_3d = generate_3d(g_config.data_size, 42);
    auto data_4d = generate_4d(g_config.data_size, 42);

    ankerl::nanobench::Bench bench;
    bench.performanceCounters(true)
         .epochs(g_config.epochs)
         .warmup(g_config.warmup)
         .minEpochIterations(g_config.min_iters)
         .maxEpochTime(std::chrono::milliseconds(g_config.epoch_time_ms));

    // MurmurHash3 at different sizes
    benchmark_hash(bench, "MurmurHash3 (2D)", data_2d, hash_murmur);
    benchmark_hash(bench, "MurmurHash3 (3D)", data_3d, hash_murmur);
    benchmark_hash(bench, "MurmurHash3 (4D)", data_4d, hash_murmur);

    // XXH3 at different sizes
    benchmark_hash(bench, "XXH3 (2D)", data_2d, hash_xxh3);
    benchmark_hash(bench, "XXH3 (3D)", data_3d, hash_xxh3);
    benchmark_hash(bench, "XXH3 (4D)", data_4d, hash_xxh3);

    // wyhash at different sizes
    benchmark_hash(bench, "wyhash (2D)", data_2d, hash_wyhash);
    benchmark_hash(bench, "wyhash (3D)", data_3d, hash_wyhash);
    benchmark_hash(bench, "wyhash (4D)", data_4d, hash_wyhash);

    print_hash_table();
    g_results.clear();
}

// ============================================================================
// Main
// ============================================================================

int main(int argc, char* argv[]) {
    g_config = parse_args(argc, argv);

    std::cout << "===== Hash Function Benchmark =====\n\n";
    g_config.print();
    std::cout << "\n";

    bool run_hash = (g_config.scenario == "all" || g_config.scenario == "hash");
    bool run_sizes = (g_config.scenario == "all" || g_config.scenario == "sizes");

    if (run_hash) {
        run_hash_comparison();
    }

    if (run_sizes) {
        run_size_comparison();
    }

    std::cout << "Benchmark complete.\n";
    return 0;
}
