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
 * Metrics:
 * - Insert throughput (ops/sec)
 * - Query throughput (ops/sec)
 * - False positive rate
 * - Memory usage
 */

#include "blocked_bloom_filter.hpp"
#include "register_blocked_bf.hpp"
#include "simd_bloom_filter.hpp"
#include "globimap.hpp"

#include <chrono>
#include <cmath>
#include <iomanip>
#include <iostream>
#include <random>
#include <vector>

using namespace globimap;

// ============================================================================
// Timer utility
// ============================================================================

class Timer {
    std::chrono::high_resolution_clock::time_point start_;
public:
    Timer() : start_(std::chrono::high_resolution_clock::now()) {}

    double elapsed_ms() const {
        auto end = std::chrono::high_resolution_clock::now();
        return std::chrono::duration<double, std::milli>(end - start_).count();
    }

    double elapsed_us() const {
        return elapsed_ms() * 1000.0;
    }
};

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
// Benchmark functions
// ============================================================================

template<typename Filter, typename Config>
void benchmark_filter(const std::string &name, const Config &conf,
                      const std::vector<std::vector<uint64_t>> &insert_data,
                      const std::vector<std::vector<uint64_t>> &query_data) {
    Filter filter(conf);

    // Insert benchmark
    Timer insert_timer;
    for (const auto &point : insert_data) {
        filter.put(point);
    }
    double insert_ms = insert_timer.elapsed_ms();
    double insert_rate = (insert_data.size() / insert_ms) * 1000.0;

    // Query benchmark (true positives)
    Timer query_timer;
    int found = 0;
    for (const auto &point : insert_data) {
        if (filter.get_bool(point)) {
            found++;
        }
    }
    double query_ms = query_timer.elapsed_ms();
    double query_rate = (insert_data.size() / query_ms) * 1000.0;

    // FPR measurement (query non-existent items)
    int false_positives = 0;
    for (const auto &point : query_data) {
        if (filter.get_bool(point)) {
            false_positives++;
        }
    }
    double fpr = static_cast<double>(false_positives) / query_data.size();

    // Memory
    uint64_t mem = filter.memory_usage();

    // Output
    std::cout << std::left << std::setw(25) << name
              << std::right << std::setw(12) << std::fixed << std::setprecision(0) << insert_rate
              << std::right << std::setw(12) << std::fixed << std::setprecision(0) << query_rate
              << std::right << std::setw(10) << std::fixed << std::setprecision(4) << fpr
              << std::right << std::setw(12) << mem
              << "\n";
}

// Special benchmark for GloBiMap (different interface)
void benchmark_globimap(const std::vector<std::vector<uint64_t>> &insert_data,
                        const std::vector<std::vector<uint64_t>> &query_data,
                        uint k, uint logm) {
    GloBiMap<> filter;
    filter.configure(k, logm);

    // Insert benchmark
    Timer insert_timer;
    for (const auto &point : insert_data) {
        filter.put(point);
    }
    double insert_ms = insert_timer.elapsed_ms();
    double insert_rate = (insert_data.size() / insert_ms) * 1000.0;

    // Query benchmark
    Timer query_timer;
    for (const auto &point : insert_data) {
        filter.get(point);
    }
    double query_ms = query_timer.elapsed_ms();
    double query_rate = (insert_data.size() / query_ms) * 1000.0;

    // FPR measurement
    int false_positives = 0;
    for (const auto &point : query_data) {
        if (filter.get(point)) {
            false_positives++;
        }
    }
    double fpr = static_cast<double>(false_positives) / query_data.size();

    // Memory
    uint64_t mem = filter.filter.size();

    std::cout << std::left << std::setw(25) << "GloBiMap"
              << std::right << std::setw(12) << std::fixed << std::setprecision(0) << insert_rate
              << std::right << std::setw(12) << std::fixed << std::setprecision(0) << query_rate
              << std::right << std::setw(10) << std::fixed << std::setprecision(4) << fpr
              << std::right << std::setw(12) << mem
              << "\n";
}

#if SIMD_BF_AVX2_AVAILABLE
// SIMD batch benchmark
void benchmark_simd_batch(const std::vector<std::vector<uint64_t>> &insert_data,
                          const std::vector<std::vector<uint64_t>> &query_data,
                          uint64_t n, double fpr_target) {
    SimdBFConfig conf;
    conf.expected_items = n;
    conf.false_positive_rate = fpr_target;
    SimdBloomFilter filter(conf);

    // Prepare batches
    size_t num_batches = insert_data.size() / 8;

    // Insert benchmark (batched)
    Timer insert_timer;
    for (size_t b = 0; b < num_batches; ++b) {
        std::array<std::vector<uint64_t>, 8> batch;
        for (int i = 0; i < 8; ++i) {
            batch[i] = insert_data[b * 8 + i];
        }
        filter.put_batch(batch);
    }
    double insert_ms = insert_timer.elapsed_ms();
    double insert_rate = (num_batches * 8 / insert_ms) * 1000.0;

    // Query benchmark (batched)
    Timer query_timer;
    int found = 0;
    for (size_t b = 0; b < num_batches; ++b) {
        std::array<std::vector<uint64_t>, 8> batch;
        for (int i = 0; i < 8; ++i) {
            batch[i] = insert_data[b * 8 + i];
        }
        auto results = filter.get_bool_batch(batch);
        for (int i = 0; i < 8; ++i) {
            if (results[i]) found++;
        }
    }
    double query_ms = query_timer.elapsed_ms();
    double query_rate = (num_batches * 8 / query_ms) * 1000.0;

    // FPR measurement
    int false_positives = 0;
    for (const auto &point : query_data) {
        if (filter.get_bool(point)) {
            false_positives++;
        }
    }
    double fpr = static_cast<double>(false_positives) / query_data.size();

    uint64_t mem = filter.memory_usage();

    std::cout << std::left << std::setw(25) << "SimdBF (batch)"
              << std::right << std::setw(12) << std::fixed << std::setprecision(0) << insert_rate
              << std::right << std::setw(12) << std::fixed << std::setprecision(0) << query_rate
              << std::right << std::setw(10) << std::fixed << std::setprecision(4) << fpr
              << std::right << std::setw(12) << mem
              << "\n";
}
#endif

void run_benchmark_scenario(const std::string &scenario_name,
                            uint64_t n, double fpr_target) {
    std::cout << "\n=== " << scenario_name << " ===\n";
    std::cout << "Items: " << n << ", Target FPR: " << fpr_target << "\n\n";

    // Generate data
    auto insert_data = generate_dataset(n, 42);
    auto query_data = generate_dataset(n, 12345);  // Different seed = likely not inserted

    // Header
    std::cout << std::left << std::setw(25) << "Implementation"
              << std::right << std::setw(12) << "Insert/s"
              << std::right << std::setw(12) << "Query/s"
              << std::right << std::setw(10) << "FPR"
              << std::right << std::setw(12) << "Memory (B)"
              << "\n";
    std::cout << std::string(71, '-') << "\n";

    // Configure filters
    BlockedBFConfig blocked_conf;
    blocked_conf.expected_items = n;
    blocked_conf.false_positive_rate = fpr_target;

    RegisterBlockedBFConfig rb_conf;
    rb_conf.expected_items = n;
    rb_conf.false_positive_rate = fpr_target;
    rb_conf.compensation = 0;

    // Run benchmarks
    benchmark_filter<BasicBloomFilterAdapter>("BasicBloomFilter", blocked_conf, insert_data, query_data);
    benchmark_filter<BlockedBloomFilter>("BlockedBloomFilter", blocked_conf, insert_data, query_data);
    benchmark_filter<RegisterBlockedBF>("RegisterBlockedBF", rb_conf, insert_data, query_data);

#if SIMD_BF_AVX2_AVAILABLE
    SimdBFConfig simd_conf;
    simd_conf.expected_items = n;
    simd_conf.false_positive_rate = fpr_target;

    benchmark_filter<SimdBloomFilter>("SimdBloomFilter", simd_conf, insert_data, query_data);
    benchmark_filter<PatternedSimdBloomFilter>("PatternedSimdBF", simd_conf, insert_data, query_data);
    benchmark_simd_batch(insert_data, query_data, n, fpr_target);
#endif

    // GloBiMap
    uint k = static_cast<uint>(-std::log2(fpr_target) + 0.5);
    uint logm = static_cast<uint>(std::log2(-1.44 * n * std::log2(fpr_target)) + 0.5);
    benchmark_globimap(insert_data, query_data, k, logm);
}

int main() {
    std::cout << "===== Bloom Filter Benchmark =====\n";
    std::cout << "Comparing cache-optimal implementations\n";
#if SIMD_BF_AVX2_AVAILABLE
    std::cout << "SIMD: AVX2 enabled\n";
#else
    std::cout << "SIMD: Not available (compile with -mavx2)\n";
#endif

    // Small scenario (10K items, 1% FPR)
    run_benchmark_scenario("Small (10K items, 1% FPR)", 10000, 0.01);

    // Medium scenario (100K items, 0.1% FPR)
    run_benchmark_scenario("Medium (100K items, 0.1% FPR)", 100000, 0.001);

    // Large scenario (1M items, 0.01% FPR)
    run_benchmark_scenario("Large (1M items, 0.01% FPR)", 1000000, 0.0001);

    std::cout << "\n===== Benchmark Complete =====\n";
    return 0;
}
