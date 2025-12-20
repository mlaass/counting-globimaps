/**
 * Hilbert Curve Performance Benchmark
 *
 * Compares LUT-based vs reference implementations across different scenarios.
 * Measures throughput, latency, and cache behavior.
 */

#include <iostream>
#include <iomanip>
#include <chrono>
#include <vector>
#include <random>
#include <algorithm>
#include <cmath>
#include "../../include/space_filling_curves.hpp"

using namespace sfc;

// Benchmark configuration
constexpr int WARMUP_ITERATIONS = 10000;
constexpr int BENCHMARK_ITERATIONS = 10000000;

// Prevent compiler optimization
template<typename T>
void do_not_optimize(T const& value) {
    asm volatile("" : : "r,m"(value) : "memory");
}

struct BenchResult {
    double ns_per_op;
    double ops_per_sec;
    double cycles_per_op;  // Estimated
};

template<typename Func>
BenchResult benchmark(const char* name, Func f, int iterations) {
    // Warmup
    for (int i = 0; i < WARMUP_ITERATIONS; ++i) {
        do_not_optimize(f(i));
    }

    auto start = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < iterations; ++i) {
        do_not_optimize(f(i));
    }
    auto end = std::chrono::high_resolution_clock::now();

    double total_ns = std::chrono::duration<double, std::nano>(end - start).count();
    double ns_per_op = total_ns / iterations;
    double ops_per_sec = 1e9 / ns_per_op;

    // Estimate cycles (assuming ~3 GHz CPU)
    double cycles_per_op = ns_per_op * 3.0;

    return {ns_per_op, ops_per_sec, cycles_per_op};
}

void print_result(const char* name, const BenchResult& r) {
    std::cout << std::left << std::setw(30) << name
              << std::right << std::setw(10) << std::fixed << std::setprecision(2) << r.ns_per_op << " ns  "
              << std::setw(12) << std::setprecision(0) << r.ops_per_sec << " ops/s  "
              << std::setw(8) << std::setprecision(1) << r.cycles_per_op << " cycles"
              << std::endl;
}

int main() {
    std::cout << "\n=== Hilbert Curve Performance Benchmark ===" << std::endl;
    std::cout << "\nBenchmark iterations: " << BENCHMARK_ITERATIONS << std::endl;

    // Generate random test data
    std::vector<uint32_t> xs(BENCHMARK_ITERATIONS);
    std::vector<uint32_t> ys(BENCHMARK_ITERATIONS);

    std::mt19937 gen(42);
    std::uniform_int_distribution<uint32_t> dist16(0, 65535);
    std::uniform_int_distribution<uint32_t> dist8(0, 255);

    for (int i = 0; i < BENCHMARK_ITERATIONS; ++i) {
        xs[i] = dist16(gen);
        ys[i] = dist16(gen);
    }

    // ========================================================================
    // 1. Basic Encode Performance
    // ========================================================================
    std::cout << "\n--- 16-bit Coordinate Encoding ---" << std::endl;
    std::cout << std::left << std::setw(30) << "Implementation"
              << std::right << std::setw(10) << "Latency"
              << std::setw(16) << "Throughput"
              << std::setw(12) << "Est. Cycles" << std::endl;
    std::cout << std::string(70, '-') << std::endl;

    auto r_lut = benchmark("Hilbert2D LUT", [&](int i) {
        return Hilbert2D<16>::encode(xs[i], ys[i]);
    }, BENCHMARK_ITERATIONS);
    print_result("Hilbert2D (LUT)", r_lut);

    auto r_ref = benchmark("Hilbert2D Reference", [&](int i) {
        return Hilbert2D<16>::encode_reference(xs[i], ys[i]);
    }, BENCHMARK_ITERATIONS);
    print_result("Hilbert2D (Reference)", r_ref);

    auto r_morton = benchmark("Morton2D", [&](int i) {
        return Morton2D<16>::encode(xs[i], ys[i]);
    }, BENCHMARK_ITERATIONS);
    print_result("Morton2D (BMI2/portable)", r_morton);

    std::cout << "\nSpeedup: LUT vs Reference = " << std::fixed << std::setprecision(1)
              << (r_ref.ns_per_op / r_lut.ns_per_op) << "x" << std::endl;
    std::cout << "Overhead: Hilbert LUT vs Morton = " << std::fixed << std::setprecision(1)
              << (r_lut.ns_per_op / r_morton.ns_per_op) << "x" << std::endl;

    // ========================================================================
    // 2. Different Bit Widths
    // ========================================================================
    std::cout << "\n--- Different Bit Widths ---" << std::endl;

    auto r8_lut = benchmark("Hilbert2D<8> LUT", [&](int i) {
        return Hilbert2D<8>::encode(xs[i] & 0xFF, ys[i] & 0xFF);
    }, BENCHMARK_ITERATIONS);
    print_result("Hilbert2D<8> (LUT)", r8_lut);

    auto r8_ref = benchmark("Hilbert2D<8> Ref", [&](int i) {
        return Hilbert2D<8>::encode_reference(xs[i] & 0xFF, ys[i] & 0xFF);
    }, BENCHMARK_ITERATIONS);
    print_result("Hilbert2D<8> (Reference)", r8_ref);

    auto r12_lut = benchmark("Hilbert2D<12> LUT", [&](int i) {
        return Hilbert2D<12>::encode(xs[i] & 0xFFF, ys[i] & 0xFFF);
    }, BENCHMARK_ITERATIONS);
    print_result("Hilbert2D<12> (LUT)", r12_lut);

    auto r12_ref = benchmark("Hilbert2D<12> Ref", [&](int i) {
        return Hilbert2D<12>::encode_reference(xs[i] & 0xFFF, ys[i] & 0xFFF);
    }, BENCHMARK_ITERATIONS);
    print_result("Hilbert2D<12> (Reference)", r12_ref);

    // ========================================================================
    // 3. Sequential vs Random Access
    // ========================================================================
    std::cout << "\n--- Access Pattern Impact ---" << std::endl;

    // Sequential access (linear scan)
    auto r_seq = benchmark("Sequential", [](int i) {
        uint32_t x = i & 0xFFFF;
        uint32_t y = (i >> 16) & 0xFFFF;
        return Hilbert2D<16>::encode(x, y);
    }, BENCHMARK_ITERATIONS);
    print_result("Sequential access", r_seq);

    // Random access (already benchmarked as r_lut)
    print_result("Random access", r_lut);

    // ========================================================================
    // 4. Batch Throughput
    // ========================================================================
    std::cout << "\n--- Batch Processing ---" << std::endl;

    // Process in batches to measure sustained throughput
    constexpr int BATCH_SIZE = 10000;
    constexpr int NUM_BATCHES = BENCHMARK_ITERATIONS / BATCH_SIZE;

    auto batch_start = std::chrono::high_resolution_clock::now();
    uint64_t sum = 0;
    for (int b = 0; b < NUM_BATCHES; ++b) {
        for (int i = 0; i < BATCH_SIZE; ++i) {
            int idx = b * BATCH_SIZE + i;
            sum += Hilbert2D<16>::encode(xs[idx], ys[idx]);
        }
    }
    auto batch_end = std::chrono::high_resolution_clock::now();
    do_not_optimize(sum);

    double batch_ns = std::chrono::duration<double, std::nano>(batch_end - batch_start).count();
    double batch_ns_per_op = batch_ns / BENCHMARK_ITERATIONS;
    double batch_throughput = BENCHMARK_ITERATIONS / (batch_ns / 1e9);

    std::cout << "Batch throughput: " << std::fixed << std::setprecision(2)
              << batch_throughput / 1e6 << " M encodes/sec" << std::endl;
    std::cout << "Batch latency:    " << batch_ns_per_op << " ns/op" << std::endl;

    // ========================================================================
    // 5. Encode + Decode Roundtrip
    // ========================================================================
    std::cout << "\n--- Encode + Decode Roundtrip ---" << std::endl;

    auto r_rt = benchmark("Roundtrip", [&](int i) {
        uint64_t code = Hilbert2D<16>::encode(xs[i], ys[i]);
        uint32_t dx, dy;
        Hilbert2D<16>::decode(code, dx, dy);
        return dx + dy;
    }, BENCHMARK_ITERATIONS);
    print_result("Hilbert2D encode+decode", r_rt);

    // ========================================================================
    // Summary
    // ========================================================================
    std::cout << "\n=== Summary ===" << std::endl;
    std::cout << "LUT-based Hilbert2D<16>:    " << std::fixed << std::setprecision(2)
              << r_lut.ns_per_op << " ns/op" << std::endl;
    std::cout << "Reference Hilbert2D<16>:    " << r_ref.ns_per_op << " ns/op" << std::endl;
    std::cout << "Morton2D<16> (baseline):    " << r_morton.ns_per_op << " ns/op" << std::endl;
    std::cout << "\nLUT speedup over reference: " << (r_ref.ns_per_op / r_lut.ns_per_op) << "x" << std::endl;

    return 0;
}
