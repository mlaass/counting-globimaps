/*
 * Unit tests for cache-optimal bloom filter implementations
 *
 * Tests:
 * - BasicBloomFilterAdapter (baseline)
 * - BlockedBloomFilter
 * - RegisterBlockedBloomFilter
 * - SimdBloomFilter (if AVX2 available)
 * - PatternedSimdBloomFilter (if AVX2 available)
 */

#include "blocked_bloom_filter.hpp"
#include "register_blocked_bf.hpp"
#include "simd_bloom_filter.hpp"
#include <cassert>
#include <chrono>
#include <cmath>
#include <iostream>
#include <random>
#include <set>
#include <vector>

using namespace globimap;

// Simple test framework
int test_count = 0;
int test_passed = 0;

#define TEST(name)                                          \
    void test_##name();                                     \
    void run_test_##name() {                                \
        test_count++;                                       \
        std::cout << "Running test: " << #name << "... ";   \
        try {                                               \
            test_##name();                                  \
            test_passed++;                                  \
            std::cout << "PASSED\n";                        \
        } catch (const std::exception &e) {                 \
            std::cout << "FAILED: " << e.what() << "\n";    \
        } catch (...) {                                     \
            std::cout << "FAILED: Unknown exception\n";     \
        }                                                   \
    }                                                       \
    void test_##name()

#define ASSERT_TRUE(cond)                                   \
    if (!(cond))                                            \
        throw std::runtime_error("Assertion failed: " #cond)

#define ASSERT_FALSE(cond)                                  \
    if (cond)                                               \
        throw std::runtime_error("Assertion failed: !" #cond)

#define ASSERT_EQ(a, b)                                     \
    if ((a) != (b))                                         \
        throw std::runtime_error("Assertion failed: " #a " == " #b)

#define ASSERT_GT(a, b)                                     \
    if ((a) <= (b))                                         \
        throw std::runtime_error("Assertion failed: " #a " > " #b)

#define ASSERT_GE(a, b)                                     \
    if ((a) < (b))                                          \
        throw std::runtime_error("Assertion failed: " #a " >= " #b)

#define ASSERT_LE(a, b)                                     \
    if ((a) > (b))                                          \
        throw std::runtime_error("Assertion failed: " #a " <= " #b)

#define ASSERT_LT(a, b)                                     \
    if ((a) >= (b))                                         \
        throw std::runtime_error("Assertion failed: " #a " < " #b)

// ============================================================================
// BasicBloomFilterAdapter Tests
// ============================================================================

TEST(basic_config_validation) {
    BlockedBFConfig conf;
    conf.expected_items = 10000;
    conf.false_positive_rate = 0.01;
    conf.validate();  // Should not throw

    // Invalid FPR
    try {
        BlockedBFConfig bad;
        bad.false_positive_rate = 0.0;
        bad.validate();
        ASSERT_TRUE(false);  // Should have thrown
    } catch (const std::invalid_argument &) {
        // Expected
    }
}

TEST(basic_put_get) {
    BlockedBFConfig conf;
    conf.expected_items = 10000;
    conf.false_positive_rate = 0.01;
    BasicBloomFilterAdapter bf(conf);

    std::vector<uint64_t> point = {123, 456};
    ASSERT_FALSE(bf.get_bool(point));

    bf.put(point);
    ASSERT_TRUE(bf.get_bool(point));
    ASSERT_EQ(bf.get_min(point), 1);
}

TEST(basic_no_false_negatives) {
    BlockedBFConfig conf;
    conf.expected_items = 1000;
    conf.false_positive_rate = 0.01;
    BasicBloomFilterAdapter bf(conf);

    std::mt19937_64 rng(42);
    std::vector<std::vector<uint64_t>> inserted;

    for (int i = 0; i < 500; ++i) {
        std::vector<uint64_t> point = {rng(), rng()};
        bf.put(point);
        inserted.push_back(point);
    }

    // All inserted elements must be found
    for (const auto &point : inserted) {
        ASSERT_TRUE(bf.get_bool(point));
    }
}

TEST(basic_fpr_reasonable) {
    BlockedBFConfig conf;
    conf.expected_items = 10000;
    conf.false_positive_rate = 0.01;
    BasicBloomFilterAdapter bf(conf);

    // Insert n elements
    for (uint64_t i = 0; i < conf.expected_items; ++i) {
        bf.put({i, 0});
    }

    // Query n elements that were NOT inserted
    int false_positives = 0;
    for (uint64_t i = conf.expected_items; i < 2 * conf.expected_items; ++i) {
        if (bf.get_bool({i, 0})) {
            false_positives++;
        }
    }

    double measured_fpr = static_cast<double>(false_positives) / conf.expected_items;
    std::cout << " (FPR=" << measured_fpr << ") ";
    ASSERT_LE(measured_fpr, 0.05);  // Allow some margin
}

TEST(basic_clear) {
    BlockedBFConfig conf;
    conf.expected_items = 1000;
    conf.false_positive_rate = 0.01;
    BasicBloomFilterAdapter bf(conf);

    bf.put({100, 200});
    ASSERT_TRUE(bf.get_bool({100, 200}));

    bf.clear();
    ASSERT_FALSE(bf.get_bool({100, 200}));
}

// ============================================================================
// BlockedBloomFilter Tests
// ============================================================================

TEST(blocked_put_get) {
    BlockedBFConfig conf;
    conf.expected_items = 10000;
    conf.false_positive_rate = 0.01;
    BlockedBloomFilter bf(conf);

    std::vector<uint64_t> point = {789, 012};
    ASSERT_FALSE(bf.get_bool(point));

    bf.put(point);
    ASSERT_TRUE(bf.get_bool(point));
}

TEST(blocked_no_false_negatives) {
    BlockedBFConfig conf;
    conf.expected_items = 1000;
    conf.false_positive_rate = 0.01;
    BlockedBloomFilter bf(conf);

    std::mt19937_64 rng(123);
    std::vector<std::vector<uint64_t>> inserted;

    for (int i = 0; i < 500; ++i) {
        std::vector<uint64_t> point = {rng(), rng()};
        bf.put(point);
        inserted.push_back(point);
    }

    for (const auto &point : inserted) {
        ASSERT_TRUE(bf.get_bool(point));
    }
}

TEST(blocked_fpr_reasonable) {
    BlockedBFConfig conf;
    conf.expected_items = 10000;
    conf.false_positive_rate = 0.01;
    BlockedBloomFilter bf(conf);

    for (uint64_t i = 0; i < conf.expected_items; ++i) {
        bf.put({i, 0});
    }

    int false_positives = 0;
    for (uint64_t i = conf.expected_items; i < 2 * conf.expected_items; ++i) {
        if (bf.get_bool({i, 0})) {
            false_positives++;
        }
    }

    double measured_fpr = static_cast<double>(false_positives) / conf.expected_items;
    std::cout << " (FPR=" << measured_fpr << ") ";
    // Blocked filters may have slightly higher FPR due to block constraints
    ASSERT_LE(measured_fpr, 0.10);
}

TEST(blocked_memory_usage) {
    BlockedBFConfig conf;
    conf.expected_items = 10000;
    conf.false_positive_rate = 0.01;
    BlockedBloomFilter bf(conf);

    uint64_t mem = bf.memory_usage();
    ASSERT_GT(mem, 0);

    // Memory should be reasonable (not orders of magnitude off)
    uint64_t expected_bits = conf.computed_bits();
    uint64_t expected_bytes = (expected_bits + 7) / 8;
    ASSERT_GE(mem, expected_bytes / 2);
    ASSERT_LE(mem, expected_bytes * 2);
}

TEST(blocked_summary) {
    BlockedBFConfig conf;
    conf.expected_items = 1000;
    conf.false_positive_rate = 0.05;
    BlockedBloomFilter bf(conf);

    std::string s = bf.summary();
    ASSERT_TRUE(s.find("BlockedBloomFilter") != std::string::npos);
    ASSERT_TRUE(s.find("num_blocks") != std::string::npos);
}

// ============================================================================
// RegisterBlockedBloomFilter Tests
// ============================================================================

TEST(register_blocked_put_get) {
    RegisterBlockedBFConfig conf;
    conf.expected_items = 10000;
    conf.false_positive_rate = 0.01;
    RegisterBlockedBF bf(conf);

    std::vector<uint64_t> point = {111, 222};
    ASSERT_FALSE(bf.get_bool(point));

    bf.put(point);
    ASSERT_TRUE(bf.get_bool(point));
}

TEST(register_blocked_no_false_negatives) {
    RegisterBlockedBFConfig conf;
    conf.expected_items = 1000;
    conf.false_positive_rate = 0.01;
    RegisterBlockedBF bf(conf);

    std::mt19937_64 rng(456);
    std::vector<std::vector<uint64_t>> inserted;

    for (int i = 0; i < 500; ++i) {
        std::vector<uint64_t> point = {rng(), rng()};
        bf.put(point);
        inserted.push_back(point);
    }

    for (const auto &point : inserted) {
        ASSERT_TRUE(bf.get_bool(point));
    }
}

TEST(register_blocked_compensation) {
    // Negative compensation = less memory, higher FPR
    RegisterBlockedBFConfig conf1;
    conf1.expected_items = 10000;
    conf1.false_positive_rate = 0.01;
    conf1.compensation = -2;
    RegisterBlockedBloomFilter<-2> bf1(conf1);

    // Positive compensation = more memory, lower FPR
    RegisterBlockedBFConfig conf2;
    conf2.expected_items = 10000;
    conf2.false_positive_rate = 0.01;
    conf2.compensation = 2;
    RegisterBlockedBloomFilter<2> bf2(conf2);

    ASSERT_LT(bf1.memory_usage(), bf2.memory_usage());
}

TEST(register_blocked_fpr) {
    RegisterBlockedBFConfig conf;
    conf.expected_items = 10000;
    conf.false_positive_rate = 0.01;
    RegisterBlockedBF bf(conf);

    for (uint64_t i = 0; i < conf.expected_items; ++i) {
        bf.put({i, 0});
    }

    int false_positives = 0;
    for (uint64_t i = conf.expected_items; i < 2 * conf.expected_items; ++i) {
        if (bf.get_bool({i, 0})) {
            false_positives++;
        }
    }

    double measured_fpr = static_cast<double>(false_positives) / conf.expected_items;
    std::cout << " (FPR=" << measured_fpr << ") ";
    ASSERT_LE(measured_fpr, 0.15);  // Register-blocked may have higher FPR
}

// ============================================================================
// SIMD Bloom Filter Tests (conditional on AVX2)
// ============================================================================

TEST(simd_availability) {
    std::cout << " (AVX2=" << (simd_available() ? "yes" : "no") << ") ";
    // Just checking it doesn't crash
}

#if SIMD_BF_AVX2_AVAILABLE

TEST(simd_put_get) {
    SimdBFConfig conf;
    conf.expected_items = 10000;
    conf.false_positive_rate = 0.01;
    SimdBloomFilter bf(conf);

    std::vector<uint64_t> point = {333, 444};
    bf.put(point);
    ASSERT_TRUE(bf.get_bool(point));
}

TEST(simd_no_false_negatives) {
    SimdBFConfig conf;
    conf.expected_items = 1000;
    conf.false_positive_rate = 0.01;
    SimdBloomFilter bf(conf);

    std::mt19937_64 rng(789);
    std::vector<std::vector<uint64_t>> inserted;

    for (int i = 0; i < 500; ++i) {
        std::vector<uint64_t> point = {rng(), rng()};
        bf.put(point);
        inserted.push_back(point);
    }

    for (const auto &point : inserted) {
        ASSERT_TRUE(bf.get_bool(point));
    }
}

TEST(simd_batch_operations) {
    SimdBFConfig conf;
    conf.expected_items = 10000;
    conf.false_positive_rate = 0.01;
    SimdBloomFilter bf(conf);

    // Create batch of 8 points
    std::array<std::vector<uint64_t>, 8> points;
    for (int i = 0; i < 8; ++i) {
        points[i] = {static_cast<uint64_t>(i * 100), static_cast<uint64_t>(i * 200)};
    }

    // Insert batch
    bf.put_batch(points);

    // Query batch
    auto results = bf.get_bool_batch(points);
    for (int i = 0; i < 8; ++i) {
        ASSERT_TRUE(results[i]);
    }
}

TEST(patterned_simd_put_get) {
    SimdBFConfig conf;
    conf.expected_items = 10000;
    conf.false_positive_rate = 0.01;
    conf.use_patterned = true;
    PatternedSimdBloomFilter bf(conf);

    std::vector<uint64_t> point = {555, 666};
    bf.put(point);
    ASSERT_TRUE(bf.get_bool(point));
}

TEST(patterned_simd_no_false_negatives) {
    SimdBFConfig conf;
    conf.expected_items = 1000;
    conf.false_positive_rate = 0.01;
    PatternedSimdBloomFilter bf(conf);

    std::mt19937_64 rng(101112);
    std::vector<std::vector<uint64_t>> inserted;

    for (int i = 0; i < 500; ++i) {
        std::vector<uint64_t> point = {rng(), rng()};
        bf.put(point);
        inserted.push_back(point);
    }

    for (const auto &point : inserted) {
        ASSERT_TRUE(bf.get_bool(point));
    }
}

#endif // SIMD_BF_AVX2_AVAILABLE

// ============================================================================
// Comparison Tests
// ============================================================================

TEST(compare_memory_efficiency) {
    const uint64_t n = 10000;
    const double fpr = 0.01;

    BlockedBFConfig conf;
    conf.expected_items = n;
    conf.false_positive_rate = fpr;

    BasicBloomFilterAdapter basic(conf);
    BlockedBloomFilter blocked(conf);

    RegisterBlockedBFConfig rb_conf;
    rb_conf.expected_items = n;
    rb_conf.false_positive_rate = fpr;
    RegisterBlockedBF reg_blocked(rb_conf);

    std::cout << "\n  Basic: " << basic.memory_usage() << " bytes"
              << "\n  Blocked: " << blocked.memory_usage() << " bytes"
              << "\n  RegisterBlocked: " << reg_blocked.memory_usage() << " bytes ";

    // All should have reasonable memory usage
    ASSERT_GT(basic.memory_usage(), 0);
    ASSERT_GT(blocked.memory_usage(), 0);
    ASSERT_GT(reg_blocked.memory_usage(), 0);
}

TEST(variable_length_vectors) {
    BlockedBFConfig conf;
    conf.expected_items = 10000;
    conf.false_positive_rate = 0.01;
    BlockedBloomFilter bf(conf);

    // 2D point
    bf.put({100, 200});
    ASSERT_TRUE(bf.get_bool({100, 200}));

    // 3D point (with category)
    bf.put({100, 200, 1});
    ASSERT_TRUE(bf.get_bool({100, 200, 1}));

    // Different category should be different
    // (may have false positive, but testing it doesn't crash)
    bf.get_bool({100, 200, 2});
}

// ============================================================================
// Main
// ============================================================================

int main() {
    std::cout << "===== Cache-Optimal Bloom Filter Unit Tests =====\n\n";

    // BasicBloomFilter tests
    run_test_basic_config_validation();
    run_test_basic_put_get();
    run_test_basic_no_false_negatives();
    run_test_basic_fpr_reasonable();
    run_test_basic_clear();

    // BlockedBloomFilter tests
    run_test_blocked_put_get();
    run_test_blocked_no_false_negatives();
    run_test_blocked_fpr_reasonable();
    run_test_blocked_memory_usage();
    run_test_blocked_summary();

    // RegisterBlockedBloomFilter tests
    run_test_register_blocked_put_get();
    run_test_register_blocked_no_false_negatives();
    run_test_register_blocked_compensation();
    run_test_register_blocked_fpr();

    // SIMD tests
    run_test_simd_availability();
#if SIMD_BF_AVX2_AVAILABLE
    run_test_simd_put_get();
    run_test_simd_no_false_negatives();
    run_test_simd_batch_operations();
    run_test_patterned_simd_put_get();
    run_test_patterned_simd_no_false_negatives();
#endif

    // Comparison tests
    run_test_compare_memory_efficiency();
    run_test_variable_length_vectors();

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
