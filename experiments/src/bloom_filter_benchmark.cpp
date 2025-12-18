/*
 * Comprehensive Bloom Filter Benchmark
 *
 * Compares cache-optimal bloom filter implementations:
 * - BasicBloomFilter (baseline)
 * - BlockedBloomFilter (cache-line aligned)
 * - RegisterBlockedBloomFilter (64-bit atomic masks)
 * - SimdBloomFilter (AVX2 vectorized)
 * - PatternedSimdBloomFilter (advanced SIMD)
 * - GloBiMap (our binary filter for comparison)
 *
 * Metrics (via hardware performance counters):
 * - Nanoseconds per operation
 * - CPU cycles per operation
 * - Instructions per operation
 * - Branch misses per operation
 * - False positive rate
 * - Memory usage
 *
 * Requirements:
 * - Linux with perf_event support
 * - May need: sudo sysctl kernel.perf_event_paranoid=1
 * - For best results: sudo cpupower frequency-set -g performance
 *
 * Usage:
 *   ./bloom_filter_benchmark [options]
 *
 * Options:
 *   --epochs N        Number of epochs per benchmark (default: 11)
 *   --min-iters N     Minimum iterations per epoch (default: 1000)
 *   --warmup N        Warmup iterations (default: 100)
 *   --epoch-time MS   Max time per epoch in ms (default: 100)
 *   --scenario S      Run only scenario: small, medium, large, all (default: all)
 *   --help            Show this help
 */

#define ANKERL_NANOBENCH_IMPLEMENT
#include "external/nanobench.h"

#include "blocked_bloom_filter.hpp"
#include "register_blocked_bf.hpp"
#include "simd_bloom_filter.hpp"
#include "globimap.hpp"

#include <chrono>
#include <cmath>
#include <cstring>
#include <iomanip>
#include <iostream>
#include <random>
#include <string>
#include <vector>

using namespace globimap;

// ============================================================================
// Configuration
// ============================================================================

struct BenchmarkConfig {
    size_t epochs = 11;           // Number of epochs (measurements) per benchmark
    size_t min_iters = 1000;      // Minimum iterations per epoch
    size_t warmup = 100;          // Warmup iterations
    size_t epoch_time_ms = 100;   // Max time per epoch in milliseconds
    std::string scenario = "all"; // Which scenario to run

    void print() const {
        std::cout << "Benchmark configuration:\n";
        std::cout << "  Epochs: " << epochs << "\n";
        std::cout << "  Min iterations/epoch: " << min_iters << "\n";
        std::cout << "  Warmup iterations: " << warmup << "\n";
        std::cout << "  Max epoch time: " << epoch_time_ms << " ms\n";
        std::cout << "  Scenario: " << scenario << "\n";
    }
};

void print_usage(const char* prog) {
    std::cout << "Usage: " << prog << " [options]\n\n";
    std::cout << "Options:\n";
    std::cout << "  --epochs N        Number of epochs per benchmark (default: 11)\n";
    std::cout << "  --min-iters N     Minimum iterations per epoch (default: 1000)\n";
    std::cout << "  --warmup N        Warmup iterations (default: 100)\n";
    std::cout << "  --epoch-time MS   Max time per epoch in ms (default: 100)\n";
    std::cout << "  --scenario S      Run only scenario: small, medium, large, all (default: all)\n";
    std::cout << "  --help            Show this help\n";
    std::cout << "\nExamples:\n";
    std::cout << "  " << prog << " --epochs 51 --min-iters 10000    # More accurate, longer run\n";
    std::cout << "  " << prog << " --scenario large                  # Only run large scenario\n";
    std::cout << "  " << prog << " --epoch-time 500                  # 500ms per epoch\n";
}

BenchmarkConfig parse_args(int argc, char* argv[]) {
    BenchmarkConfig config;

    for (int i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0) {
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

// Global config (set from main)
BenchmarkConfig g_config;

// ============================================================================
// Data generation
// ============================================================================

std::vector<std::vector<uint64_t>> generate_dataset(size_t n, uint64_t seed) {
    std::mt19937_64 rng(seed);
    std::vector<std::vector<uint64_t>> data;
    data.reserve(n);
    for (size_t i = 0; i < n; ++i) {
        data.push_back({rng(), rng()});
    }
    return data;
}

// ============================================================================
// Benchmark templates for each filter type
// ============================================================================

template<typename Filter, typename Config>
void benchmark_filter(ankerl::nanobench::Bench &bench,
                      const std::string &name,
                      const Config &conf,
                      const std::vector<std::vector<uint64_t>> &insert_data,
                      const std::vector<std::vector<uint64_t>> &query_data) {
    Filter filter(conf);

    // Insert benchmark
    size_t insert_idx = 0;
    bench.run(name + " insert", [&]() {
        filter.put(insert_data[insert_idx++ % insert_data.size()]);
    });

    // Reset and fill for query benchmark
    filter.clear();
    for (const auto &point : insert_data) {
        filter.put(point);
    }

    // Query benchmark (true positives)
    size_t query_idx = 0;
    bench.run(name + " query", [&]() {
        ankerl::nanobench::doNotOptimizeAway(
            filter.get_bool(insert_data[query_idx++ % insert_data.size()]));
    });

    // FPR measurement (not timed, just for statistics)
    int false_positives = 0;
    for (const auto &point : query_data) {
        if (filter.get_bool(point)) {
            false_positives++;
        }
    }
    double fpr = static_cast<double>(false_positives) / query_data.size();
    uint64_t mem = filter.memory_usage();

    std::cout << "  " << name << " stats: FPR=" << std::fixed << std::setprecision(4)
              << fpr << ", Memory=" << mem << " bytes\n";
}

// Special benchmark for GloBiMap (different interface)
void benchmark_globimap(ankerl::nanobench::Bench &bench,
                        const std::vector<std::vector<uint64_t>> &insert_data,
                        const std::vector<std::vector<uint64_t>> &query_data,
                        uint k, uint logm) {
    GloBiMap<> filter;
    filter.configure(k, logm);

    // Insert benchmark
    size_t insert_idx = 0;
    bench.run("GloBiMap insert", [&]() {
        filter.put(insert_data[insert_idx++ % insert_data.size()]);
    });

    // Reconfigure to reset (don't use clear() - it sets size to 0!)
    filter.configure(k, logm);
    for (const auto &point : insert_data) {
        filter.put(point);
    }

    // Query benchmark
    size_t query_idx = 0;
    bench.run("GloBiMap query", [&]() {
        ankerl::nanobench::doNotOptimizeAway(
            filter.get(insert_data[query_idx++ % insert_data.size()]));
    });

    // FPR measurement
    int false_positives = 0;
    for (const auto &point : query_data) {
        if (filter.get(point)) {
            false_positives++;
        }
    }
    double fpr = static_cast<double>(false_positives) / query_data.size();
    // std::vector<bool> is bit-packed, so memory = size / 8 bytes
    uint64_t mem = (filter.filter.size() + 7) / 8;

    std::cout << "  GloBiMap stats: FPR=" << std::fixed << std::setprecision(4)
              << fpr << ", Memory=" << mem << " bytes\n";
}

#if SIMD_BF_AVX2_AVAILABLE
// SIMD batch benchmark
void benchmark_simd_batch(ankerl::nanobench::Bench &bench,
                          const std::vector<std::vector<uint64_t>> &insert_data,
                          const std::vector<std::vector<uint64_t>> &query_data,
                          uint64_t n, double fpr_target) {
    SimdBFConfig conf;
    conf.expected_items = n;
    conf.false_positive_rate = fpr_target;
    SimdBloomFilter filter(conf);

    // Pre-construct all batches outside timed loop to avoid copy overhead
    size_t num_batches = insert_data.size() / 8;
    std::vector<std::array<std::vector<uint64_t>, 8>> batches(num_batches);
    for (size_t b = 0; b < num_batches; ++b) {
        for (int i = 0; i < 8; ++i) {
            batches[b][i] = insert_data[b * 8 + i];
        }
    }

    // Insert benchmark (batched - 8 at a time)
    // Use .batch(8) to report per-element metrics, not per-batch
    size_t batch_idx = 0;
    bench.batch(8).run("SimdBF batch insert", [&]() {
        filter.put_batch(batches[batch_idx++ % num_batches]);
    });

    // Reset and fill
    filter.clear();
    for (size_t b = 0; b < num_batches; ++b) {
        filter.put_batch(batches[b]);
    }

    // Query benchmark (batched)
    // Use .batch(8) to report per-element metrics
    batch_idx = 0;
    bench.batch(8).run("SimdBF batch query", [&]() {
        ankerl::nanobench::doNotOptimizeAway(filter.get_bool_batch(batches[batch_idx++ % num_batches]));
    });

    // Reset batch size to 1 for subsequent benchmarks
    bench.batch(1);

    // FPR measurement
    int false_positives = 0;
    for (const auto &point : query_data) {
        if (filter.get_bool(point)) {
            false_positives++;
        }
    }
    double fpr = static_cast<double>(false_positives) / query_data.size();
    uint64_t mem = filter.memory_usage();

    std::cout << "  SimdBF batch stats: FPR=" << std::fixed << std::setprecision(4)
              << fpr << ", Memory=" << mem << " bytes\n";
}
#endif

void run_benchmark_scenario(const std::string &scenario_name,
                            uint64_t n, double fpr_target) {
    std::cout << "\n========================================\n";
    std::cout << scenario_name << "\n";
    std::cout << "Items: " << n << ", Target FPR: " << fpr_target << "\n";
    std::cout << "========================================\n\n";

    // Generate data
    auto insert_data = generate_dataset(n, 42);
    auto query_data = generate_dataset(n, 12345);  // Different seed = likely not inserted

    // Configure benchmark using global config
    ankerl::nanobench::Bench bench;
    bench.performanceCounters(true)
         .epochs(g_config.epochs)
         .minEpochIterations(g_config.min_iters)
         .warmup(g_config.warmup)
         .maxEpochTime(std::chrono::milliseconds(g_config.epoch_time_ms))
         .relative(true);  // Show relative performance to first benchmark

    // Configure filters
    BlockedBFConfig blocked_conf;
    blocked_conf.expected_items = n;
    blocked_conf.false_positive_rate = fpr_target;

    RegisterBlockedBFConfig rb_conf;
    rb_conf.expected_items = n;
    rb_conf.false_positive_rate = fpr_target;
    rb_conf.compensation = 0;

    // Run benchmarks
    benchmark_filter<BasicBloomFilterAdapter>(bench, "BasicBF", blocked_conf, insert_data, query_data);
    benchmark_filter<BlockedBloomFilter>(bench, "BlockedBF", blocked_conf, insert_data, query_data);
    benchmark_filter<RegisterBlockedBF>(bench, "RegisterBlockedBF", rb_conf, insert_data, query_data);

#if SIMD_BF_AVX2_AVAILABLE
    SimdBFConfig simd_conf;
    simd_conf.expected_items = n;
    simd_conf.false_positive_rate = fpr_target;

    benchmark_filter<SimdBloomFilter>(bench, "SimdBF", simd_conf, insert_data, query_data);
    benchmark_filter<PatternedSimdBloomFilter>(bench, "PatternedSimdBF", simd_conf, insert_data, query_data);
    benchmark_simd_batch(bench, insert_data, query_data, n, fpr_target);
#endif

    // GloBiMap
    uint k = static_cast<uint>(-std::log2(fpr_target) + 0.5);
    uint logm = static_cast<uint>(std::log2(-1.44 * n * std::log2(fpr_target)) + 0.5);
    benchmark_globimap(bench, insert_data, query_data, k, logm);
}

int main(int argc, char* argv[]) {
    g_config = parse_args(argc, argv);

    std::cout << "===== Bloom Filter Benchmark with Hardware Performance Counters =====\n";
    std::cout << "Comparing cache-optimal implementations\n";
#if SIMD_BF_AVX2_AVAILABLE
    std::cout << "SIMD: AVX2 enabled\n";
#else
    std::cout << "SIMD: Not available (compile with -mavx2)\n";
#endif
    std::cout << "\nNote: For accurate cycle counts, run with:\n";
    std::cout << "  sudo sysctl kernel.perf_event_paranoid=1\n";
    std::cout << "  sudo cpupower frequency-set -g performance\n\n";

    std::cout << "FPR Note: Blocked/Register/SIMD filters confine all k hashes to small blocks\n";
    std::cout << "(256-bit or 64-bit), trading FPR accuracy for cache performance.\n";
    std::cout << "Expect 100-500x higher FPR than target for these filters.\n";
    std::cout << "BasicBF and GloBiMap use full filter space and achieve target FPR.\n\n";

    g_config.print();

    bool run_small = (g_config.scenario == "all" || g_config.scenario == "small");
    bool run_medium = (g_config.scenario == "all" || g_config.scenario == "medium");
    bool run_large = (g_config.scenario == "all" || g_config.scenario == "large");

    // Small scenario (10K items, 1% FPR)
    if (run_small) {
        run_benchmark_scenario("Small (10K items, 1% FPR)", 10000, 0.01);
    }

    // Medium scenario (100K items, 0.1% FPR)
    if (run_medium) {
        run_benchmark_scenario("Medium (100K items, 0.1% FPR)", 100000, 0.001);
    }

    // Large scenario (1M items, 0.01% FPR)
    if (run_large) {
        run_benchmark_scenario("Large (1M items, 0.01% FPR)", 1000000, 0.0001);
    }

    std::cout << "\n===== Benchmark Complete =====\n";
    return 0;
}
