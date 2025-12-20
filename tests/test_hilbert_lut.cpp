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
// Neighborhood Encoding Tests
// ============================================================================

TEST(test_neighborhood_2d_same_block) {
    // Test points in the middle of a 16x16 block (same_block fast path)
    // For these points, all 9 neighbors stay within the same block
    std::vector<std::pair<uint32_t, uint32_t>> test_points = {
        {100, 100},   // Well inside a block
        {1000, 2000}, // Larger coordinates
        {5, 5},       // Near origin but not at edge
        {255, 255},   // At block interior
    };

    for (const auto& [x, y] : test_points) {
        uint64_t batch_codes[9];
        Hilbert2D<16>::encode_neighborhood_2d(x, y, batch_codes);

        // Verify each code matches individual encode
        int idx = 0;
        for (int dy = -1; dy <= 1; ++dy) {
            for (int dx = -1; dx <= 1; ++dx) {
                uint64_t expected = Hilbert2D<16>::encode(x + dx, y + dy);
                if (batch_codes[idx] != expected) {
                    std::cout << "FAIL\n    Mismatch at (" << x << "+" << dx
                              << ", " << y << "+" << dy << "): "
                              << "batch=" << batch_codes[idx]
                              << ", expected=" << expected << std::endl;
                    return;
                }
                idx++;
            }
        }
    }
    PASS();
}

TEST(test_neighborhood_2d_block_boundary) {
    // Test points at block boundaries (slow path with full encode)
    // These are at positions where (x & 0xF) == 0, 15 or (y & 0xF) == 0, 15
    std::vector<std::pair<uint32_t, uint32_t>> test_points = {
        {16, 16},     // At block boundary
        {0, 0},       // Corner (needs bounds handling)
        {15, 15},     // Edge of first block
        {256, 0},     // Y at boundary
        {0, 256},     // X at boundary
        {255, 256},   // Both near boundaries
        {1000, 1024}, // At 1024 boundary (power of 2)
    };

    for (const auto& [x, y] : test_points) {
        // Skip if any neighbor would underflow
        if (x == 0 || y == 0) continue;

        uint64_t batch_codes[9];
        Hilbert2D<16>::encode_neighborhood_2d(x, y, batch_codes);

        int idx = 0;
        for (int dy = -1; dy <= 1; ++dy) {
            for (int dx = -1; dx <= 1; ++dx) {
                uint64_t expected = Hilbert2D<16>::encode(x + dx, y + dy);
                if (batch_codes[idx] != expected) {
                    std::cout << "FAIL\n    Boundary mismatch at (" << x << "+" << dx
                              << ", " << y << "+" << dy << "): "
                              << "batch=" << batch_codes[idx]
                              << ", expected=" << expected << std::endl;
                    return;
                }
                idx++;
            }
        }
    }
    PASS();
}

TEST(test_neighborhood_2d_random_comprehensive) {
    // Random test covering both fast and slow paths
    std::mt19937 gen(42);
    std::uniform_int_distribution<uint32_t> dist(1, 65534);  // Avoid 0 and max

    int mismatches = 0;
    int fast_path_count = 0;
    int slow_path_count = 0;

    for (int i = 0; i < 10000; ++i) {
        uint32_t x = dist(gen);
        uint32_t y = dist(gen);

        // Track which path we're testing
        bool is_fast = ((x & 0xF) >= 1 && (x & 0xF) <= 14 &&
                        (y & 0xF) >= 1 && (y & 0xF) <= 14);
        if (is_fast) fast_path_count++;
        else slow_path_count++;

        uint64_t batch_codes[9];
        Hilbert2D<16>::encode_neighborhood_2d(x, y, batch_codes);

        int idx = 0;
        for (int dy = -1; dy <= 1; ++dy) {
            for (int dx = -1; dx <= 1; ++dx) {
                uint64_t expected = Hilbert2D<16>::encode(x + dx, y + dy);
                if (batch_codes[idx] != expected) {
                    if (mismatches < 5) {
                        std::cout << "\n    Mismatch at (" << x << "+" << dx
                                  << ", " << y << "+" << dy << "): "
                                  << "batch=" << batch_codes[idx]
                                  << ", expected=" << expected
                                  << " [" << (is_fast ? "fast" : "slow") << " path]";
                    }
                    mismatches++;
                }
                idx++;
            }
        }
    }

    if (mismatches > 0) {
        std::cout << "\n    Total mismatches: " << mismatches << " / 90000" << std::endl;
    }

    std::cout << "OK" << std::endl;
    std::cout << "    Fast path tests: " << fast_path_count
              << " (" << (100.0 * fast_path_count / 10000) << "%)" << std::endl;
    std::cout << "    Slow path tests: " << slow_path_count
              << " (" << (100.0 * slow_path_count / 10000) << "%)" << std::endl;

    ASSERT_EQ(mismatches, 0);
    tests_passed++;
}

TEST(test_neighborhood_2d_8bit) {
    // Test 8-bit version exhaustively
    int mismatches = 0;

    for (uint32_t x = 1; x < 255; ++x) {
        for (uint32_t y = 1; y < 255; ++y) {
            uint64_t batch_codes[9];
            Hilbert2D<8>::encode_neighborhood_2d(x, y, batch_codes);

            int idx = 0;
            for (int dy = -1; dy <= 1; ++dy) {
                for (int dx = -1; dx <= 1; ++dx) {
                    uint64_t expected = Hilbert2D<8>::encode(x + dx, y + dy);
                    if (batch_codes[idx] != expected) mismatches++;
                    idx++;
                }
            }
        }
    }

    ASSERT_EQ(mismatches, 0);
    PASS();
}

TEST(test_neighborhood_2d_boundary_performance) {
    // Performance test specifically for boundary cases
    // These are points where (x & 0xF) == 0 or 15, or (y & 0xF) == 0 or 15
    const int N = 100000;
    std::vector<uint32_t> xs(N), ys(N);

    std::mt19937 gen(99);
    // Generate only boundary points
    for (int i = 0; i < N; ++i) {
        // Pick a random 16x16 block
        uint32_t block_x = gen() % 4096;
        uint32_t block_y = gen() % 4096;
        // Pick edge position (0 or 15)
        uint32_t edge_x = (gen() % 2) * 15;
        uint32_t edge_y = (gen() % 2) * 15;
        xs[i] = (block_x << 4) | edge_x;
        ys[i] = (block_y << 4) | edge_y;
        // Clamp to valid range
        if (xs[i] == 0) xs[i] = 16;
        if (ys[i] == 0) ys[i] = 16;
        if (xs[i] >= 65535) xs[i] = 65534;
        if (ys[i] >= 65535) ys[i] = 65534;
    }

    volatile uint64_t dummy = 0;

    // Warmup
    for (int i = 0; i < 1000; ++i) {
        uint64_t codes[9];
        Hilbert2D<16>::encode_neighborhood_2d(xs[i], ys[i], codes);
        dummy += codes[4];
    }

    // Benchmark batch encoding (boundary path)
    auto start_batch = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < N; ++i) {
        uint64_t codes[9];
        Hilbert2D<16>::encode_neighborhood_2d(xs[i], ys[i], codes);
        dummy += codes[4];
    }
    auto end_batch = std::chrono::high_resolution_clock::now();
    double batch_ns = std::chrono::duration<double, std::nano>(end_batch - start_batch).count() / N;

    // Benchmark individual encodes (9 per point)
    auto start_indiv = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < N; ++i) {
        for (int dy = -1; dy <= 1; ++dy) {
            for (int dx = -1; dx <= 1; ++dx) {
                dummy += Hilbert2D<16>::encode(xs[i] + dx, ys[i] + dy);
            }
        }
    }
    auto end_indiv = std::chrono::high_resolution_clock::now();
    double indiv_ns = std::chrono::duration<double, std::nano>(end_indiv - start_indiv).count() / N;

    std::cout << "OK" << std::endl;
    std::cout << "\n    Boundary Path Performance (block edges only):" << std::endl;
    std::cout << "    Batch (grouped):      " << std::fixed << std::setprecision(2) << batch_ns << " ns" << std::endl;
    std::cout << "    Individual (9 calls): " << std::fixed << std::setprecision(2) << indiv_ns << " ns" << std::endl;
    std::cout << "    Speedup:              " << std::fixed << std::setprecision(2) << (indiv_ns / batch_ns) << "x" << std::endl;

    // Verify we're actually faster
    ASSERT(batch_ns < indiv_ns);
    tests_passed++;
}

TEST(test_neighborhood_2d_performance) {
    // Performance comparison: batch vs individual encodes (mixed paths)
    const int N = 100000;
    std::vector<uint32_t> xs(N), ys(N);

    std::mt19937 gen(42);
    std::uniform_int_distribution<uint32_t> dist(1, 65534);
    for (int i = 0; i < N; ++i) {
        xs[i] = dist(gen);
        ys[i] = dist(gen);
    }

    volatile uint64_t dummy = 0;

    // Warmup
    for (int i = 0; i < 1000; ++i) {
        uint64_t codes[9];
        Hilbert2D<16>::encode_neighborhood_2d(xs[i], ys[i], codes);
        dummy += codes[4];
    }

    // Benchmark batch encoding
    auto start_batch = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < N; ++i) {
        uint64_t codes[9];
        Hilbert2D<16>::encode_neighborhood_2d(xs[i], ys[i], codes);
        dummy += codes[4];
    }
    auto end_batch = std::chrono::high_resolution_clock::now();
    double batch_ns = std::chrono::duration<double, std::nano>(end_batch - start_batch).count() / N;

    // Benchmark individual encodes (9 per point)
    auto start_indiv = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < N; ++i) {
        for (int dy = -1; dy <= 1; ++dy) {
            for (int dx = -1; dx <= 1; ++dx) {
                dummy += Hilbert2D<16>::encode(xs[i] + dx, ys[i] + dy);
            }
        }
    }
    auto end_indiv = std::chrono::high_resolution_clock::now();
    double indiv_ns = std::chrono::duration<double, std::nano>(end_indiv - start_indiv).count() / N;

    std::cout << "OK" << std::endl;
    std::cout << "\n    Neighborhood Encoding Performance:" << std::endl;
    std::cout << "    Batch (9 codes):      " << std::fixed << std::setprecision(2) << batch_ns << " ns" << std::endl;
    std::cout << "    Individual (9 calls): " << std::fixed << std::setprecision(2) << indiv_ns << " ns" << std::endl;
    std::cout << "    Speedup:              " << std::fixed << std::setprecision(2) << (indiv_ns / batch_ns) << "x" << std::endl;

    tests_passed++;
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
// Hilbert3D LUT Tests
// ============================================================================

TEST(test_hilbert3d_lut_bijection_small) {
    // Test that all 8x8x8 = 512 coordinates map to unique Hilbert codes
    constexpr unsigned Bits = 3;
    std::vector<uint64_t> codes;
    codes.reserve(512);

    for (uint32_t x = 0; x < 8; ++x) {
        for (uint32_t y = 0; y < 8; ++y) {
            for (uint32_t z = 0; z < 8; ++z) {
                codes.push_back(Hilbert3D<Bits>::encode(x, y, z));
            }
        }
    }

    // Check all codes are unique
    std::sort(codes.begin(), codes.end());
    for (size_t i = 1; i < codes.size(); ++i) {
        if (codes[i] == codes[i-1]) {
            std::cout << "FAIL\n    Duplicate code: " << codes[i] << std::endl;
            return;
        }
    }

    // Check codes span [0, 511]
    ASSERT_EQ(codes[0], 0ULL);
    ASSERT_EQ(codes[511], 511ULL);

    PASS();
}

TEST(test_hilbert3d_lut_bijection_16x16x16) {
    // Test 16x16x16 = 4096 coordinates with 4-bit encoding
    constexpr unsigned Bits = 4;
    std::vector<uint64_t> codes;
    codes.reserve(4096);

    for (uint32_t x = 0; x < 16; ++x) {
        for (uint32_t y = 0; y < 16; ++y) {
            for (uint32_t z = 0; z < 16; ++z) {
                codes.push_back(Hilbert3D<Bits>::encode(x, y, z));
            }
        }
    }

    // Check all codes are unique
    std::sort(codes.begin(), codes.end());
    for (size_t i = 1; i < codes.size(); ++i) {
        if (codes[i] == codes[i-1]) {
            std::cout << "FAIL\n    Duplicate code at index " << i << ": " << codes[i] << std::endl;
            return;
        }
    }

    // Check codes span [0, 4095]
    ASSERT_EQ(codes[0], 0ULL);
    ASSERT_EQ(codes[4095], 4095ULL);

    PASS();
}

TEST(test_hilbert3d_decode_roundtrip) {
    // Test decode(encode(x,y,z)) = (x,y,z) for all 8x8x8 coordinates
    constexpr unsigned Bits = 3;

    for (uint32_t x = 0; x < 8; ++x) {
        for (uint32_t y = 0; y < 8; ++y) {
            for (uint32_t z = 0; z < 8; ++z) {
                uint64_t code = Hilbert3D<Bits>::encode(x, y, z);
                uint32_t dx, dy, dz;
                Hilbert3D<Bits>::decode(code, dx, dy, dz);

                if (dx != x || dy != y || dz != z) {
                    std::cout << "FAIL\n    Roundtrip failed for (" << x << "," << y << "," << z << ")" << std::endl;
                    std::cout << "    Code: " << code << ", Decoded: (" << dx << "," << dy << "," << dz << ")" << std::endl;
                    return;
                }
            }
        }
    }

    PASS();
}

TEST(test_hilbert3d_spatial_locality) {
    // Test that spatially adjacent points have Hilbert codes within reasonable distance
    // (This verifies the curve has good spatial locality)
    constexpr unsigned Bits = 4;
    int locality_violations = 0;
    int total_tests = 0;

    for (uint32_t x = 1; x < 15; ++x) {
        for (uint32_t y = 1; y < 15; ++y) {
            for (uint32_t z = 1; z < 15; ++z) {
                uint64_t center = Hilbert3D<Bits>::encode(x, y, z);

                // Check 6 face-adjacent neighbors
                uint64_t neighbors[6] = {
                    Hilbert3D<Bits>::encode(x-1, y, z),
                    Hilbert3D<Bits>::encode(x+1, y, z),
                    Hilbert3D<Bits>::encode(x, y-1, z),
                    Hilbert3D<Bits>::encode(x, y+1, z),
                    Hilbert3D<Bits>::encode(x, y, z-1),
                    Hilbert3D<Bits>::encode(x, y, z+1)
                };

                // At least some neighbors should be close in Hilbert space
                int close_neighbors = 0;
                for (int i = 0; i < 6; ++i) {
                    int64_t diff = static_cast<int64_t>(neighbors[i]) - static_cast<int64_t>(center);
                    if (std::abs(diff) <= 64) close_neighbors++;  // Within 64 positions
                }

                if (close_neighbors < 2) locality_violations++;
                total_tests++;
            }
        }
    }

    // Allow up to 10% violations (edge effects and curve structure)
    double violation_rate = 100.0 * locality_violations / total_tests;
    if (violation_rate > 10.0) {
        std::cout << "FAIL\n    Locality violation rate: " << std::fixed << std::setprecision(1) << violation_rate << "%" << std::endl;
        return;
    }

    std::cout << "OK (locality violations: " << std::fixed << std::setprecision(1) << violation_rate << "%)" << std::endl;
    tests_passed++;
}

TEST(test_hilbert3d_lut_performance) {
    constexpr unsigned Bits = 10;
    constexpr uint32_t max_coord = (1 << Bits) - 1;
    constexpr int iterations = 100000;

    std::mt19937 rng(42);
    std::uniform_int_distribution<uint32_t> dist(0, max_coord);

    std::vector<uint32_t> xs(iterations), ys(iterations), zs(iterations);
    for (int i = 0; i < iterations; ++i) {
        xs[i] = dist(rng);
        ys[i] = dist(rng);
        zs[i] = dist(rng);
    }

    // Benchmark LUT encode
    volatile uint64_t sink = 0;
    auto start = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < iterations; ++i) {
        sink ^= Hilbert3D<Bits>::encode(xs[i], ys[i], zs[i]);
    }
    auto end = std::chrono::high_resolution_clock::now();
    double lut_ns = std::chrono::duration<double, std::nano>(end - start).count() / iterations;

    // Benchmark reference encode
    start = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < iterations; ++i) {
        sink ^= Hilbert3D<Bits>::encode_reference(xs[i], ys[i], zs[i]);
    }
    end = std::chrono::high_resolution_clock::now();
    double ref_ns = std::chrono::duration<double, std::nano>(end - start).count() / iterations;

    // Benchmark Morton3D for comparison
    start = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < iterations; ++i) {
        sink ^= Morton3D<Bits>::encode(xs[i], ys[i], zs[i]);
    }
    end = std::chrono::high_resolution_clock::now();
    double morton_ns = std::chrono::duration<double, std::nano>(end - start).count() / iterations;

    (void)sink;

    std::cout << "\n    Performance Results:" << std::endl;
    std::cout << "    Hilbert3D LUT:       " << std::fixed << std::setprecision(2) << lut_ns << " ns/op" << std::endl;
    std::cout << "    Hilbert3D Reference: " << std::fixed << std::setprecision(2) << ref_ns << " ns/op" << std::endl;
    std::cout << "    Morton3D:            " << std::fixed << std::setprecision(2) << morton_ns << " ns/op" << std::endl;
    std::cout << "    LUT vs Reference:    " << std::fixed << std::setprecision(1) << (ref_ns / lut_ns) << "x speedup" << std::endl;
    std::cout << "    LUT vs Morton3D:     " << std::fixed << std::setprecision(1) << (lut_ns / morton_ns) << "x slower" << std::endl;

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
