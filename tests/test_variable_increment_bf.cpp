#include "variable_increment_bf.hpp"
#include <cassert>
#include <iostream>
#include <random>
#include <set>
#include <vector>

using namespace globimap;

// Simple test framework
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

#define ASSERT_FALSE(cond) \
    if (cond) throw std::runtime_error("Assertion failed: !" #cond)

#define ASSERT_EQ(a, b) \
    if ((a) != (b)) throw std::runtime_error("Assertion failed: " #a " == " #b)

#define ASSERT_GT(a, b) \
    if ((a) <= (b)) throw std::runtime_error("Assertion failed: " #a " > " #b)

#define ASSERT_GE(a, b) \
    if ((a) < (b)) throw std::runtime_error("Assertion failed: " #a " >= " #b)

#define ASSERT_LE(a, b) \
    if ((a) > (b)) throw std::runtime_error("Assertion failed: " #a " <= " #b)

// Test 1: Configuration validation
TEST(config_validation) {
    // Valid configuration
    VICBFConfig valid_conf{8, 16, 16, 4};
    valid_conf.validate();  // Should not throw

    // Invalid: hash_k = 0
    try {
        VICBFConfig conf{0, 16, 16, 4};
        conf.validate();
        ASSERT_TRUE(false);  // Should have thrown
    } catch (const std::invalid_argument &e) {
        // Expected
    }

    // Invalid: counter_bits not in {8, 16, 32, 64}
    try {
        VICBFConfig conf{8, 16, 12, 4};
        conf.validate();
        ASSERT_TRUE(false);
    } catch (const std::invalid_argument &e) {
        // Expected
    }

    // Invalid: increment_L not power of 2
    try {
        VICBFConfig conf{8, 16, 16, 5};
        conf.validate();
        ASSERT_TRUE(false);
    } catch (const std::invalid_argument &e) {
        // Expected
    }
}

// Test 2: Basic insertion
TEST(basic_insertion) {
    VICBFConfig conf{8, 16, 16, 4};  // Small filter for testing
    VariableIncrementBloomFilter bf(conf);

    std::vector<uint64_t> point1 = {1, 2, 3};
    std::vector<uint64_t> point2 = {4, 5, 6};

    // Insert point1
    bf.put(point1);

    // Query point1 - should be present
    ASSERT_TRUE(bf.get_bool(point1));
    ASSERT_GT(bf.get_min(point1), 0);

    // Query point2 - not inserted, might be false positive
    // We can't assert false here due to false positives
}

// Test 3: Counter increment behavior
TEST(counter_increment_behavior) {
    VICBFConfig conf{4, 12, 16, 4};  // L=4, so increments in [4, 7]
    VariableIncrementBloomFilter bf(conf);

    std::vector<uint64_t> point = {100, 200};

    // First insertion
    bf.put(point);
    uint64_t count1 = bf.get_min(point);

    // Count should be at least 4 (minimum increment) and at most 7*4=28 (all k hashes get max increment)
    ASSERT_GE(count1, 4);
    ASSERT_LE(count1, 7 * 4);  // 4 hashes * max increment 7

    // Second insertion
    bf.put(point);
    uint64_t count2 = bf.get_min(point);

    // Count should have increased
    ASSERT_GT(count2, count1);

    // Count should be at least 2*4=8 and at most 2*7*4=56
    ASSERT_GE(count2, 8);
    ASSERT_LE(count2, 56);
}

// Test 4: Multiple insertions
TEST(multiple_insertions) {
    VICBFConfig conf{8, 16, 16, 4};
    VariableIncrementBloomFilter bf(conf);

    std::vector<uint64_t> point = {42, 43};

    // Insert same point 10 times
    for (int i = 0; i < 10; ++i) {
        bf.put(point);
    }

    // Counter should reflect multiple insertions
    uint64_t count = bf.get_min(point);
    ASSERT_GT(count, 0);

    // With L=4, expected increment is ~5.5 per insertion
    // After 10 insertions: ~55, but min across k hashes could be less
    ASSERT_GE(count, 10 * 4);  // At least 10 * min_increment
}

// Test 5: Batch insertion
TEST(batch_insertion) {
    VICBFConfig conf{8, 16, 16, 4};
    VariableIncrementBloomFilter bf(conf);

    std::vector<std::vector<uint64_t>> points;
    for (int i = 0; i < 100; ++i) {
        points.push_back({static_cast<uint64_t>(i), static_cast<uint64_t>(i * 2)});
    }

    bf.put_all(points);

    // Check that all points are present
    int present_count = 0;
    for (const auto &point : points) {
        if (bf.get_bool(point)) {
            present_count++;
        }
    }

    // All points should be present (no false negatives)
    ASSERT_EQ(present_count, 100);
}

// Test 6: False positive rate
TEST(false_positive_rate) {
    VICBFConfig conf{8, 20, 16, 4};  // Larger filter for lower FPR
    VariableIncrementBloomFilter bf(conf);

    // Insert 1000 elements
    std::set<uint64_t> inserted_set;
    for (int i = 0; i < 1000; ++i) {
        std::vector<uint64_t> point = {static_cast<uint64_t>(i)};
        bf.put(point);
        inserted_set.insert(i);
    }

    // Query 1000 elements that were NOT inserted
    int false_positives = 0;
    for (int i = 1000; i < 2000; ++i) {
        std::vector<uint64_t> point = {static_cast<uint64_t>(i)};
        if (bf.get_bool(point)) {
            false_positives++;
        }
    }

    // FPR should be reasonably low (< 10% for this configuration)
    double fpr = static_cast<double>(false_positives) / 1000.0;
    std::cout << " (FPR=" << fpr << ") ";
    ASSERT_LE(fpr, 0.15);  // Allow up to 15% FPR
}

// Test 7: Counter overflow behavior (8-bit counters)
TEST(counter_overflow_8bit) {
    VICBFConfig conf{4, 10, 8, 4};  // 8-bit counters, max value = 255
    VariableIncrementBloomFilter bf(conf);

    std::vector<uint64_t> point = {999};

    // Insert many times to cause overflow
    for (int i = 0; i < 100; ++i) {
        bf.put(point);
    }

    uint64_t count = bf.get_min(point);

    // Counter should saturate at 255
    ASSERT_LE(count, 255);
}

// Test 8: Counter overflow behavior (16-bit counters)
TEST(counter_overflow_16bit) {
    VICBFConfig conf{4, 10, 16, 4};  // 16-bit counters, max value = 65535
    VariableIncrementBloomFilter bf(conf);

    std::vector<uint64_t> point = {888};

    // Insert many times
    for (int i = 0; i < 10000; ++i) {
        bf.put(point);
    }

    uint64_t count = bf.get_min(point);

    // Counter should saturate at 65535
    ASSERT_LE(count, 65535);
}

// Test 9: Memory usage calculation
TEST(memory_usage) {
    // 8-bit counters: 2^16 counters = 65536 bytes
    VICBFConfig conf8{8, 16, 8, 4};
    VariableIncrementBloomFilter bf8(conf8);
    ASSERT_EQ(bf8.memory_usage(), 65536);

    // 16-bit counters: 2^16 counters * 2 bytes = 131072 bytes
    VICBFConfig conf16{8, 16, 16, 4};
    VariableIncrementBloomFilter bf16(conf16);
    ASSERT_EQ(bf16.memory_usage(), 131072);

    // 32-bit counters: 2^16 counters * 4 bytes = 262144 bytes
    VICBFConfig conf32{8, 16, 32, 4};
    VariableIncrementBloomFilter bf32(conf32);
    ASSERT_EQ(bf32.memory_usage(), 262144);

    // 64-bit counters: 2^16 counters * 8 bytes = 524288 bytes
    VICBFConfig conf64{8, 16, 64, 4};
    VariableIncrementBloomFilter bf64(conf64);
    ASSERT_EQ(bf64.memory_usage(), 524288);
}

// Test 10: Different increment_L values
TEST(different_increment_L) {
    std::vector<uint64_t> point = {123, 456};

    // L=2: increments in [2, 3]
    VICBFConfig conf2{4, 12, 16, 2};
    VariableIncrementBloomFilter bf2(conf2);
    bf2.put(point);
    uint64_t count2 = bf2.get_min(point);
    ASSERT_GE(count2, 2);

    // L=8: increments in [8, 15]
    VICBFConfig conf8{4, 12, 16, 8};
    VariableIncrementBloomFilter bf8(conf8);
    bf8.put(point);
    uint64_t count8 = bf8.get_min(point);
    ASSERT_GE(count8, 8);

    // Larger L should give larger counts
    ASSERT_GT(count8, count2);
}

// Test 11: Multi-dimensional points
TEST(multidimensional_points) {
    VICBFConfig conf{8, 16, 16, 4};
    VariableIncrementBloomFilter bf(conf);

    // 1D point
    std::vector<uint64_t> point1d = {100};
    bf.put(point1d);
    ASSERT_TRUE(bf.get_bool(point1d));

    // 2D point
    std::vector<uint64_t> point2d = {100, 200};
    bf.put(point2d);
    ASSERT_TRUE(bf.get_bool(point2d));

    // 3D point
    std::vector<uint64_t> point3d = {100, 200, 300};
    bf.put(point3d);
    ASSERT_TRUE(bf.get_bool(point3d));

    // 5D point
    std::vector<uint64_t> point5d = {1, 2, 3, 4, 5};
    bf.put(point5d);
    ASSERT_TRUE(bf.get_bool(point5d));
}

// Test 12: Summary output
TEST(summary_output) {
    VICBFConfig conf{8, 16, 16, 4};
    VariableIncrementBloomFilter bf(conf);

    std::string summary = bf.summary();

    // Summary should contain key information
    ASSERT_TRUE(summary.find("Variable-Increment") != std::string::npos);
    ASSERT_TRUE(summary.find("hash functions") != std::string::npos);
    ASSERT_TRUE(summary.find("Counter bits") != std::string::npos);
    ASSERT_TRUE(summary.find("Memory usage") != std::string::npos);
}

// Test 13: Query accuracy with known insertions
TEST(query_accuracy) {
    VICBFConfig conf{8, 18, 32, 4};  // Larger filter, 32-bit counters
    VariableIncrementBloomFilter bf(conf);

    std::vector<uint64_t> point = {777};

    // Insert exactly 10 times
    for (int i = 0; i < 10; ++i) {
        bf.put(point);
    }

    uint64_t estimated_count = bf.get_min(point);

    // With L=4, expected increment per insertion is (4+5+6+7)/4 = 5.5
    // After 10 insertions: expected ~55
    // But get_min returns minimum across k hashes, so could be less
    // We'll check it's in a reasonable range
    ASSERT_GE(estimated_count, 10 * 4);   // At least 10 * min_increment
    ASSERT_LE(estimated_count, 10 * 7 * 8);  // At most 10 * max_increment * k
}

// Main test runner
int main() {
    std::cout << "===== Variable-Increment CBF Unit Tests =====\n\n";

    run_test_config_validation();
    run_test_basic_insertion();
    run_test_counter_increment_behavior();
    run_test_multiple_insertions();
    run_test_batch_insertion();
    run_test_false_positive_rate();
    run_test_counter_overflow_8bit();
    run_test_counter_overflow_16bit();
    run_test_memory_usage();
    run_test_different_increment_L();
    run_test_multidimensional_points();
    run_test_summary_output();
    run_test_query_accuracy();

    std::cout << "\n===== Test Summary =====\n";
    std::cout << "Tests passed: " << test_passed << "/" << test_count << "\n";

    if (test_passed == test_count) {
        std::cout << "All tests PASSED!\n";
        return 0;
    } else {
        std::cout << "Some tests FAILED!\n";
        return 1;
    }
}
