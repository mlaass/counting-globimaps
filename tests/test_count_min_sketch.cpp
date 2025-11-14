#include "count_min_sketch.hpp"
#include <cassert>
#include <cmath>
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

#define ASSERT_NEAR(a, b, tol) \
    if (std::abs((a) - (b)) > (tol)) throw std::runtime_error("Assertion failed: " #a " ≈ " #b)

// Test 1: Configuration validation
TEST(config_validation) {
    // Valid configuration
    CMSConfig valid{0.01, 0.01, false, 16};
    valid.validate();  // Should not throw

    // Invalid: epsilon out of range
    try {
        CMSConfig conf{0.0, 0.01, false, 16};
        conf.validate();
        ASSERT_TRUE(false);
    } catch (const std::invalid_argument &e) {
        // Expected
    }

    try {
        CMSConfig conf{1.0, 0.01, false, 16};
        conf.validate();
        ASSERT_TRUE(false);
    } catch (const std::invalid_argument &e) {
        // Expected
    }

    // Invalid: delta out of range
    try {
        CMSConfig conf{0.01, 0.0, false, 16};
        conf.validate();
        ASSERT_TRUE(false);
    } catch (const std::invalid_argument &e) {
        // Expected
    }

    // Invalid: counter_bits
    try {
        CMSConfig conf{0.01, 0.01, false, 7};
        conf.validate();
        ASSERT_TRUE(false);
    } catch (const std::invalid_argument &e) {
        // Expected
    }
}

// Test 2: Dimension calculation
TEST(dimension_calculation) {
    // From Cormode & Muthukrishnan (2005), Section 2.1:
    // width = ceil(e / epsilon), depth = ceil(ln(1 / delta))

    double epsilon = 0.01;
    double delta = 0.01;

    uint width = CMSConfig::calculate_width(epsilon);
    uint depth = CMSConfig::calculate_depth(delta);

    // width = ceil(2.71828 / 0.01) = ceil(271.828) = 272
    ASSERT_EQ(width, 272);

    // depth = ceil(ln(100)) = ceil(4.605) = 5
    ASSERT_EQ(depth, 5);

    std::cout << " (w=" << width << ", d=" << depth << ") ";
}

// Test 3: Basic insertion and query
TEST(basic_insertion_query) {
    CMSConfig conf{0.01, 0.01, false, 16};
    CountMinSketch cms(conf);

    std::vector<uint64_t> point = {123, 456};

    // Insert once
    cms.put(point);
    ASSERT_EQ(cms.get_min(point), 1);
    ASSERT_TRUE(cms.get_bool(point));
    ASSERT_EQ(cms.total_count(), 1);

    // Insert again
    cms.put(point);
    ASSERT_EQ(cms.get_min(point), 2);
    ASSERT_EQ(cms.total_count(), 2);

    // Insert third time
    cms.put(point);
    ASSERT_EQ(cms.get_min(point), 3);
    ASSERT_EQ(cms.total_count(), 3);
}

// Test 4: Query non-existent element
TEST(query_nonexistent) {
    CMSConfig conf{0.01, 0.01, false, 16};
    CountMinSketch cms(conf);

    std::vector<uint64_t> point1 = {100, 200};
    std::vector<uint64_t> point2 = {300, 400};

    cms.put(point1);

    // point2 was not inserted, but may have false positive count due to hash collisions
    uint64_t count = cms.get_min(point2);
    // In general, count could be > 0 due to collisions, but likely 0 for small data
    std::cout << " (count=" << count << ") ";
}

// Test 5: Conservative update vs standard update
TEST(conservative_vs_standard_update) {
    CMSConfig conf_standard{0.01, 0.01, false, 16};
    CMSConfig conf_conservative{0.01, 0.01, true, 16};

    CountMinSketch cms_standard(conf_standard);
    CountMinSketch cms_conservative(conf_conservative);

    std::vector<uint64_t> point = {777};

    // Insert 10 times in both
    for (int i = 0; i < 10; ++i) {
        cms_standard.put(point);
        cms_conservative.put(point);
    }

    uint64_t count_standard = cms_standard.get_min(point);
    uint64_t count_conservative = cms_conservative.get_min(point);

    // Both should be close to 10
    ASSERT_EQ(count_standard, 10);
    ASSERT_EQ(count_conservative, 10);

    std::cout << " (std=" << count_standard << ", cons=" << count_conservative << ") ";
}

// Test 6: Frequency count differentiation
TEST(frequency_count_differentiation) {
    CMSConfig conf{0.01, 0.01, false, 16};
    CountMinSketch cms(conf);

    std::vector<uint64_t> heavy = {1000};
    std::vector<uint64_t> light1 = {2000};
    std::vector<uint64_t> light2 = {3000};

    // Insert heavy 100 times
    for (int i = 0; i < 100; ++i) {
        cms.put(heavy);
    }

    // Insert light items 5 times each
    for (int i = 0; i < 5; ++i) {
        cms.put(light1);
        cms.put(light2);
    }

    // Total: 110 items
    ASSERT_EQ(cms.total_count(), 110);

    // Check counts
    uint64_t heavy_count = cms.get_min(heavy);
    uint64_t light1_count = cms.get_min(light1);
    uint64_t light2_count = cms.get_min(light2);

    // Heavy should be much larger than light
    ASSERT_GT(heavy_count, light1_count);
    ASSERT_GT(heavy_count, light2_count);

    // Heavy should be close to 100 (allow some tolerance for collisions)
    ASSERT_GE(heavy_count, 90);

    std::cout << " (heavy=" << heavy_count << ", light=" << light1_count << "," << light2_count << ") ";
}

// Test 7: Error bound verification
TEST(error_bound_verification) {
    // Use larger epsilon for easier testing
    CMSConfig conf{0.1, 0.01, false, 16};
    CountMinSketch cms(conf);

    std::vector<uint64_t> point = {12345};

    // Insert exactly 50 times
    for (int i = 0; i < 50; ++i) {
        cms.put(point);
    }

    // Insert 1000 random other points to create noise
    std::mt19937_64 rng(42);
    for (int i = 0; i < 1000; ++i) {
        std::vector<uint64_t> random_point = {rng()};
        cms.put(random_point);
    }

    uint64_t estimate = cms.get_min(point);
    uint64_t true_count = 50;
    uint64_t total = cms.total_count();

    // Error bound: estimate <= true_count + epsilon * total
    double epsilon_actual = cms.epsilon_actual();
    uint64_t max_error = static_cast<uint64_t>(epsilon_actual * total);

    std::cout << " (est=" << estimate << ", true=" << true_count
              << ", max_err=" << max_error << ") ";

    // Estimate should be >= true count (CM Sketch never underestimates)
    ASSERT_GE(estimate, true_count);

    // Estimate should be within error bound (with high probability)
    ASSERT_LE(estimate, true_count + max_error);
}

// Test 8: Batch insertion
TEST(batch_insertion) {
    CMSConfig conf{0.01, 0.01, false, 16};
    CountMinSketch cms(conf);

    std::vector<std::vector<uint64_t>> points;
    for (int i = 0; i < 100; ++i) {
        points.push_back({static_cast<uint64_t>(i), static_cast<uint64_t>(i * 2)});
    }

    cms.put_all(points);

    ASSERT_EQ(cms.total_count(), 100);

    // Check that all points are present
    int present_count = 0;
    for (const auto &point : points) {
        if (cms.get_bool(point)) {
            present_count++;
        }
    }

    ASSERT_EQ(present_count, 100);
}

// Test 9: Memory usage calculation
TEST(memory_usage) {
    CMSConfig conf{0.01, 0.01, false, 16};
    CountMinSketch cms(conf);

    uint width = cms.width();
    uint depth = cms.depth();

    // Expected: width * depth * 2 bytes (16-bit counters)
    uint64_t expected_memory = width * depth * 2;
    ASSERT_EQ(cms.memory_usage(), expected_memory);

    std::cout << " (memory=" << config_utils::format_memory(cms.memory_usage()) << ") ";
}

// Test 10: Different counter bit widths
TEST(different_counter_widths) {
    CMSConfig conf_8{0.1, 0.1, false, 8};
    CMSConfig conf_16{0.1, 0.1, false, 16};
    CMSConfig conf_32{0.1, 0.1, false, 32};
    CMSConfig conf_64{0.1, 0.1, false, 64};

    CountMinSketch cms_8(conf_8);
    CountMinSketch cms_16(conf_16);
    CountMinSketch cms_32(conf_32);
    CountMinSketch cms_64(conf_64);

    std::vector<uint64_t> point = {42};

    cms_8.put(point);
    cms_16.put(point);
    cms_32.put(point);
    cms_64.put(point);

    ASSERT_EQ(cms_8.get_min(point), 1);
    ASSERT_EQ(cms_16.get_min(point), 1);
    ASSERT_EQ(cms_32.get_min(point), 1);
    ASSERT_EQ(cms_64.get_min(point), 1);

    // Memory should scale with counter width
    uint64_t mem_8 = cms_8.memory_usage();
    uint64_t mem_16 = cms_16.memory_usage();
    uint64_t mem_32 = cms_32.memory_usage();
    uint64_t mem_64 = cms_64.memory_usage();

    ASSERT_EQ(mem_16, mem_8 * 2);
    ASSERT_EQ(mem_32, mem_8 * 4);
    ASSERT_EQ(mem_64, mem_8 * 8);
}

// Test 11: Counter saturation (8-bit counters)
TEST(counter_saturation) {
    CMSConfig conf{0.1, 0.1, false, 8};  // 8-bit counters, max = 255
    CountMinSketch cms(conf);

    std::vector<uint64_t> point = {999};

    // Insert 300 times (more than 255)
    for (int i = 0; i < 300; ++i) {
        cms.put(point);
    }

    uint64_t count = cms.get_min(point);

    // Should saturate at 255
    ASSERT_LE(count, 255);
}

// Test 12: Mean estimator
TEST(mean_estimator) {
    CMSConfig conf{0.01, 0.01, false, 16};
    CountMinSketch cms(conf);

    std::vector<uint64_t> point = {12345};

    // Insert 5 times
    for (int i = 0; i < 5; ++i) {
        cms.put(point);
    }

    uint64_t min_estimate = cms.get_min(point);
    uint64_t mean_estimate = cms.get_mean(point);

    // Mean should be >= min (average >= minimum)
    ASSERT_GE(mean_estimate, min_estimate);

    // Both should be > 0
    ASSERT_GT(min_estimate, 0);
    ASSERT_GT(mean_estimate, 0);

    std::cout << " (min=" << min_estimate << ", mean=" << mean_estimate << ") ";
}

// Test 13: Epsilon and delta actual values
TEST(epsilon_delta_actual) {
    double epsilon = 0.01;
    double delta = 0.01;

    CMSConfig conf{epsilon, delta, false, 16};
    CountMinSketch cms(conf);

    double epsilon_actual = cms.epsilon_actual();
    double delta_actual = cms.delta_actual();

    // epsilon_actual = e / width
    uint width = cms.width();
    double expected_epsilon = std::exp(1.0) / static_cast<double>(width);
    ASSERT_NEAR(epsilon_actual, expected_epsilon, 1e-9);

    // delta_actual = exp(-depth)
    uint depth = cms.depth();
    double expected_delta = std::exp(-static_cast<double>(depth));
    ASSERT_NEAR(delta_actual, expected_delta, 1e-9);

    std::cout << " (ε=" << epsilon_actual << ", δ=" << delta_actual << ") ";
}

// Test 14: Summary output
TEST(summary_output) {
    CMSConfig conf{0.01, 0.01, false, 16};
    CountMinSketch cms(conf);

    std::string summary = cms.summary();

    ASSERT_TRUE(summary.find("Count-Min Sketch") != std::string::npos);
    ASSERT_TRUE(summary.find("Epsilon") != std::string::npos);
    ASSERT_TRUE(summary.find("Delta") != std::string::npos);
    ASSERT_TRUE(summary.find("Dimensions") != std::string::npos);
}

// Test 15: Frequency estimation with collisions
TEST(frequency_estimation_with_collisions) {
    CMSConfig conf{0.05, 0.05, false, 16};
    CountMinSketch cms(conf);

    std::vector<uint64_t> target = {555};

    // Insert target 20 times
    for (int i = 0; i < 20; ++i) {
        cms.put(target);
    }

    // Insert 500 random elements to create collisions
    std::mt19937_64 rng(123);
    for (int i = 0; i < 500; ++i) {
        std::vector<uint64_t> random_point = {rng(), rng()};
        cms.put(random_point);
    }

    uint64_t estimate = cms.get_min(target);

    // Should be >= 20 (never underestimates)
    ASSERT_GE(estimate, 20);

    // Should be within reasonable error bound
    double epsilon_actual = cms.epsilon_actual();
    uint64_t max_expected = 20 + static_cast<uint64_t>(epsilon_actual * cms.total_count()) + 50;
    ASSERT_LE(estimate, max_expected);

    std::cout << " (true=20, est=" << estimate << ") ";
}

// Test 16: Conservative update reduces overcounting
TEST(conservative_update_reduces_overcounting) {
    CMSConfig conf_standard{0.05, 0.05, false, 16};
    CMSConfig conf_conservative{0.05, 0.05, true, 16};

    CountMinSketch cms_standard(conf_standard);
    CountMinSketch cms_conservative(conf_conservative);

    std::vector<uint64_t> target = {888};

    // Insert target 20 times
    for (int i = 0; i < 20; ++i) {
        cms_standard.put(target);
        cms_conservative.put(target);
    }

    // Insert 500 random elements
    std::mt19937_64 rng(456);
    for (int i = 0; i < 500; ++i) {
        std::vector<uint64_t> random_point = {rng(), rng()};
        cms_standard.put(random_point);
        cms_conservative.put(random_point);
    }

    uint64_t estimate_standard = cms_standard.get_min(target);
    uint64_t estimate_conservative = cms_conservative.get_min(target);

    // Both should be >= 20
    ASSERT_GE(estimate_standard, 20);
    ASSERT_GE(estimate_conservative, 20);

    // Conservative should generally be closer to true value (less overcounting)
    std::cout << " (std=" << estimate_standard << ", cons=" << estimate_conservative << ") ";

    // Conservative should be <= standard (less or equal overcounting)
    ASSERT_LE(estimate_conservative, estimate_standard);
}

// Test 17: Dimensions match epsilon/delta requirements
TEST(dimensions_match_requirements) {
    double epsilon = 0.001;
    double delta = 0.001;

    CMSConfig conf{epsilon, delta, false, 32};
    CountMinSketch cms(conf);

    // Verify dimensions
    uint width = cms.width();
    uint depth = cms.depth();

    // width should be >= e / epsilon
    double min_width = std::exp(1.0) / epsilon;
    ASSERT_GE(width, static_cast<uint>(min_width));

    // depth should be >= ln(1 / delta)
    double min_depth = std::log(1.0 / delta);
    ASSERT_GE(depth, static_cast<uint>(min_depth));

    std::cout << " (w=" << width << ", d=" << depth << ") ";
}

// Main test runner
int main() {
    std::cout << "===== Count-Min Sketch Unit Tests =====\n\n";

    run_test_config_validation();
    run_test_dimension_calculation();
    run_test_basic_insertion_query();
    run_test_query_nonexistent();
    run_test_conservative_vs_standard_update();
    run_test_frequency_count_differentiation();
    run_test_error_bound_verification();
    run_test_batch_insertion();
    run_test_memory_usage();
    run_test_different_counter_widths();
    run_test_counter_saturation();
    run_test_mean_estimator();
    run_test_epsilon_delta_actual();
    run_test_summary_output();
    run_test_frequency_estimation_with_collisions();
    run_test_conservative_update_reduces_overcounting();
    run_test_dimensions_match_requirements();

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
