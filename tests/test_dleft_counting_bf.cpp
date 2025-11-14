#include "dleft_counting_bf.hpp"
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
    // Valid configuration
    DLeftCBFConfig valid{1024, 4, 4, 3, 8};
    valid.validate();  // Should not throw

    // Invalid: num_buckets not divisible by d (1001 % 4 = 1)
    bool threw1 = false;
    try {
        DLeftCBFConfig conf{1001, 4, 4, 3, 8};
        conf.validate();
    } catch (const std::invalid_argument &e) {
        threw1 = true;
    }
    ASSERT_TRUE(threw1);

    // Invalid: d out of range (d=10 > 8)
    bool threw2 = false;
    try {
        DLeftCBFConfig conf{1000, 10, 4, 3, 8};
        conf.validate();
    } catch (const std::invalid_argument &e) {
        threw2 = true;
    }
    ASSERT_TRUE(threw2);

    // Invalid: d = 0
    bool threw3 = false;
    try {
        DLeftCBFConfig conf{1024, 0, 4, 3, 8};
        conf.validate();
    } catch (const std::invalid_argument &e) {
        threw3 = true;
    }
    ASSERT_TRUE(threw3);
}

// Test 2: Basic insertion and query
TEST(basic_insertion_query) {
    DLeftCBFConfig conf{1024, 4, 4, 3, 8};
    DLeftCountingBloomFilter bf(conf);

    std::vector<uint64_t> point = {123, 456};

    // Insert once
    bf.put(point);
    ASSERT_EQ(bf.get_min(point), 1);
    ASSERT_TRUE(bf.get_bool(point));

    // Insert again - should increment
    bf.put(point);
    ASSERT_EQ(bf.get_min(point), 2);

    // Insert third time
    bf.put(point);
    ASSERT_EQ(bf.get_min(point), 3);
}

// Test 3: Query non-existent element
TEST(query_nonexistent) {
    DLeftCBFConfig conf{1024, 4, 4, 3, 8};
    DLeftCountingBloomFilter bf(conf);

    std::vector<uint64_t> point1 = {100, 200};
    std::vector<uint64_t> point2 = {300, 400};

    bf.put(point1);

    // point2 was not inserted
    ASSERT_EQ(bf.get_min(point2), 0);
    ASSERT_FALSE(bf.get_bool(point2));
}

// Test 4: Deletion support
TEST(deletion_support) {
    DLeftCBFConfig conf{1024, 4, 4, 3, 8};
    DLeftCountingBloomFilter bf(conf);

    std::vector<uint64_t> point = {111, 222};

    // Insert 3 times
    bf.put(point);
    bf.put(point);
    bf.put(point);
    ASSERT_EQ(bf.get_min(point), 3);

    // Delete once
    bf.remove(point);
    ASSERT_EQ(bf.get_min(point), 2);

    // Delete again
    bf.remove(point);
    ASSERT_EQ(bf.get_min(point), 1);

    // Delete final time
    bf.remove(point);
    ASSERT_EQ(bf.get_min(point), 0);
    ASSERT_FALSE(bf.get_bool(point));
}

// Test 5: Multiple different elements
TEST(multiple_elements) {
    DLeftCBFConfig conf{2048, 4, 4, 3, 8};
    DLeftCountingBloomFilter bf(conf);

    std::vector<std::vector<uint64_t>> points = {
        {1, 2}, {3, 4}, {5, 6}, {7, 8}, {9, 10}
    };

    // Insert all points
    for (const auto &point : points) {
        bf.put(point);
    }

    // Check all present
    for (const auto &point : points) {
        ASSERT_TRUE(bf.get_bool(point));
        ASSERT_EQ(bf.get_min(point), 1);
    }
}

// Test 6: Load factor calculation
TEST(load_factor) {
    DLeftCBFConfig conf{1024, 4, 4, 3, 8};  // 1024 buckets * 4 slots = 4096 total slots
    DLeftCountingBloomFilter bf(conf);

    // Initially empty
    ASSERT_EQ(bf.load_factor(), 0.0);

    // Insert 100 elements
    for (int i = 0; i < 100; ++i) {
        std::vector<uint64_t> point = {static_cast<uint64_t>(i)};
        bf.put(point);
    }

    double lf = bf.load_factor();
    std::cout << " (load=" << lf << ") ";

    // Should have some occupancy
    ASSERT_GT(lf, 0.0);
    ASSERT_LT(lf, 0.1);  // Should be well below capacity
}

// Test 7: Counter saturation (small counter_bits)
TEST(counter_saturation) {
    DLeftCBFConfig conf{1024, 4, 4, 2, 8};  // 2-bit counters, max = 3
    DLeftCountingBloomFilter bf(conf);

    std::vector<uint64_t> point = {999};

    // Insert 10 times (more than max counter value)
    for (int i = 0; i < 10; ++i) {
        bf.put(point);
    }

    uint64_t count = bf.get_min(point);

    // Should saturate at 3 (2^2 - 1)
    ASSERT_LE(count, 3);
}

// Test 8: Batch insertion
TEST(batch_insertion) {
    DLeftCBFConfig conf{2048, 4, 4, 3, 8};
    DLeftCountingBloomFilter bf(conf);

    std::vector<std::vector<uint64_t>> points;
    for (int i = 0; i < 50; ++i) {
        points.push_back({static_cast<uint64_t>(i), static_cast<uint64_t>(i * 2)});
    }

    bf.put_all(points);

    // Check all points present
    int present_count = 0;
    for (const auto &point : points) {
        if (bf.get_bool(point)) {
            present_count++;
        }
    }

    ASSERT_EQ(present_count, 50);
}

// Test 9: Fingerprint collisions
TEST(fingerprint_behavior) {
    DLeftCBFConfig conf{1024, 4, 4, 3, 8};
    DLeftCountingBloomFilter bf(conf);

    // Insert many elements to test fingerprint distribution
    for (int i = 0; i < 200; ++i) {
        std::vector<uint64_t> point = {static_cast<uint64_t>(i)};
        bf.put(point);
    }

    // Query all inserted elements
    int found = 0;
    for (int i = 0; i < 200; ++i) {
        std::vector<uint64_t> point = {static_cast<uint64_t>(i)};
        if (bf.get_bool(point)) {
            found++;
        }
    }

    // Should find most elements (some may be lost due to fingerprint collisions or bucket overflow)
    ASSERT_GT(found, 150);  // At least 75% should be found
    std::cout << " (found=" << found << "/200) ";
}

// Test 10: Delete-insert cycles
TEST(delete_insert_cycles) {
    DLeftCBFConfig conf{1024, 4, 4, 3, 8};
    DLeftCountingBloomFilter bf(conf);

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

// Test 11: Memory usage
TEST(memory_usage) {
    DLeftCBFConfig conf{1024, 4, 4, 3, 8};  // 1024 buckets * 4 slots = 4096 slots
    DLeftCountingBloomFilter bf(conf);

    uint64_t memory = bf.memory_usage();

    // Should be approximately 4096 slots * 5 bytes/slot = ~20KB
    ASSERT_GT(memory, 15000);
    ASSERT_LT(memory, 25000);

    std::cout << " (memory=" << config_utils::format_memory(memory) << ") ";
}

// Test 12: Summary output
TEST(summary_output) {
    DLeftCBFConfig conf{1024, 4, 4, 3, 8};
    DLeftCountingBloomFilter bf(conf);

    std::string summary = bf.summary();

    ASSERT_TRUE(summary.find("d-Left") != std::string::npos);
    ASSERT_TRUE(summary.find("segments") != std::string::npos);
    ASSERT_TRUE(summary.find("Deletion support: Yes") != std::string::npos);
}

// Test 13: Different configurations
TEST(different_configurations) {
    // Small config
    DLeftCBFConfig conf_small{512, 4, 3, 2, 6};
    DLeftCountingBloomFilter bf_small(conf_small);

    // Large config
    DLeftCBFConfig conf_large{4096, 4, 4, 4, 8};
    DLeftCountingBloomFilter bf_large(conf_large);

    std::vector<uint64_t> point = {42};

    bf_small.put(point);
    bf_large.put(point);

    ASSERT_TRUE(bf_small.get_bool(point));
    ASSERT_TRUE(bf_large.get_bool(point));
}

// Test 14: High load scenario
TEST(high_load_scenario) {
    DLeftCBFConfig conf{1024, 4, 4, 3, 8};  // Total 4096 slots
    DLeftCountingBloomFilter bf(conf);

    // Insert many elements to stress test
    int inserted = 0;
    for (int i = 0; i < 2000; ++i) {
        std::vector<uint64_t> point = {static_cast<uint64_t>(i)};
        bf.put(point);
        inserted++;
    }

    double lf = bf.load_factor();
    std::cout << " (load=" << lf << ", inserted=" << inserted << ") ";

    // Load factor should be significant
    ASSERT_GT(lf, 0.2);
}

// Test 15: Exact counting (no false positives for counts)
TEST(exact_counting) {
    DLeftCBFConfig conf{2048, 4, 4, 4, 8};
    DLeftCountingBloomFilter bf(conf);

    std::vector<uint64_t> point = {12345};

    // Insert exactly 5 times
    for (int i = 0; i < 5; ++i) {
        bf.put(point);
    }

    uint64_t count = bf.get_min(point);

    // Should return exact count (unless fingerprint collision occurred)
    // d-Left CBF provides exact counts when fingerprint matches
    ASSERT_EQ(count, 5);
}

// Main test runner
int main() {
    std::cout << "===== d-Left Counting Bloom Filter Unit Tests =====\n\n";

    run_test_config_validation();
    run_test_basic_insertion_query();
    run_test_query_nonexistent();
    run_test_deletion_support();
    run_test_multiple_elements();
    run_test_load_factor();
    run_test_counter_saturation();
    run_test_batch_insertion();
    run_test_fingerprint_behavior();
    run_test_delete_insert_cycles();
    run_test_memory_usage();
    run_test_summary_output();
    run_test_different_configurations();
    run_test_high_load_scenario();
    run_test_exact_counting();

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
