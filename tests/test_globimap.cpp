#include "globimap.hpp"
#include <cassert>
#include <iostream>
#include <random>
#include <vector>

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

// Test 1: Basic configuration
TEST(basic_configure) {
    GloBiMap<> bf;
    bf.configure(8, 16);  // 8 hash functions, 2^16 bits
    ASSERT_EQ(bf.filter.size(), 65536);
}

// Test 2: Basic put and get
TEST(basic_put_get) {
    GloBiMap<> bf;
    bf.configure(8, 16);

    std::vector<uint64_t> point = {123, 456};

    // Not present before insertion
    ASSERT_FALSE(bf.get(point));

    // Insert
    bf.put(point);

    // Present after insertion
    ASSERT_TRUE(bf.get(point));
}

// Test 3: False negative guarantee - inserted elements always found
TEST(false_negative_guarantee) {
    GloBiMap<> bf;
    bf.configure(8, 20);  // Large filter to reduce collisions

    std::mt19937_64 rng(42);
    std::vector<std::vector<uint64_t>> inserted;

    // Insert 1000 random elements
    for (int i = 0; i < 1000; ++i) {
        std::vector<uint64_t> point = {rng(), rng()};
        bf.put(point);
        inserted.push_back(point);
    }

    // All inserted elements must be found
    for (const auto &point : inserted) {
        ASSERT_TRUE(bf.get(point));
    }
}

// Test 4: False positive rate
TEST(false_positive_rate) {
    GloBiMap<> bf;
    bf.configure(8, 20);  // 2^20 bits = 1MB

    // Insert 10000 elements
    for (int i = 0; i < 10000; ++i) {
        std::vector<uint64_t> point = {static_cast<uint64_t>(i), 0};
        bf.put(point);
    }

    // Query 10000 elements that were NOT inserted
    int false_positives = 0;
    for (int i = 10000; i < 20000; ++i) {
        std::vector<uint64_t> point = {static_cast<uint64_t>(i), 0};
        if (bf.get(point)) {
            false_positives++;
        }
    }

    double fpr = static_cast<double>(false_positives) / 10000.0;
    std::cout << " (FPR=" << fpr << ") ";
    ASSERT_LE(fpr, 0.05);  // Expect < 5% FPR with these parameters
}

// Test 5: Multi-element insertion
TEST(multi_element_insertion) {
    GloBiMap<> bf;
    bf.configure(8, 18);

    // Insert 1000 distinct elements
    for (int i = 0; i < 1000; ++i) {
        std::vector<uint64_t> point = {static_cast<uint64_t>(i * 100), static_cast<uint64_t>(i * 200)};
        bf.put(point);
    }

    // Check all are present
    int found = 0;
    for (int i = 0; i < 1000; ++i) {
        std::vector<uint64_t> point = {static_cast<uint64_t>(i * 100), static_cast<uint64_t>(i * 200)};
        if (bf.get(point)) {
            found++;
        }
    }
    ASSERT_EQ(found, 1000);
}

// Test 6: Stats output
TEST(stats_output) {
    GloBiMap<> bf;
    bf.configure(8, 16);

    // Insert some elements
    for (int i = 0; i < 100; ++i) {
        bf.put({static_cast<uint64_t>(i), static_cast<uint64_t>(i * 2)});
    }

    auto st = bf.stats();
    double ones = std::get<0>(st);
    double foz = std::get<1>(st);  // fraction of zeros

    ASSERT_GT(ones, 0);
    ASSERT_LT(ones, bf.filter.size());
    ASSERT_GT(foz, 0);
    ASSERT_LT(foz, 1);
}

// Test 7: Summary output
TEST(summary_output) {
    GloBiMap<> bf;
    bf.configure(8, 16);
    bf.put({100, 200});

    std::string summary = bf.summary();

    // Should contain expected fields
    ASSERT_TRUE(summary.find("storage_b") != std::string::npos);
    ASSERT_TRUE(summary.find("storage_kb") != std::string::npos);
    ASSERT_TRUE(summary.find("ones") != std::string::npos);
    ASSERT_TRUE(summary.find("foz") != std::string::npos);
}

// Test 8: Serialization roundtrip
TEST(serialization_roundtrip) {
    GloBiMap<> bf;
    bf.configure(8, 14);  // 2^14 = 16384 bits

    // Insert some elements
    std::vector<std::vector<uint64_t>> points = {
        {100, 200}, {300, 400}, {500, 600}, {700, 800}
    };
    for (const auto &p : points) {
        bf.put(p);
    }

    // Serialize
    std::string buf;
    bf.tobuffer(buf);

    // Create new filter and deserialize
    GloBiMap<> bf2;
    bf2.configure(8, 14);
    bf2._frombuffer(buf, bf.filter.size());

    // Check all points are found in restored filter
    for (const auto &p : points) {
        ASSERT_TRUE(bf2.get(p));
    }
}

// Test 9: Clear filter
TEST(clear_filter) {
    GloBiMap<> bf;
    bf.configure(8, 16);

    bf.put({100, 200});
    ASSERT_TRUE(bf.get({100, 200}));

    bf.clear();

    // After clear, filter should be empty
    ASSERT_EQ(bf.filter.size(), 0);
}

// Test 10: Rasterize basic
TEST(rasterize_basic) {
    GloBiMap<> bf;
    bf.configure(8, 20);

    // Insert some points in a grid
    bf.put({10, 10});
    bf.put({11, 11});
    bf.put({12, 10});

    // Rasterize a 5x5 region starting at (8, 8)
    auto &result = bf.rasterize(8, 8, 5, 5);

    ASSERT_EQ(result.size(), 25);

    // Check known points - (10,10) is at offset (2,2) in the 5x5 grid
    // storage[i * s1 + j] where i = row, j = col
    ASSERT_EQ(result[2 * 5 + 2], 1);  // (10, 10)
    ASSERT_EQ(result[3 * 5 + 3], 1);  // (11, 11)
    ASSERT_EQ(result[4 * 5 + 2], 1);  // (12, 10)
}

// Test 11: Error correction
TEST(error_correction) {
    GloBiMap<> bf;
    bf.configure(8, 20);

    // Insert a point
    bf.put({10, 10});
    ASSERT_TRUE(bf.get({10, 10}));

    // Register it as an error (false positive to suppress)
    bf.add_error({10, 10});

    // Rasterize and apply correction
    bf.rasterize(8, 8, 5, 5);
    auto &corrected = bf.apply_correction(8, 8, 5, 5);

    // After correction, the error point should be suppressed
    ASSERT_EQ(corrected[2 * 5 + 2], 0);  // (10, 10) at offset (2, 2)
}

// Test 12: Variable length vectors (multi-category support)
TEST(variable_length_vectors) {
    GloBiMap<> bf;
    bf.configure(8, 20);

    // Insert with different category dimensions
    bf.put({100, 200, 1});  // Category 1
    bf.put({100, 200, 2});  // Category 2
    bf.put({100, 200, 3});  // Category 3

    // Each should be found
    ASSERT_TRUE(bf.get({100, 200, 1}));
    ASSERT_TRUE(bf.get({100, 200, 2}));
    ASSERT_TRUE(bf.get({100, 200, 3}));

    // Different category should (likely) not be found
    // Note: may have false positives, so we just check basic functionality
    // The key is that different-length vectors hash differently

    // 2D query should be different from 3D
    // (may or may not be present due to hash collisions, just testing it doesn't crash)
    bf.get({100, 200});
}

// Test 13: Different filter sizes
TEST(different_filter_sizes) {
    for (size_t logm = 10; logm <= 20; logm += 2) {
        GloBiMap<> bf;
        bf.configure(8, logm);

        size_t expected_size = 1ULL << logm;
        ASSERT_EQ(bf.filter.size(), expected_size);

        bf.put({1000, 2000});
        ASSERT_TRUE(bf.get({1000, 2000}));
    }
}

// Test 14: Different k values
TEST(different_k_values) {
    for (size_t k = 1; k <= 16; k *= 2) {
        GloBiMap<> bf;
        bf.configure(k, 16);

        bf.put({999, 888});
        ASSERT_TRUE(bf.get({999, 888}));
    }
}

// Main test runner
int main() {
    std::cout << "===== GloBiMap (Binary Bloom Filter) Unit Tests =====\n\n";

    run_test_basic_configure();
    run_test_basic_put_get();
    run_test_false_negative_guarantee();
    run_test_false_positive_rate();
    run_test_multi_element_insertion();
    run_test_stats_output();
    run_test_summary_output();
    run_test_serialization_roundtrip();
    run_test_clear_filter();
    run_test_rasterize_basic();
    run_test_error_correction();
    run_test_variable_length_vectors();
    run_test_different_filter_sizes();
    run_test_different_k_values();

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
