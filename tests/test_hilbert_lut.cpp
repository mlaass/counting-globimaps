/**
 * Hilbert LUT Verification and Correctness Tests
 *
 * Tests that the LUT-based Hilbert2D::encode() produces identical results
 * to the reference bit-by-bit implementation for all coordinate combinations.
 */

#include <iostream>
#include <iomanip>
#include <chrono>
#include <vector>
#include <random>
#include "../include/space_filling_curves.hpp"

using namespace sfc;

// Simple test framework
static int tests_run = 0;
static int tests_passed = 0;

#define TEST(name) void name(); \
    static struct name##_register { \
        name##_register() { std::cout << "  " << #name << "... "; tests_run++; name(); } \
    } name##_instance; \
    void name()

#define ASSERT(cond) do { \
    if (!(cond)) { \
        std::cout << "FAIL\n    Assertion failed: " #cond << std::endl; \
        return; \
    } \
} while(0)

#define ASSERT_EQ(a, b) do { \
    if ((a) != (b)) { \
        std::cout << "FAIL\n    Expected: " << (b) << ", Got: " << (a) << std::endl; \
        return; \
    } \
} while(0)

#define PASS() do { tests_passed++; std::cout << "OK" << std::endl; } while(0)

// ============================================================================
// Correctness Tests
// ============================================================================

TEST(test_lut_matches_reference_small_grid) {
    // Test all 256x256 combinations (65,536 points)
    // This is a comprehensive test for small coordinates
    int mismatches = 0;
    for (uint32_t x = 0; x < 256; ++x) {
        for (uint32_t y = 0; y < 256; ++y) {
            uint64_t lut_result = Hilbert2D<16>::encode(x, y);
            uint64_t ref_result = Hilbert2D<16>::encode_reference(x, y);
            if (lut_result != ref_result) {
                if (mismatches < 5) {
                    std::cout << "\n    Mismatch at (" << x << ", " << y << "): "
                              << "LUT=" << lut_result << ", REF=" << ref_result;
                }
                mismatches++;
            }
        }
    }
    if (mismatches > 0) {
        std::cout << "\n    Total mismatches: " << mismatches << " / 65536" << std::endl;
    }
    ASSERT_EQ(mismatches, 0);
    PASS();
}

TEST(test_lut_full_16bit_random_sample) {
    // Random sample of 100,000 points across full 16-bit range
    std::mt19937 gen(42);  // Fixed seed for reproducibility
    std::uniform_int_distribution<uint32_t> dist(0, 65535);

    int mismatches = 0;
    for (int i = 0; i < 100000; ++i) {
        uint32_t x = dist(gen);
        uint32_t y = dist(gen);
        uint64_t lut_result = Hilbert2D<16>::encode(x, y);
        uint64_t ref_result = Hilbert2D<16>::encode_reference(x, y);
        if (lut_result != ref_result) {
            if (mismatches < 5) {
                std::cout << "\n    Mismatch at (" << x << ", " << y << "): "
                          << "LUT=" << lut_result << ", REF=" << ref_result;
            }
            mismatches++;
        }
    }
    if (mismatches > 0) {
        std::cout << "\n    Total mismatches: " << mismatches << " / 100000" << std::endl;
    }
    ASSERT_EQ(mismatches, 0);
    PASS();
}

TEST(test_lut_corner_cases) {
    // Test corner and edge cases
    auto check = [](uint32_t x, uint32_t y) {
        uint64_t lut = Hilbert2D<16>::encode(x, y);
        uint64_t ref = Hilbert2D<16>::encode_reference(x, y);
        return lut == ref;
    };

    // Corners
    ASSERT(check(0, 0));
    ASSERT(check(0, 65535));
    ASSERT(check(65535, 0));
    ASSERT(check(65535, 65535));

    // Powers of 2
    for (int p = 0; p < 16; ++p) {
        uint32_t v = 1U << p;
        ASSERT(check(v, v));
        ASSERT(check(v, 0));
        ASSERT(check(0, v));
        ASSERT(check(v - 1, v - 1));
    }

    // Diagonal
    for (uint32_t d = 0; d < 1000; ++d) {
        ASSERT(check(d, d));
        ASSERT(check(d, 65535 - d));
    }

    PASS();
}

TEST(test_lut_8bit_exhaustive) {
    // Exhaustive test for 8-bit coordinates (256x256 = 65,536 combinations)
    int mismatches = 0;
    for (uint32_t x = 0; x < 256; ++x) {
        for (uint32_t y = 0; y < 256; ++y) {
            uint64_t lut = Hilbert2D<8>::encode(x, y);
            uint64_t ref = Hilbert2D<8>::encode_reference(x, y);
            if (lut != ref) mismatches++;
        }
    }
    ASSERT_EQ(mismatches, 0);
    PASS();
}

TEST(test_lut_12bit_sample) {
    // Sample test for 12-bit coordinates
    std::mt19937 gen(12345);
    std::uniform_int_distribution<uint32_t> dist(0, 4095);

    int mismatches = 0;
    for (int i = 0; i < 50000; ++i) {
        uint32_t x = dist(gen);
        uint32_t y = dist(gen);
        uint64_t lut = Hilbert2D<12>::encode(x, y);
        uint64_t ref = Hilbert2D<12>::encode_reference(x, y);
        if (lut != ref) mismatches++;
    }
    ASSERT_EQ(mismatches, 0);
    PASS();
}

TEST(test_ordering_preserved) {
    // Verify that the curve order is preserved
    // Adjacent points on the Hilbert curve should be spatially close
    uint32_t prev_x = 0, prev_y = 0;
    Hilbert2D<8>::decode(0, prev_x, prev_y);

    for (uint64_t code = 1; code < 256; ++code) {
        uint32_t x, y;
        Hilbert2D<8>::decode(code, x, y);

        // Adjacent Hilbert codes should have Manhattan distance <= 1
        int64_t dx = static_cast<int64_t>(x) - prev_x;
        int64_t dy = static_cast<int64_t>(y) - prev_y;
        int64_t dist = (dx < 0 ? -dx : dx) + (dy < 0 ? -dy : dy);
        ASSERT(dist == 1);

        prev_x = x;
        prev_y = y;
    }
    PASS();
}

TEST(test_encode_decode_roundtrip) {
    // Test encode -> decode roundtrip
    std::mt19937 gen(999);
    std::uniform_int_distribution<uint32_t> dist(0, 255);

    for (int i = 0; i < 10000; ++i) {
        uint32_t x = dist(gen);
        uint32_t y = dist(gen);
        uint64_t code = Hilbert2D<8>::encode(x, y);
        uint32_t dx, dy;
        Hilbert2D<8>::decode(code, dx, dy);
        ASSERT(x == dx && y == dy);
    }
    PASS();
}

// ============================================================================
// Performance Tests
// ============================================================================

TEST(test_performance_comparison) {
    // Compare LUT vs reference performance
    const int N = 1000000;
    std::vector<uint32_t> xs(N), ys(N);

    std::mt19937 gen(42);
    std::uniform_int_distribution<uint32_t> dist(0, 65535);
    for (int i = 0; i < N; ++i) {
        xs[i] = dist(gen);
        ys[i] = dist(gen);
    }

    // Warm up caches
    volatile uint64_t dummy = 0;
    for (int i = 0; i < 1000; ++i) {
        dummy += Hilbert2D<16>::encode(xs[i], ys[i]);
        dummy += Hilbert2D<16>::encode_reference(xs[i], ys[i]);
    }

    // Benchmark LUT version
    auto start_lut = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < N; ++i) {
        dummy += Hilbert2D<16>::encode(xs[i], ys[i]);
    }
    auto end_lut = std::chrono::high_resolution_clock::now();
    double lut_ns = std::chrono::duration<double, std::nano>(end_lut - start_lut).count() / N;

    // Benchmark reference version
    auto start_ref = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < N; ++i) {
        dummy += Hilbert2D<16>::encode_reference(xs[i], ys[i]);
    }
    auto end_ref = std::chrono::high_resolution_clock::now();
    double ref_ns = std::chrono::duration<double, std::nano>(end_ref - start_ref).count() / N;

    std::cout << "OK" << std::endl;
    std::cout << "\n    Performance Results:" << std::endl;
    std::cout << "    LUT encode:       " << std::fixed << std::setprecision(2) << lut_ns << " ns/op" << std::endl;
    std::cout << "    Reference encode: " << std::fixed << std::setprecision(2) << ref_ns << " ns/op" << std::endl;
    std::cout << "    Speedup:          " << std::fixed << std::setprecision(1) << (ref_ns / lut_ns) << "x" << std::endl;

    tests_passed++;
}

// ============================================================================
// Main
// ============================================================================

int main() {
    std::cout << "\n=== Hilbert LUT Verification Tests ===" << std::endl;
    std::cout << "\nRunning tests:" << std::endl;

    // Tests are auto-registered and run during static initialization
    // Just print summary here

    std::cout << "\n=== Summary ===" << std::endl;
    std::cout << "Passed: " << tests_passed << " / " << tests_run << std::endl;

    return (tests_passed == tests_run) ? 0 : 1;
}
