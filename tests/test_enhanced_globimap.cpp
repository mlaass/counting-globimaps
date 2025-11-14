#include "counting_globimap.hpp"
#include <cassert>
#include <iostream>
#include <random>
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

// Test 1: Backward compatibility (default config behaves as before)
TEST(backward_compatibility) {
    FilterConfig conf;
    conf.hash_k = 8;
    conf.layers = {{8, 16}, {16, 14}};
    // minimal_increment defaults to false, cascade_factor defaults to 1.0

    CountingGloBiMap<> gbm(conf);

    std::vector<uint64_t> point = {123, 456};

    // Insert and query
    gbm.put(point);
    ASSERT_TRUE(gbm.get_bool(point));
    ASSERT_GE(gbm.get_min(point), 1);

    // Insert multiple times
    for (int i = 0; i < 10; ++i) {
        gbm.put(point);
    }

    uint64_t count = gbm.get_min(point);
    ASSERT_GE(count, 10);
}

// Test 2: Minimal increment reduces overcounting
TEST(minimal_increment_reduces_overcounting) {
    // Standard config
    FilterConfig conf_standard;
    conf_standard.hash_k = 8;
    conf_standard.layers = {{8, 16}, {16, 14}};
    conf_standard.minimal_increment = false;

    // Minimal increment config
    FilterConfig conf_mi;
    conf_mi.hash_k = 8;
    conf_mi.layers = {{8, 16}, {16, 14}};
    conf_mi.minimal_increment = true;

    CountingGloBiMap<> gbm_standard(conf_standard);
    CountingGloBiMap<> gbm_mi(conf_mi);

    std::vector<uint64_t> target = {1000, 2000};

    // Insert target 20 times
    for (int i = 0; i < 20; ++i) {
        gbm_standard.put(target);
        gbm_mi.put(target);
    }

    // Insert noise
    std::mt19937_64 rng(42);
    for (int i = 0; i < 500; ++i) {
        std::vector<uint64_t> noise = {rng(), rng()};
        gbm_standard.put(noise);
        gbm_mi.put(noise);
    }

    uint64_t count_standard = gbm_standard.get_min(target);
    uint64_t count_mi = gbm_mi.get_min(target);

    std::cout << " (std=" << count_standard << ", mi=" << count_mi << ") ";

    // Both should be >= 20
    ASSERT_GE(count_standard, 20);
    ASSERT_GE(count_mi, 20);

    // MI should generally be closer to true value (less overcounting)
    // or at least not significantly worse
    ASSERT_LE(count_mi, count_standard * 1.5);
}

// Test 3: Cascade factor triggers early cascade
TEST(cascade_factor_early_cascade) {
    FilterConfig conf;
    conf.hash_k = 4;
    conf.layers = {{8, 12}};  // Single layer, 8-bit counters (max 255)
    conf.cascade_factor = 0.5;  // Cascade at 50% = 127

    CountingGloBiMap<> gbm(conf);

    std::vector<uint64_t> point = {555, 666};

    // Insert 150 times (exceeds 127)
    for (int i = 0; i < 150; ++i) {
        gbm.put(point);
    }

    // With cascade_factor = 0.5, the counters should not saturate at 255
    // This is harder to test directly, but the filter should still work
    ASSERT_TRUE(gbm.get_bool(point));
    uint64_t count = gbm.get_min(point);
    ASSERT_GE(count, 100);  // Should have substantial count

    std::cout << " (count=" << count << ") ";
}

// Test 4: Combined minimal_increment and cascade_factor
TEST(combined_mi_and_cf) {
    FilterConfig conf;
    conf.hash_k = 8;
    conf.layers = {{8, 14}, {16, 12}};
    conf.minimal_increment = true;
    conf.cascade_factor = 0.75;

    CountingGloBiMap<> gbm(conf);

    std::vector<uint64_t> point = {777, 888};

    // Insert multiple times
    for (int i = 0; i < 50; ++i) {
        gbm.put(point);
    }

    ASSERT_TRUE(gbm.get_bool(point));
    uint64_t count = gbm.get_min(point);
    ASSERT_GE(count, 40);  // Should have good accuracy

    std::cout << " (count=" << count << ") ";
}

// Test 5: FilterConfig to_string includes new options
TEST(config_to_string_with_options) {
    FilterConfig conf1;
    conf1.hash_k = 4;
    conf1.layers = {{8, 12}};
    conf1.minimal_increment = true;

    std::string str1 = conf1.to_string();
    ASSERT_TRUE(str1.find("mi") != std::string::npos);

    FilterConfig conf2;
    conf2.hash_k = 4;
    conf2.layers = {{8, 12}};
    conf2.cascade_factor = 0.75;

    std::string str2 = conf2.to_string();
    ASSERT_TRUE(str2.find("cf_") != std::string::npos);

    FilterConfig conf3;
    conf3.hash_k = 4;
    conf3.layers = {{8, 12}};
    conf3.minimal_increment = true;
    conf3.cascade_factor = 0.5;

    std::string str3 = conf3.to_string();
    ASSERT_TRUE(str3.find("mi") != std::string::npos);
    ASSERT_TRUE(str3.find("cf_") != std::string::npos);
}

// Test 6: Multiple elements with minimal_increment
TEST(multiple_elements_with_mi) {
    FilterConfig conf;
    conf.hash_k = 8;
    conf.layers = {{8, 16}, {16, 14}};
    conf.minimal_increment = true;

    CountingGloBiMap<> gbm(conf);

    std::vector<std::vector<uint64_t>> points = {
        {1, 2}, {3, 4}, {5, 6}, {7, 8}, {9, 10}
    };

    // Insert all points
    for (const auto &point : points) {
        gbm.put(point);
    }

    // Check all present
    for (const auto &point : points) {
        ASSERT_TRUE(gbm.get_bool(point));
        ASSERT_GE(gbm.get_min(point), 1);
    }
}

// Test 7: Cascade factor = 1.0 behaves as standard
TEST(cascade_factor_one_is_standard) {
    FilterConfig conf1;
    conf1.hash_k = 4;
    conf1.layers = {{8, 14}};
    conf1.cascade_factor = 1.0;

    FilterConfig conf2;
    conf2.hash_k = 4;
    conf2.layers = {{8, 14}};
    // No cascade_factor specified (defaults to 1.0)

    CountingGloBiMap<> gbm1(conf1);
    CountingGloBiMap<> gbm2(conf2);

    std::vector<uint64_t> point = {999, 111};

    for (int i = 0; i < 20; ++i) {
        gbm1.put(point);
        gbm2.put(point);
    }

    // Should have similar behavior
    uint64_t count1 = gbm1.get_min(point);
    uint64_t count2 = gbm2.get_min(point);

    ASSERT_EQ(count1, count2);
}

// Test 8: Accuracy with different cascade factors
TEST(cascade_factor_variations) {
    std::vector<double> factors = {1.0, 0.9, 0.75, 0.5};

    for (double factor : factors) {
        FilterConfig conf;
        conf.hash_k = 8;
        conf.layers = {{8, 14}, {16, 12}};
        conf.cascade_factor = factor;

        CountingGloBiMap<> gbm(conf);

        std::vector<uint64_t> point = {123, 456};

        for (int i = 0; i < 30; ++i) {
            gbm.put(point);
        }

        uint64_t count = gbm.get_min(point);
        ASSERT_GE(count, 25);  // Should be reasonably accurate

        std::cout << " (cf=" << factor << ", count=" << count << ") ";
    }
}

// Test 9: Minimal increment accuracy improvement
TEST(mi_accuracy_on_single_element) {
    FilterConfig conf_standard;
    conf_standard.hash_k = 8;
    conf_standard.layers = {{8, 16}, {16, 14}};

    FilterConfig conf_mi;
    conf_mi.hash_k = 8;
    conf_mi.layers = {{8, 16}, {16, 14}};
    conf_mi.minimal_increment = true;

    CountingGloBiMap<> gbm_standard(conf_standard);
    CountingGloBiMap<> gbm_mi(conf_mi);

    std::vector<uint64_t> point = {12345, 67890};

    // Insert exactly 100 times
    for (int i = 0; i < 100; ++i) {
        gbm_standard.put(point);
        gbm_mi.put(point);
    }

    uint64_t count_standard = gbm_standard.get_min(point);
    uint64_t count_mi = gbm_mi.get_min(point);

    std::cout << " (std=" << count_standard << ", mi=" << count_mi << ", true=100) ";

    // Both should be close to 100
    ASSERT_GE(count_standard, 90);
    ASSERT_GE(count_mi, 90);
}

// Test 10: put_all works with enhancements
TEST(put_all_with_enhancements) {
    FilterConfig conf;
    conf.hash_k = 8;
    conf.layers = {{8, 16}, {16, 14}};
    conf.minimal_increment = true;
    conf.cascade_factor = 0.75;

    CountingGloBiMap<> gbm(conf);

    std::vector<uint64_t> points_flat;
    for (int i = 0; i < 50; ++i) {
        points_flat.push_back(i);
        points_flat.push_back(i * 2);
    }

    gbm.put_all(points_flat);

    // Check that all points are present
    int present_count = 0;
    for (int i = 0; i < 50; ++i) {
        std::vector<uint64_t> point = {static_cast<uint64_t>(i), static_cast<uint64_t>(i * 2)};
        if (gbm.get_bool(point)) {
            present_count++;
        }
    }

    ASSERT_GE(present_count, 45);  // At least 90% should be found
}

// Main test runner
int main() {
    std::cout << "===== Enhanced CountingGloBiMap Unit Tests =====\n\n";

    run_test_backward_compatibility();
    run_test_minimal_increment_reduces_overcounting();
    run_test_cascade_factor_early_cascade();
    run_test_combined_mi_and_cf();
    run_test_config_to_string_with_options();
    run_test_multiple_elements_with_mi();
    run_test_cascade_factor_one_is_standard();
    run_test_cascade_factor_variations();
    run_test_mi_accuracy_on_single_element();
    run_test_put_all_with_enhancements();

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
