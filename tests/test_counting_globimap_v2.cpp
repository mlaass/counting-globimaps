#include "counting_globimap_v2.hpp"
#include <cassert>
#include <iostream>
#include <random>
#include <vector>
#include <chrono>

using namespace globimap;

// Simple test framework (matching existing tests)
int test_count = 0;
int test_passed = 0;

#define TEST(name) \
    void test_##name(); \
    void run_test_##name() { \
        test_count++; \
        std::cout << "Running test: " << #name << "... "; \
        try { \
            test_##name(); \
            test_passed++; \
            std::cout << "PASSED\n"; \
        } catch (const std::exception &e) { \
            std::cout << "FAILED: " << e.what() << "\n"; \
        } catch (...) { \
            std::cout << "FAILED: Unknown exception\n"; \
        } \
    } \
    void test_##name()

#define ASSERT_TRUE(cond) \
    if (!(cond)) throw std::runtime_error("Assertion failed: " #cond)

#define ASSERT_EQ(a, b) \
    if ((a) != (b)) throw std::runtime_error("Assertion failed: " #a " != " #b " (" + std::to_string(a) + " vs " + std::to_string(b) + ")")

// ============================================================================
// TypedLayer Tests
// ============================================================================

TEST(layer_basic_increment) {
    Layer12 layer;
    layer.resize(10);  // 2^10 = 1024 counters

    ASSERT_EQ(layer.size(), 1024);
    ASSERT_EQ(layer.get_value(0), 0);
    ASSERT_TRUE(!layer.has_overflow(0));

    bool overflow = layer.increment(0);
    ASSERT_TRUE(!overflow);
    ASSERT_EQ(layer.get_value(0), 1);
    ASSERT_TRUE(!layer.has_overflow(0));

    for (int i = 0; i < 99; ++i) {
        layer.increment(0);
    }
    ASSERT_EQ(layer.get_value(0), 100);
}

TEST(layer8_overflow_at_127) {
    Layer8 layer;
    layer.resize(8);

    // Increment to max (127)
    for (int i = 0; i < 127; ++i) {
        bool overflow = layer.increment(0);
        ASSERT_TRUE(!overflow);
    }
    ASSERT_EQ(layer.get_value(0), 127);
    ASSERT_TRUE(!layer.has_overflow(0));

    // Next increment overflows
    bool overflow = layer.increment(0);
    ASSERT_TRUE(overflow);
    ASSERT_EQ(layer.get_value(0), 0);  // Wrapped
    ASSERT_TRUE(layer.has_overflow(0));

    // Continue incrementing
    overflow = layer.increment(0);
    ASSERT_TRUE(!overflow);
    ASSERT_EQ(layer.get_value(0), 1);
    ASSERT_TRUE(layer.has_overflow(0));  // Still set
}

TEST(layer12_overflow_at_2047) {
    Layer12 layer;
    layer.resize(4);

    for (int i = 0; i < 2047; ++i) {
        ASSERT_TRUE(!layer.increment(0));
    }
    ASSERT_EQ(layer.get_value(0), 2047);
    ASSERT_TRUE(!layer.has_overflow(0));

    ASSERT_TRUE(layer.increment(0));  // Overflow
    ASSERT_EQ(layer.get_value(0), 0);
    ASSERT_TRUE(layer.has_overflow(0));
}

TEST(layer_memory_size) {
    Layer12 layer;
    layer.resize(20);  // 2^20 = 1M counters

    size_t expected = (1 << 20) * sizeof(uint16_t);
    ASSERT_EQ(layer.byte_size(), expected);
    ASSERT_EQ(layer.byte_size(), 2 * 1024 * 1024);  // 2 MB
}

// ============================================================================
// CountingGloBiMapV2 Tests
// ============================================================================

TEST(cgm_single_layer_basic) {
    CGM_12 filter;
    filter.configure(8, {20});

    std::vector<uint64_t> point = {100, 200};

    ASSERT_EQ(filter.get_min(point), 0);
    ASSERT_TRUE(!filter.get_bool(point));

    filter.put(point);
    ASSERT_EQ(filter.get_min(point), 1);
    ASSERT_TRUE(filter.get_bool(point));

    for (int i = 0; i < 99; ++i) {
        filter.put(point);
    }
    ASSERT_EQ(filter.get_min(point), 100);
}

TEST(cgm_two_layer_cascade) {
    CGM_12_20 filter;
    filter.configure(4, {10, 8});

    std::vector<uint64_t> point = {42, 84};

    // Insert beyond Layer12 max (2047)
    for (int i = 0; i < 3000; ++i) {
        filter.put(point);
    }

    uint64_t count = filter.get_min(point);
    ASSERT_EQ(count, 3000);
}

TEST(cgm_reconstruction_various_counts) {
    CGM_12_20 filter;
    filter.configure(8, {12, 10});

    std::vector<uint64_t> point = {123, 456};
    std::vector<int> test_counts = {1, 100, 2047, 2048, 5000, 10000};

    for (int target : test_counts) {
        filter.clear();
        for (int i = 0; i < target; ++i) {
            filter.put(point);
        }
        uint64_t result = filter.get_min(point);
        ASSERT_EQ(result, (uint64_t)target);
    }
}

TEST(cgm_minimal_increment_mode) {
    CGM_12_20 filter;
    filter.configure(8, {16, 12}, true);

    std::vector<uint64_t> point = {999, 888};

    for (int i = 0; i < 100; ++i) {
        filter.put(point);
    }

    ASSERT_EQ(filter.get_min(point), 100);
    ASSERT_TRUE(filter.is_minimal_increment());
}

TEST(cgm_multiple_points) {
    CGM_12_20 filter;
    filter.configure(8, {18, 14});

    std::vector<std::vector<uint64_t>> points = {
        {100, 200},
        {100, 201},
        {101, 200},
        {999, 888}
    };

    for (int i = 0; i < 50; ++i) filter.put(points[0]);
    for (int i = 0; i < 100; ++i) filter.put(points[1]);
    for (int i = 0; i < 200; ++i) filter.put(points[2]);
    for (int i = 0; i < 500; ++i) filter.put(points[3]);

    ASSERT_EQ(filter.get_min(points[0]), 50);
    ASSERT_EQ(filter.get_min(points[1]), 100);
    ASSERT_EQ(filter.get_min(points[2]), 200);
    ASSERT_EQ(filter.get_min(points[3]), 500);
}

TEST(cgm_memory_usage) {
    CGM_12_20 filter;
    filter.configure(8, {20, 14});

    // Layer12: 2^20 * 2 bytes = 2 MB
    // Layer20: 2^14 * 4 bytes = 64 KB
    size_t expected = (1 << 20) * 2 + (1 << 14) * 4;
    ASSERT_EQ(filter.memory_usage(), expected);
}

TEST(cgm_asymmetric_layers) {
    CGM_12_20 filter;
    filter.configure(8, {20, 10});

    std::vector<uint64_t> point = {12345, 67890};

    for (int i = 0; i < 5000; ++i) {
        filter.put(point);
    }

    ASSERT_EQ(filter.get_min(point), 5000);
}

TEST(cgm_three_layers) {
    CGM_12_20_32 filter;
    filter.configure(8, {16, 12, 8});

    std::vector<uint64_t> point = {1, 2};

    int large_count = 600000;
    for (int i = 0; i < large_count; ++i) {
        filter.put(point);
    }

    ASSERT_EQ(filter.get_min(point), (uint64_t)large_count);
}

TEST(cgm_multi_category) {
    CGM_12_20 filter;
    filter.configure(8, {18, 14});

    std::vector<uint64_t> cat1 = {100, 200, 1};
    std::vector<uint64_t> cat2 = {100, 200, 2};
    std::vector<uint64_t> cat3 = {100, 200, 3};

    for (int i = 0; i < 50; ++i) filter.put(cat1);
    for (int i = 0; i < 100; ++i) filter.put(cat2);
    for (int i = 0; i < 200; ++i) filter.put(cat3);

    ASSERT_EQ(filter.get_min(cat1), 50);
    ASSERT_EQ(filter.get_min(cat2), 100);
    ASSERT_EQ(filter.get_min(cat3), 200);
}

TEST(cgm_clear) {
    CGM_12 filter;
    filter.configure(8, {16});

    std::vector<uint64_t> point = {42, 24};
    for (int i = 0; i < 100; ++i) filter.put(point);
    ASSERT_EQ(filter.get_min(point), 100);

    filter.clear();
    ASSERT_EQ(filter.get_min(point), 0);
}

TEST(cgm_layer_access) {
    CGM_12_20 filter;
    filter.configure(8, {16, 12});

    // Access layers for debugging
    const auto& layer0 = filter.get_layer<0>();
    const auto& layer1 = filter.get_layer<1>();

    ASSERT_EQ(layer0.size(), 1 << 16);
    ASSERT_EQ(layer1.size(), 1 << 12);
    ASSERT_EQ(layer0.value_bits, 11);
    ASSERT_EQ(layer1.value_bits, 19);
}

// ============================================================================
// Performance benchmark (informational)
// ============================================================================

void benchmark_insert() {
    std::cout << "\n=== Performance Benchmark ===\n";

    CGM_12_20 filter;
    filter.configure(8, {20, 14});

    std::mt19937_64 rng(42);
    std::uniform_int_distribution<uint64_t> dist(0, 1000000);

    const int N = 1000000;
    std::vector<std::vector<uint64_t>> points(N);
    for (int i = 0; i < N; ++i) {
        points[i] = {dist(rng), dist(rng)};
    }

    auto start = std::chrono::high_resolution_clock::now();
    for (const auto& p : points) {
        filter.put(p);
    }
    auto end = std::chrono::high_resolution_clock::now();

    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
    double throughput = N / (ms / 1000.0);

    std::cout << "Insert: " << N << " ops in " << ms << " ms";
    std::cout << " (" << (throughput / 1e6) << " M ops/sec)\n";

    // Query benchmark
    start = std::chrono::high_resolution_clock::now();
    uint64_t sum = 0;
    for (const auto& p : points) {
        sum += filter.get_min(p);
    }
    end = std::chrono::high_resolution_clock::now();

    auto us = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();
    double latency = us / (double)N;

    std::cout << "Query: " << N << " ops in " << (us / 1000) << " ms";
    std::cout << " (" << latency << " us/query, sum=" << sum << ")\n";

    std::cout << "Memory: " << (filter.memory_usage() / (1024.0 * 1024.0)) << " MB\n";
}

// ============================================================================
// Main
// ============================================================================

int main() {
    std::cout << "CountingGloBiMapV2 Unit Tests\n";
    std::cout << "============================\n\n";

    // TypedLayer tests
    run_test_layer_basic_increment();
    run_test_layer8_overflow_at_127();
    run_test_layer12_overflow_at_2047();
    run_test_layer_memory_size();

    // CountingGloBiMapV2 tests
    run_test_cgm_single_layer_basic();
    run_test_cgm_two_layer_cascade();
    run_test_cgm_reconstruction_various_counts();
    run_test_cgm_minimal_increment_mode();
    run_test_cgm_multiple_points();
    run_test_cgm_memory_usage();
    run_test_cgm_asymmetric_layers();
    run_test_cgm_three_layers();
    run_test_cgm_multi_category();
    run_test_cgm_clear();
    run_test_cgm_layer_access();

    std::cout << "\n============================\n";
    std::cout << "Tests passed: " << test_passed << "/" << test_count << "\n";

    // Run benchmark
    benchmark_insert();

    return (test_passed == test_count) ? 0 : 1;
}
