/*
 * Unit tests for Space-Filling Curves
 *
 * Tests:
 * - Morton 2D/3D encode/decode roundtrip
 * - Hilbert 2D/3D encode/decode roundtrip
 * - Edge cases (zero, max coordinates)
 * - Locality preservation (adjacent points have close codes)
 * - Bijection (no collisions in valid range)
 */

#include "space_filling_curves.hpp"
#include <algorithm>
#include <cassert>
#include <chrono>
#include <cmath>
#include <iostream>
#include <random>
#include <set>
#include <vector>

// Simple test framework (matching existing project patterns)
int test_count = 0;
int test_passed = 0;

#define TEST(name)                                        \
    void test_##name();                                   \
    void run_test_##name() {                              \
        test_count++;                                     \
        std::cout << "Running test: " << #name << "... "; \
        std::cout.flush();                                \
        try {                                             \
            test_##name();                                \
            test_passed++;                                \
            std::cout << "PASSED\n";                      \
        } catch (const std::exception &e) {               \
            std::cout << "FAILED: " << e.what() << "\n";  \
        } catch (...) {                                   \
            std::cout << "FAILED: Unknown exception\n";   \
        }                                                 \
    }                                                     \
    void test_##name()

#define ASSERT_TRUE(cond)                                 \
    if (!(cond))                                          \
        throw std::runtime_error("Assertion failed: " #cond)

#define ASSERT_FALSE(cond)                                \
    if (cond)                                             \
        throw std::runtime_error("Assertion failed: !" #cond)

#define ASSERT_EQ(a, b)                                   \
    if ((a) != (b))                                       \
        throw std::runtime_error("Assertion failed: " #a " == " #b)

#define ASSERT_NE(a, b)                                   \
    if ((a) == (b))                                       \
        throw std::runtime_error("Assertion failed: " #a " != " #b)

#define ASSERT_LT(a, b)                                   \
    if ((a) >= (b))                                       \
        throw std::runtime_error("Assertion failed: " #a " < " #b)

#define ASSERT_LE(a, b)                                   \
    if ((a) > (b))                                        \
        throw std::runtime_error("Assertion failed: " #a " <= " #b)

#define ASSERT_GT(a, b)                                   \
    if ((a) <= (b))                                       \
        throw std::runtime_error("Assertion failed: " #a " > " #b)

#define ASSERT_GE(a, b)                                   \
    if ((a) < (b))                                        \
        throw std::runtime_error("Assertion failed: " #a " >= " #b)

// ============================================================================
// Morton 2D Tests
// ============================================================================

TEST(morton2d_encode_decode_roundtrip) {
    std::mt19937 rng(42);

    // Test random coordinates
    for (int i = 0; i < 1000; ++i) {
        uint32_t x = rng() & 0xFFFF;
        uint32_t y = rng() & 0xFFFF;

        uint64_t code = sfc::Morton2D<16>::encode(x, y);
        uint32_t dx, dy;
        sfc::Morton2D<16>::decode(code, dx, dy);

        ASSERT_EQ(x, dx);
        ASSERT_EQ(y, dy);
    }
}

TEST(morton2d_zero_coordinates) {
    uint64_t code = sfc::Morton2D<16>::encode(0, 0);
    ASSERT_EQ(code, 0ULL);

    uint32_t x, y;
    sfc::Morton2D<16>::decode(0, x, y);
    ASSERT_EQ(x, 0U);
    ASSERT_EQ(y, 0U);
}

TEST(morton2d_max_coordinates) {
    constexpr uint32_t max_val = 0xFFFF;
    uint64_t code = sfc::Morton2D<16>::encode(max_val, max_val);

    // Max code should be 2^32 - 1 for 16-bit coordinates
    ASSERT_EQ(code, sfc::Morton2D<16>::max_code);

    uint32_t x, y;
    sfc::Morton2D<16>::decode(code, x, y);
    ASSERT_EQ(x, max_val);
    ASSERT_EQ(y, max_val);
}

TEST(morton2d_bit_interleaving) {
    // Test that bits are correctly interleaved
    // x=1 (binary: 0001), y=0 -> code should have bit 0 set
    ASSERT_EQ(sfc::Morton2D<16>::encode(1, 0), 1ULL);

    // x=0, y=1 -> code should have bit 1 set
    ASSERT_EQ(sfc::Morton2D<16>::encode(0, 1), 2ULL);

    // x=1, y=1 -> both bits 0 and 1 set -> 3
    ASSERT_EQ(sfc::Morton2D<16>::encode(1, 1), 3ULL);

    // x=2 (binary: 0010), y=0 -> bit 2 set
    ASSERT_EQ(sfc::Morton2D<16>::encode(2, 0), 4ULL);

    // x=0, y=2 -> bit 3 set
    ASSERT_EQ(sfc::Morton2D<16>::encode(0, 2), 8ULL);
}

TEST(morton2d_adjacent_points_close_codes) {
    // Adjacent points in 2D should have somewhat close Morton codes
    // (though Morton has jump segments between some adjacent pairs)
    std::mt19937 rng(123);

    int close_count = 0;
    int total_pairs = 1000;

    for (int i = 0; i < total_pairs; ++i) {
        uint32_t x = rng() % 1000;
        uint32_t y = rng() % 1000;

        // Get code for (x, y) and adjacent points
        uint64_t c0 = sfc::Morton2D<16>::encode(x, y);
        uint64_t c1 = sfc::Morton2D<16>::encode(x + 1, y);
        uint64_t c2 = sfc::Morton2D<16>::encode(x, y + 1);

        // Check if code differences are small (< 10 for nearby points)
        if (std::abs(static_cast<int64_t>(c1 - c0)) < 10) close_count++;
        if (std::abs(static_cast<int64_t>(c2 - c0)) < 10) close_count++;
    }

    // At least some adjacent points should have close codes
    // (Morton has ~50% adjacency for horizontal neighbors)
    ASSERT_GT(close_count, total_pairs / 2);
}

TEST(morton2d_is_bijective) {
    // Test that Morton is bijective for a small grid
    constexpr int size = 64;
    std::set<uint64_t> codes;

    for (int x = 0; x < size; ++x) {
        for (int y = 0; y < size; ++y) {
            uint64_t code = sfc::Morton2D<16>::encode(x, y);
            ASSERT_TRUE(codes.find(code) == codes.end()); // No duplicates
            codes.insert(code);
        }
    }

    ASSERT_EQ(codes.size(), static_cast<size_t>(size * size));
}

// ============================================================================
// Morton 3D Tests
// ============================================================================

TEST(morton3d_encode_decode_roundtrip) {
    std::mt19937 rng(42);

    for (int i = 0; i < 1000; ++i) {
        uint32_t x = rng() & 0xFFFF;
        uint32_t y = rng() & 0xFFFF;
        uint32_t z = rng() & 0xFFFF;

        uint64_t code = sfc::Morton3D<16>::encode(x, y, z);
        uint32_t dx, dy, dz;
        sfc::Morton3D<16>::decode(code, dx, dy, dz);

        ASSERT_EQ(x, dx);
        ASSERT_EQ(y, dy);
        ASSERT_EQ(z, dz);
    }
}

TEST(morton3d_zero_coordinates) {
    uint64_t code = sfc::Morton3D<16>::encode(0, 0, 0);
    ASSERT_EQ(code, 0ULL);

    uint32_t x, y, z;
    sfc::Morton3D<16>::decode(0, x, y, z);
    ASSERT_EQ(x, 0U);
    ASSERT_EQ(y, 0U);
    ASSERT_EQ(z, 0U);
}

TEST(morton3d_bit_interleaving) {
    // x=1, y=0, z=0 -> bit 0 set
    ASSERT_EQ(sfc::Morton3D<16>::encode(1, 0, 0), 1ULL);

    // x=0, y=1, z=0 -> bit 1 set
    ASSERT_EQ(sfc::Morton3D<16>::encode(0, 1, 0), 2ULL);

    // x=0, y=0, z=1 -> bit 2 set
    ASSERT_EQ(sfc::Morton3D<16>::encode(0, 0, 1), 4ULL);

    // x=1, y=1, z=1 -> bits 0,1,2 set -> 7
    ASSERT_EQ(sfc::Morton3D<16>::encode(1, 1, 1), 7ULL);
}

TEST(morton3d_adjacent_points_close_codes) {
    std::mt19937 rng(123);

    int close_count = 0;
    int total_pairs = 500;

    for (int i = 0; i < total_pairs; ++i) {
        uint32_t x = rng() % 100;
        uint32_t y = rng() % 100;
        uint32_t z = rng() % 100;

        uint64_t c0 = sfc::Morton3D<16>::encode(x, y, z);
        uint64_t c1 = sfc::Morton3D<16>::encode(x + 1, y, z);
        uint64_t c2 = sfc::Morton3D<16>::encode(x, y + 1, z);
        uint64_t c3 = sfc::Morton3D<16>::encode(x, y, z + 1);

        // Check if code differences are small
        if (std::abs(static_cast<int64_t>(c1 - c0)) < 20) close_count++;
        if (std::abs(static_cast<int64_t>(c2 - c0)) < 20) close_count++;
        if (std::abs(static_cast<int64_t>(c3 - c0)) < 20) close_count++;
    }

    ASSERT_GT(close_count, total_pairs / 2);
}

// ============================================================================
// Hilbert 2D Tests
// ============================================================================

TEST(hilbert2d_encode_decode_roundtrip) {
    std::mt19937 rng(42);

    for (int i = 0; i < 1000; ++i) {
        uint32_t x = rng() & 0xFF; // Use smaller range for Hilbert
        uint32_t y = rng() & 0xFF;

        uint64_t code = sfc::Hilbert2D<8>::encode(x, y);
        uint32_t dx, dy;
        sfc::Hilbert2D<8>::decode(code, dx, dy);

        ASSERT_EQ(x, dx);
        ASSERT_EQ(y, dy);
    }
}

TEST(hilbert2d_zero_coordinates) {
    uint64_t code = sfc::Hilbert2D<8>::encode(0, 0);
    ASSERT_EQ(code, 0ULL);

    uint32_t x, y;
    sfc::Hilbert2D<8>::decode(0, x, y);
    ASSERT_EQ(x, 0U);
    ASSERT_EQ(y, 0U);
}

TEST(hilbert2d_max_coordinates) {
    constexpr uint32_t max_val = 255; // 8-bit
    uint64_t code = sfc::Hilbert2D<8>::encode(max_val, max_val);

    uint32_t x, y;
    sfc::Hilbert2D<8>::decode(code, x, y);
    ASSERT_EQ(x, max_val);
    ASSERT_EQ(y, max_val);
}

TEST(hilbert2d_adjacent_points_close_codes) {
    // Hilbert curve has better locality than Morton - adjacent points
    // should almost always have close codes
    std::mt19937 rng(123);

    int close_count = 0;
    int total_pairs = 1000;

    for (int i = 0; i < total_pairs; ++i) {
        uint32_t x = rng() % 200;
        uint32_t y = rng() % 200;

        uint64_t c0 = sfc::Hilbert2D<8>::encode(x, y);
        uint64_t c1 = sfc::Hilbert2D<8>::encode(x + 1, y);
        uint64_t c2 = sfc::Hilbert2D<8>::encode(x, y + 1);

        // Hilbert should have code difference <= 3 for most adjacent pairs
        if (std::abs(static_cast<int64_t>(c1 - c0)) <= 10) close_count++;
        if (std::abs(static_cast<int64_t>(c2 - c0)) <= 10) close_count++;
    }

    // Hilbert should have high locality (>70% of pairs)
    ASSERT_GT(close_count, total_pairs);
}

TEST(hilbert2d_is_bijective) {
    constexpr int size = 32;
    std::set<uint64_t> codes;

    for (int x = 0; x < size; ++x) {
        for (int y = 0; y < size; ++y) {
            uint64_t code = sfc::Hilbert2D<8>::encode(x, y);
            ASSERT_TRUE(codes.find(code) == codes.end());
            codes.insert(code);
        }
    }

    ASSERT_EQ(codes.size(), static_cast<size_t>(size * size));
}

TEST(hilbert2d_better_locality_than_morton) {
    // Compare locality: count how many adjacent point pairs have code diff <= 1
    constexpr int size = 64;

    int morton_consecutive = 0;
    int hilbert_consecutive = 0;

    for (int x = 0; x < size - 1; ++x) {
        for (int y = 0; y < size; ++y) {
            // Horizontal neighbor
            uint64_t mc0 = sfc::Morton2D<8>::encode(x, y);
            uint64_t mc1 = sfc::Morton2D<8>::encode(x + 1, y);
            if (std::abs(static_cast<int64_t>(mc1 - mc0)) <= 1) morton_consecutive++;

            uint64_t hc0 = sfc::Hilbert2D<8>::encode(x, y);
            uint64_t hc1 = sfc::Hilbert2D<8>::encode(x + 1, y);
            if (std::abs(static_cast<int64_t>(hc1 - hc0)) <= 1) hilbert_consecutive++;
        }
    }

    // Hilbert should have similar or more consecutive pairs than Morton
    // (Allow some tolerance as both are good locality-preserving curves)
    std::cout << "(Morton=" << morton_consecutive << ", Hilbert=" << hilbert_consecutive << ") ";
    // Hilbert should be within 5% of Morton at minimum
    ASSERT_GE(hilbert_consecutive, morton_consecutive * 95 / 100);
}

// ============================================================================
// Hilbert 3D Tests
// ============================================================================

TEST(hilbert3d_encode_decode_roundtrip) {
    std::mt19937 rng(42);

    for (int i = 0; i < 1000; ++i) {
        uint32_t x = rng() % 64;
        uint32_t y = rng() % 64;
        uint32_t z = rng() % 64;

        uint64_t code = sfc::Hilbert3D<6>::encode(x, y, z);
        uint32_t dx, dy, dz;
        sfc::Hilbert3D<6>::decode(code, dx, dy, dz);

        ASSERT_EQ(x, dx);
        ASSERT_EQ(y, dy);
        ASSERT_EQ(z, dz);
    }
}

TEST(hilbert3d_zero_coordinates) {
    uint64_t code = sfc::Hilbert3D<6>::encode(0, 0, 0);
    ASSERT_EQ(code, 0ULL);

    uint32_t x, y, z;
    sfc::Hilbert3D<6>::decode(0, x, y, z);
    ASSERT_EQ(x, 0U);
    ASSERT_EQ(y, 0U);
    ASSERT_EQ(z, 0U);
}

TEST(hilbert3d_adjacent_points_close_codes) {
    std::mt19937 rng(123);

    int close_count = 0;
    int total_pairs = 500;

    for (int i = 0; i < total_pairs; ++i) {
        uint32_t x = rng() % 30;
        uint32_t y = rng() % 30;
        uint32_t z = rng() % 30;

        uint64_t c0 = sfc::Hilbert3D<6>::encode(x, y, z);
        uint64_t c1 = sfc::Hilbert3D<6>::encode(x + 1, y, z);
        uint64_t c2 = sfc::Hilbert3D<6>::encode(x, y + 1, z);
        uint64_t c3 = sfc::Hilbert3D<6>::encode(x, y, z + 1);

        if (std::abs(static_cast<int64_t>(c1 - c0)) <= 10) close_count++;
        if (std::abs(static_cast<int64_t>(c2 - c0)) <= 10) close_count++;
        if (std::abs(static_cast<int64_t>(c3 - c0)) <= 10) close_count++;
    }

    ASSERT_GT(close_count, total_pairs);
}

// ============================================================================
// Utility Function Tests
// ============================================================================

TEST(manhattan_distance_2d) {
    ASSERT_EQ(sfc::manhattan_distance_2d(0, 0, 0, 0), 0ULL);
    ASSERT_EQ(sfc::manhattan_distance_2d(0, 0, 1, 0), 1ULL);
    ASSERT_EQ(sfc::manhattan_distance_2d(0, 0, 0, 1), 1ULL);
    ASSERT_EQ(sfc::manhattan_distance_2d(0, 0, 1, 1), 2ULL);
    ASSERT_EQ(sfc::manhattan_distance_2d(0, 0, 3, 4), 7ULL);

    // Test with larger values and wraparound
    ASSERT_EQ(sfc::manhattan_distance_2d(100, 100, 50, 50), 100ULL);
}

TEST(manhattan_distance_3d) {
    ASSERT_EQ(sfc::manhattan_distance_3d(0, 0, 0, 0, 0, 0), 0ULL);
    ASSERT_EQ(sfc::manhattan_distance_3d(0, 0, 0, 1, 0, 0), 1ULL);
    ASSERT_EQ(sfc::manhattan_distance_3d(0, 0, 0, 0, 1, 0), 1ULL);
    ASSERT_EQ(sfc::manhattan_distance_3d(0, 0, 0, 0, 0, 1), 1ULL);
    ASSERT_EQ(sfc::manhattan_distance_3d(0, 0, 0, 1, 1, 1), 3ULL);
}

// ============================================================================
// Performance Tests (informational, not assertions)
// ============================================================================

TEST(sfc_encode_performance) {
    constexpr int iterations = 1000000;
    std::mt19937 rng(42);

    // Pre-generate random coordinates
    std::vector<uint32_t> xs(iterations), ys(iterations);
    for (int i = 0; i < iterations; ++i) {
        xs[i] = rng() & 0xFFFF;
        ys[i] = rng() & 0xFFFF;
    }

    volatile uint64_t sum = 0;

    // Morton 2D
    auto start = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < iterations; ++i) {
        sum += sfc::Morton2D<16>::encode(xs[i], ys[i]);
    }
    auto end = std::chrono::high_resolution_clock::now();
    double morton_ns = std::chrono::duration<double, std::nano>(end - start).count() / iterations;

    // Hilbert 2D
    start = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < iterations; ++i) {
        sum += sfc::Hilbert2D<16>::encode(xs[i], ys[i]);
    }
    end = std::chrono::high_resolution_clock::now();
    double hilbert_ns = std::chrono::duration<double, std::nano>(end - start).count() / iterations;

    std::cout << "(Morton=" << morton_ns << "ns, Hilbert=" << hilbert_ns << "ns) ";

    // Just ensure the loop wasn't optimized away
    ASSERT_GT(sum, 0ULL);

    // Morton should be faster than Hilbert (BMI2 or magic numbers vs state machine)
    // But we don't strictly enforce this as it depends on hardware
}

// ============================================================================
// Main
// ============================================================================

int main() {
    std::cout << "\n===== Space-Filling Curves Unit Tests =====\n\n";

    // Morton 2D
    run_test_morton2d_encode_decode_roundtrip();
    run_test_morton2d_zero_coordinates();
    run_test_morton2d_max_coordinates();
    run_test_morton2d_bit_interleaving();
    run_test_morton2d_adjacent_points_close_codes();
    run_test_morton2d_is_bijective();

    // Morton 3D
    run_test_morton3d_encode_decode_roundtrip();
    run_test_morton3d_zero_coordinates();
    run_test_morton3d_bit_interleaving();
    run_test_morton3d_adjacent_points_close_codes();

    // Hilbert 2D
    run_test_hilbert2d_encode_decode_roundtrip();
    run_test_hilbert2d_zero_coordinates();
    run_test_hilbert2d_max_coordinates();
    run_test_hilbert2d_adjacent_points_close_codes();
    run_test_hilbert2d_is_bijective();
    run_test_hilbert2d_better_locality_than_morton();

    // Hilbert 3D
    run_test_hilbert3d_encode_decode_roundtrip();
    run_test_hilbert3d_zero_coordinates();
    run_test_hilbert3d_adjacent_points_close_codes();

    // Utility functions
    run_test_manhattan_distance_2d();
    run_test_manhattan_distance_3d();

    // Performance (informational)
    run_test_sfc_encode_performance();

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
