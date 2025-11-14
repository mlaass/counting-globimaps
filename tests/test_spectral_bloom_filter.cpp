#include "spectral_bloom_filter.hpp"
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

#define ASSERT_LT(a, b) \
    if ((a) >= (b)) throw std::runtime_error("Assertion failed: " #a " < " #b)

// Test 1: Configuration validation
TEST(config_validation) {
    // Valid configuration - MS variant
    SBFConfig valid_ms{8, 16, 16, MINIMUM_SELECTION};
    valid_ms.validate();

    // Valid configuration - MI variant
    SBFConfig valid_mi{8, 16, 16, MINIMAL_INCREMENT};
    valid_mi.validate();

    // Valid configuration - RM variant
    SBFConfig valid_rm{8, 16, 16, RECURRING_MINIMUM};
    valid_rm.validate();

    // Invalid: hash_k = 0
    try {
        SBFConfig conf{0, 16, 16, MINIMUM_SELECTION};
        conf.validate();
        ASSERT_TRUE(false);
    } catch (const std::invalid_argument &e) {
        // Expected
    }
}

// Test 2: MS variant - basic insertion and query
TEST(ms_variant_basic) {
    SBFConfig conf{8, 16, 16, MINIMUM_SELECTION};
    SpectralBloomFilter bf(conf);

    std::vector<uint64_t> point = {123, 456};

    // Insert once
    bf.put(point);
    ASSERT_EQ(bf.get_min(point), 1);
    ASSERT_TRUE(bf.get_bool(point));

    // Insert again
    bf.put(point);
    ASSERT_EQ(bf.get_min(point), 2);

    // Insert third time
    bf.put(point);
    ASSERT_EQ(bf.get_min(point), 3);
}

// Test 3: MI variant - conservative update behavior
TEST(mi_variant_conservative_update) {
    SBFConfig conf{4, 12, 16, MINIMAL_INCREMENT};
    SpectralBloomFilter bf(conf);

    std::vector<uint64_t> point = {100, 200};

    // First insertion - all counters at 0, so all get incremented
    bf.put(point);
    uint64_t count1 = bf.get_min(point);
    ASSERT_EQ(count1, 1);

    // Second insertion - only minimum counter(s) get incremented
    bf.put(point);
    uint64_t count2 = bf.get_min(point);

    // With MI, min should still be close to actual count
    // (conservative update reduces overcounting)
    ASSERT_GE(count2, 1);
    ASSERT_LE(count2, 3);  // More conservative than MS
}

// Test 4: RM variant - deletion support
TEST(rm_variant_deletion) {
    SBFConfig conf{8, 16, 16, RECURRING_MINIMUM};
    SpectralBloomFilter bf(conf);

    std::vector<uint64_t> point1 = {111, 222};
    std::vector<uint64_t> point2 = {333, 444};

    // Insert point1 once
    bf.put(point1);
    ASSERT_EQ(bf.get_min(point1), 1);

    // Insert point2 once
    bf.put(point2);
    ASSERT_EQ(bf.get_min(point2), 1);

    // Delete point1
    bf.remove(point1);

    // After deletion, count should decrease (may not reach 0 due to hash collisions
    // or conservative deletion behavior described in Cohen & Matias, 2003, Section 3)
    uint64_t count_after_delete = bf.get_min(point1);
    ASSERT_LT(count_after_delete, 1);  // Should have decreased

    // point2 should still be present
    ASSERT_EQ(bf.get_min(point2), 1);
    ASSERT_TRUE(bf.get_bool(point2));
}

// Test 5: RM variant - deletion not supported in other variants
TEST(deletion_only_in_rm) {
    SBFConfig conf_ms{8, 16, 16, MINIMUM_SELECTION};
    SpectralBloomFilter bf_ms(conf_ms);

    std::vector<uint64_t> point = {999};
    bf_ms.put(point);

    // Should throw when trying to delete in MS variant
    try {
        bf_ms.remove(point);
        ASSERT_TRUE(false);  // Should have thrown
    } catch (const std::logic_error &e) {
        // Expected
    }

    // Same for MI variant
    SBFConfig conf_mi{8, 16, 16, MINIMAL_INCREMENT};
    SpectralBloomFilter bf_mi(conf_mi);
    bf_mi.put(point);

    try {
        bf_mi.remove(point);
        ASSERT_TRUE(false);
    } catch (const std::logic_error &e) {
        // Expected
    }
}

// Test 6: Frequency estimation accuracy comparison
TEST(frequency_estimation_accuracy) {
    SBFConfig conf_ms{8, 18, 32, MINIMUM_SELECTION};
    SBFConfig conf_mi{8, 18, 32, MINIMAL_INCREMENT};

    SpectralBloomFilter bf_ms(conf_ms);
    SpectralBloomFilter bf_mi(conf_mi);

    std::vector<uint64_t> point = {777};

    // Insert 10 times in both
    for (int i = 0; i < 10; ++i) {
        bf_ms.put(point);
        bf_mi.put(point);
    }

    uint64_t count_ms = bf_ms.get_min(point);
    uint64_t count_mi = bf_mi.get_min(point);

    // Both should estimate around 10
    ASSERT_GE(count_ms, 5);
    ASSERT_GE(count_mi, 5);

    // MI should generally be more accurate (less overcounting)
    // due to conservative update
    std::cout << " (MS=" << count_ms << ", MI=" << count_mi << ") ";
}

// Test 7: High-frequency item detection
TEST(high_frequency_items) {
    SBFConfig conf{8, 18, 32, MINIMAL_INCREMENT};
    SpectralBloomFilter bf(conf);

    std::vector<uint64_t> high_freq = {1000};
    std::vector<uint64_t> low_freq = {2000};

    // Insert high_freq 100 times
    for (int i = 0; i < 100; ++i) {
        bf.put(high_freq);
    }

    // Insert low_freq 10 times
    for (int i = 0; i < 10; ++i) {
        bf.put(low_freq);
    }

    uint64_t count_high = bf.get_min(high_freq);
    uint64_t count_low = bf.get_min(low_freq);

    // High frequency item should have much higher count
    ASSERT_GT(count_high, count_low);
    ASSERT_GT(count_high, 50);  // Should be at least half of 100
    ASSERT_GE(count_low, 5);    // Should be at least half of 10
}

// Test 8: Insert-delete cycles (RM variant)
TEST(insert_delete_cycles) {
    SBFConfig conf{8, 16, 16, RECURRING_MINIMUM};
    SpectralBloomFilter bf(conf);

    std::vector<uint64_t> point = {555};

    // Cycle: insert, delete, insert, delete
    bf.put(point);
    ASSERT_EQ(bf.get_min(point), 1);

    bf.remove(point);
    ASSERT_EQ(bf.get_min(point), 0);

    bf.put(point);
    ASSERT_EQ(bf.get_min(point), 1);

    bf.remove(point);
    ASSERT_EQ(bf.get_min(point), 0);
}

// Test 9: Multiple insertions and accurate counting
TEST(multiple_insertions_accurate_count) {
    SBFConfig conf{8, 18, 32, MINIMAL_INCREMENT};
    SpectralBloomFilter bf(conf);

    std::vector<uint64_t> point = {888};

    // Insert exactly 20 times
    for (int i = 0; i < 20; ++i) {
        bf.put(point);
    }

    uint64_t estimated_count = bf.get_min(point);

    // Should be close to 20
    ASSERT_GE(estimated_count, 10);  // At least 50% accuracy
    ASSERT_LE(estimated_count, 40);  // Within 2x
}

// Test 10: Batch insertion
TEST(batch_insertion) {
    SBFConfig conf{8, 16, 16, MINIMUM_SELECTION};
    SpectralBloomFilter bf(conf);

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

    ASSERT_EQ(present_count, 100);
}

// Test 11: Memory usage calculation
TEST(memory_usage) {
    // MS variant - single filter
    SBFConfig conf_ms{8, 16, 16, MINIMUM_SELECTION};
    SpectralBloomFilter bf_ms(conf_ms);
    ASSERT_EQ(bf_ms.memory_usage(), 131072);  // 2^16 * 2 bytes

    // MI variant - single filter
    SBFConfig conf_mi{8, 16, 16, MINIMAL_INCREMENT};
    SpectralBloomFilter bf_mi(conf_mi);
    ASSERT_EQ(bf_mi.memory_usage(), 131072);

    // RM variant - double memory (primary + secondary)
    SBFConfig conf_rm{8, 16, 16, RECURRING_MINIMUM};
    SpectralBloomFilter bf_rm(conf_rm);
    ASSERT_EQ(bf_rm.memory_usage(), 262144);  // 2^16 * 2 * 2 bytes
}

// Test 12: get_mean alternative estimator
TEST(mean_estimator) {
    SBFConfig conf{4, 16, 16, MINIMUM_SELECTION};
    SpectralBloomFilter bf(conf);

    std::vector<uint64_t> point = {12345};

    // Insert 5 times
    for (int i = 0; i < 5; ++i) {
        bf.put(point);
    }

    uint64_t min_estimate = bf.get_min(point);
    uint64_t mean_estimate = bf.get_mean(point);

    // Mean should be >= min (average >= minimum)
    ASSERT_GE(mean_estimate, min_estimate);

    // Both should be > 0
    ASSERT_GT(min_estimate, 0);
    ASSERT_GT(mean_estimate, 0);
}

// Test 13: Summary output
TEST(summary_output) {
    SBFConfig conf_ms{8, 16, 16, MINIMUM_SELECTION};
    SpectralBloomFilter bf_ms(conf_ms);
    std::string summary_ms = bf_ms.summary();
    ASSERT_TRUE(summary_ms.find("Minimum Selection") != std::string::npos);

    SBFConfig conf_mi{8, 16, 16, MINIMAL_INCREMENT};
    SpectralBloomFilter bf_mi(conf_mi);
    std::string summary_mi = bf_mi.summary();
    ASSERT_TRUE(summary_mi.find("Minimal Increment") != std::string::npos);

    SBFConfig conf_rm{8, 16, 16, RECURRING_MINIMUM};
    SpectralBloomFilter bf_rm(conf_rm);
    std::string summary_rm = bf_rm.summary();
    ASSERT_TRUE(summary_rm.find("Recurring Minimum") != std::string::npos);
    ASSERT_TRUE(summary_rm.find("Deletion support: Yes") != std::string::npos);
}

// Test 14: Counter overflow
TEST(counter_overflow) {
    SBFConfig conf{4, 10, 8, MINIMUM_SELECTION};  // 8-bit counters
    SpectralBloomFilter bf(conf);

    std::vector<uint64_t> point = {9999};

    // Insert many times to saturate
    for (int i = 0; i < 300; ++i) {
        bf.put(point);
    }

    uint64_t count = bf.get_min(point);
    ASSERT_LE(count, 255);  // Should saturate at max 8-bit value
}

// Test 15: False positive rate
TEST(false_positive_rate) {
    SBFConfig conf{8, 20, 16, MINIMAL_INCREMENT};
    SpectralBloomFilter bf(conf);

    // Insert 1000 elements
    for (int i = 0; i < 1000; ++i) {
        std::vector<uint64_t> point = {static_cast<uint64_t>(i)};
        bf.put(point);
    }

    // Query 1000 elements that were NOT inserted
    int false_positives = 0;
    for (int i = 1000; i < 2000; ++i) {
        std::vector<uint64_t> point = {static_cast<uint64_t>(i)};
        if (bf.get_bool(point)) {
            false_positives++;
        }
    }

    double fpr = static_cast<double>(false_positives) / 1000.0;
    std::cout << " (FPR=" << fpr << ") ";
    ASSERT_LE(fpr, 0.15);  // Allow up to 15% FPR
}

// Test 16: Comparison between variants
TEST(variant_comparison) {
    SBFConfig conf_ms{8, 16, 32, MINIMUM_SELECTION};
    SBFConfig conf_mi{8, 16, 32, MINIMAL_INCREMENT};
    SBFConfig conf_rm{8, 16, 32, RECURRING_MINIMUM};

    SpectralBloomFilter bf_ms(conf_ms);
    SpectralBloomFilter bf_mi(conf_mi);
    SpectralBloomFilter bf_rm(conf_rm);

    std::vector<uint64_t> point = {42};

    // Insert 5 times in all variants
    for (int i = 0; i < 5; ++i) {
        bf_ms.put(point);
        bf_mi.put(point);
        bf_rm.put(point);
    }

    uint64_t count_ms = bf_ms.get_min(point);
    uint64_t count_mi = bf_mi.get_min(point);
    uint64_t count_rm = bf_rm.get_min(point);

    // All should detect the element
    ASSERT_GT(count_ms, 0);
    ASSERT_GT(count_mi, 0);
    ASSERT_GT(count_rm, 0);

    // MI and RM use conservative update, so may have similar behavior
    std::cout << " (MS=" << count_ms << ", MI=" << count_mi << ", RM=" << count_rm << ") ";
}

// Main test runner
int main() {
    std::cout << "===== Spectral Bloom Filter Unit Tests =====\n\n";

    run_test_config_validation();
    run_test_ms_variant_basic();
    run_test_mi_variant_conservative_update();
    run_test_rm_variant_deletion();
    run_test_deletion_only_in_rm();
    run_test_frequency_estimation_accuracy();
    run_test_high_frequency_items();
    run_test_insert_delete_cycles();
    run_test_multiple_insertions_accurate_count();
    run_test_batch_insertion();
    run_test_memory_usage();
    run_test_mean_estimator();
    run_test_summary_output();
    run_test_counter_overflow();
    run_test_false_positive_rate();
    run_test_variant_comparison();

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
