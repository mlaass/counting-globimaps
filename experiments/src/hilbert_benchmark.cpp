/**
 * SFC (Space-Filling Curve) Performance Benchmark
 *
 * Compares SFC encoding performance:
 * - Morton 2D/3D (PDEP/portable)
 * - Hilbert 2D/3D (LUT-optimized)
 * - Hilbert 2D/3D (Reference bit-by-bit)
 *
 * Key metrics (via hardware performance counters):
 * - Nanoseconds per encode
 * - CPU cycles per encode
 * - Instructions per encode
 * - Throughput (M ops/s)
 * - Instructions per cycle (IPC)
 *
 * Requirements:
 * - Linux with perf_event support
 * - May need: sudo sysctl kernel.perf_event_paranoid=1
 */

#define ANKERL_NANOBENCH_IMPLEMENT
#include "external/nanobench.h"

#include "../../include/space_filling_curves.hpp"

#include <iostream>
#include <iomanip>
#include <vector>
#include <random>

using namespace sfc;

// ============================================================================
// Configuration
// ============================================================================

struct BenchmarkConfig {
    size_t epochs = 11;
    size_t min_iters = 10000;
    size_t warmup = 1000;
    size_t epoch_time_ms = 100;
    size_t data_size = 100000;

    void print() const {
        std::cout << "Benchmark configuration:\n";
        std::cout << "  Epochs: " << epochs << "\n";
        std::cout << "  Min iterations/epoch: " << min_iters << "\n";
        std::cout << "  Warmup iterations: " << warmup << "\n";
        std::cout << "  Max epoch time: " << epoch_time_ms << " ms\n";
        std::cout << "  Data size: " << data_size << "\n";
    }
};

static BenchmarkConfig g_config;

// ============================================================================
// Main
// ============================================================================

int main() {
    std::cout << "===== SFC Encoding Benchmark =====\n\n";
    g_config.print();
    std::cout << "\n";

    // Generate random test data (2D and 3D)
    std::vector<uint32_t> xs(g_config.data_size);
    std::vector<uint32_t> ys(g_config.data_size);
    std::vector<uint32_t> zs(g_config.data_size);

    std::mt19937 gen(42);
    std::uniform_int_distribution<uint32_t> dist16(0, 65535);

    for (size_t i = 0; i < g_config.data_size; ++i) {
        xs[i] = dist16(gen);
        ys[i] = dist16(gen);
        zs[i] = dist16(gen);
    }

    // ========================================================================
    // 2D SFC Encoding
    // ========================================================================
    std::cout << "===== 2D SFC Encoding =====\n";

    ankerl::nanobench::Bench bench2d;
    bench2d.title("SFC 2D Encoding")
           .warmup(g_config.warmup)
           .epochs(g_config.epochs)
           .minEpochIterations(g_config.min_iters)
           .maxEpochTime(std::chrono::milliseconds(g_config.epoch_time_ms));

    size_t idx = 0;

    bench2d.run("Morton2D (PDEP)", [&]() {
        auto result = Morton2D<16>::encode(xs[idx % g_config.data_size], ys[idx % g_config.data_size]);
        ankerl::nanobench::doNotOptimizeAway(result);
        ++idx;
    });

    idx = 0;
    bench2d.run("Hilbert2D (LUT)", [&]() {
        auto result = Hilbert2D<16>::encode(xs[idx % g_config.data_size], ys[idx % g_config.data_size]);
        ankerl::nanobench::doNotOptimizeAway(result);
        ++idx;
    });

    idx = 0;
    bench2d.run("Hilbert2D (Reference)", [&]() {
        auto result = Hilbert2D<16>::encode_reference(xs[idx % g_config.data_size], ys[idx % g_config.data_size]);
        ankerl::nanobench::doNotOptimizeAway(result);
        ++idx;
    });

    // ========================================================================
    // 3D SFC Encoding
    // ========================================================================
    std::cout << "\n===== 3D SFC Encoding =====\n";

    ankerl::nanobench::Bench bench3d;
    bench3d.title("SFC 3D Encoding")
           .warmup(g_config.warmup)
           .epochs(g_config.epochs)
           .minEpochIterations(g_config.min_iters)
           .maxEpochTime(std::chrono::milliseconds(g_config.epoch_time_ms));

    idx = 0;
    bench3d.run("Morton3D (PDEP)", [&]() {
        auto result = Morton3D<16>::encode(xs[idx % g_config.data_size], ys[idx % g_config.data_size], zs[idx % g_config.data_size]);
        ankerl::nanobench::doNotOptimizeAway(result);
        ++idx;
    });

    idx = 0;
    bench3d.run("Hilbert3D (LUT)", [&]() {
        auto result = Hilbert3D<16>::encode(xs[idx % g_config.data_size], ys[idx % g_config.data_size], zs[idx % g_config.data_size]);
        ankerl::nanobench::doNotOptimizeAway(result);
        ++idx;
    });

    idx = 0;
    bench3d.run("Hilbert3D (Reference)", [&]() {
        auto result = Hilbert3D<16>::encode_reference(xs[idx % g_config.data_size], ys[idx % g_config.data_size], zs[idx % g_config.data_size]);
        ankerl::nanobench::doNotOptimizeAway(result);
        ++idx;
    });

    std::cout << "\nBenchmark complete.\n";
    return 0;
}
