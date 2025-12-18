/*
 * SIMD Bloom Filter Adapter
 *
 * Wraps SimdBloomFilter and PatternedSimdBloomFilter from:
 * https://github.com/save-buffer/bloomfilter_benchmarks
 *
 * Uses AVX2 intrinsics for parallel processing of 8 elements.
 * Requires CPU with AVX2 support.
 */

#ifndef SIMD_BLOOM_FILTER_HPP
#define SIMD_BLOOM_FILTER_HPP

#include "hashfn.hpp"
#include "external/bloom_filters.h"
#include <array>
#include <cmath>
#include <sstream>
#include <stdexcept>
#include <vector>

// Check for AVX2 support at compile time
#if defined(__AVX2__)
#define SIMD_BF_AVX2_AVAILABLE 1
#else
#define SIMD_BF_AVX2_AVAILABLE 0
#endif

namespace globimap {

// ============================================================================
// Configuration
// ============================================================================

struct SimdBFConfig {
    uint64_t expected_items = 10000;     // n
    double false_positive_rate = 0.01;   // epsilon
    bool use_patterned = false;          // Use PatternedSimdBloomFilter variant

    void validate() const {
        if (expected_items == 0) {
            throw std::invalid_argument("expected_items must be > 0");
        }
        if (false_positive_rate <= 0.0 || false_positive_rate >= 1.0) {
            throw std::invalid_argument("false_positive_rate must be in (0, 1)");
        }
#if !SIMD_BF_AVX2_AVAILABLE
        throw std::runtime_error("SIMD Bloom Filter requires AVX2 support. "
                                 "Compile with -mavx2 flag.");
#endif
    }

    uint64_t computed_bits() const {
        if (use_patterned) {
            return std::max(512ULL, 8ULL * expected_items);
        }
        return static_cast<uint64_t>(-1.44 * expected_items * std::log2(false_positive_rate) + 0.5);
    }

    uint computed_k() const {
        return static_cast<uint>(-std::log2(false_positive_rate) + 0.5);
    }

    std::string to_string() const {
        std::ostringstream oss;
        oss << "SimdBFConfig(n=" << expected_items
            << ", fpr=" << false_positive_rate
            << ", patterned=" << (use_patterned ? "true" : "false")
            << ", bits=" << computed_bits() << ")";
        return oss.str();
    }
};

#if SIMD_BF_AVX2_AVAILABLE

// ============================================================================
// SimdBloomFilter Adapter
// ============================================================================

/**
 * SIMD-optimized bloom filter using AVX2.
 *
 * Processes 8 elements in parallel using 256-bit vector operations.
 * Uses gather instructions for efficient block lookups.
 *
 * For maximum throughput, use batch operations (put_batch, get_bool_batch).
 */
class SimdBloomFilter {
public:
    SimdBFConfig config;

private:
    external_bf::SimdBloomFilter impl_;
    static constexpr uint64_t SEED1 = 0x9e3779b97f4a7c15ULL;
    static constexpr uint64_t SEED2 = 0x85ebca6b517d3b25ULL;

    inline void hash_point(const std::vector<uint64_t> &point,
                           uint32_t &h1, uint32_t &h2) const {
        uint64_t hash1 = SEED1, hash2 = SEED2;
        hash(point.data(), point.size(), &hash1, &hash2);
        h1 = static_cast<uint32_t>(hash1);
        h2 = static_cast<uint32_t>(hash2);
    }

    // Hash 8 points at once for SIMD operations
    inline void hash_points_batch(const std::array<std::vector<uint64_t>, 8> &points,
                                  std::array<uint32_t, 8> &h1s,
                                  std::array<uint32_t, 8> &h2s) const {
        for (int i = 0; i < 8; ++i) {
            uint64_t hash1 = SEED1, hash2 = SEED2;
            hash(points[i].data(), points[i].size(), &hash1, &hash2);
            h1s[i] = static_cast<uint32_t>(hash1);
            h2s[i] = static_cast<uint32_t>(hash2);
        }
    }

public:
    SimdBloomFilter(const SimdBFConfig &conf)
        : config(conf),
          impl_(static_cast<int>(conf.expected_items),
                static_cast<float>(conf.false_positive_rate)) {
        conf.validate();
        if (conf.use_patterned) {
            throw std::invalid_argument("For patterned SIMD, use PatternedSimdBloomFilter class");
        }
    }

    // Single-element operations (less efficient, but compatible interface)
    void put(const std::vector<uint64_t> &point) {
        // Create a batch of 8 with padding
        std::array<uint32_t, 8> h1s, h2s;
        uint32_t h1, h2;
        hash_point(point, h1, h2);

        // Fill all 8 slots with the same element
        for (int i = 0; i < 8; ++i) {
            h1s[i] = h1;
            h2s[i] = h2;
        }
        impl_.Insert(h1s.data(), h2s.data());
    }

    bool get_bool(const std::vector<uint64_t> &point) const {
        std::array<uint32_t, 8> h1s, h2s;
        uint32_t h1, h2;
        const_cast<SimdBloomFilter*>(this)->hash_point(point, h1, h2);

        for (int i = 0; i < 8; ++i) {
            h1s[i] = h1;
            h2s[i] = h2;
        }
        uint8_t results = const_cast<external_bf::SimdBloomFilter&>(impl_).Query(h1s.data(), h2s.data());
        return (results & 0x01) != 0;
    }

    uint64_t get_min(const std::vector<uint64_t> &point) const {
        return get_bool(point) ? 1 : 0;
    }

    // Batch operations (8 elements at once - maximum throughput)
    void put_batch(const std::array<std::vector<uint64_t>, 8> &points) {
        std::array<uint32_t, 8> h1s, h2s;
        hash_points_batch(points, h1s, h2s);
        impl_.Insert(h1s.data(), h2s.data());
    }

    std::array<bool, 8> get_bool_batch(const std::array<std::vector<uint64_t>, 8> &points) const {
        std::array<uint32_t, 8> h1s, h2s;
        const_cast<SimdBloomFilter*>(this)->hash_points_batch(points, h1s, h2s);
        uint8_t results = const_cast<external_bf::SimdBloomFilter&>(impl_).Query(h1s.data(), h2s.data());

        std::array<bool, 8> out;
        for (int i = 0; i < 8; ++i) {
            out[i] = (results >> i) & 1;
        }
        return out;
    }

    void clear() {
        impl_.Reset();
    }

    uint64_t memory_usage() const {
        return impl_.bv.size() * sizeof(uint64_t);
    }

    std::string summary() const {
        std::ostringstream oss;
        oss << "{\n"
            << "  \"type\": \"SimdBloomFilter\",\n"
            << "  \"expected_items\": " << config.expected_items << ",\n"
            << "  \"false_positive_rate\": " << config.false_positive_rate << ",\n"
            << "  \"num_bits\": " << impl_.m << ",\n"
            << "  \"num_blocks\": " << impl_.num_blocks << ",\n"
            << "  \"hash_k\": " << impl_.k << ",\n"
            << "  \"memory_bytes\": " << memory_usage() << ",\n"
            << "  \"simd\": \"AVX2\"\n"
            << "}";
        return oss.str();
    }
};

// ============================================================================
// PatternedSimdBloomFilter Adapter
// ============================================================================

/**
 * Advanced SIMD bloom filter with pre-generated mask patterns.
 *
 * Uses a mask table with controlled bit density and rotation for better
 * distribution. Generally achieves better FPR than standard SimdBloomFilter
 * at similar memory usage.
 */
class PatternedSimdBloomFilter {
public:
    SimdBFConfig config;

private:
    external_bf::PatternedSimdBloomFilter impl_;
    static constexpr uint64_t SEED = 0x9e3779b97f4a7c15ULL;

    inline uint64_t hash_point_single(const std::vector<uint64_t> &point) const {
        uint64_t hash1 = SEED, hash2 = 0;
        hash(point.data(), point.size(), &hash1, &hash2);
        return hash1;
    }

    inline void hash_points_batch(const std::array<std::vector<uint64_t>, 8> &points,
                                  std::array<uint64_t, 8> &hashes) const {
        for (int i = 0; i < 8; ++i) {
            hashes[i] = const_cast<PatternedSimdBloomFilter*>(this)->hash_point_single(points[i]);
        }
    }

public:
    PatternedSimdBloomFilter(const SimdBFConfig &conf)
        : config(conf),
          impl_(static_cast<int>(conf.expected_items),
                static_cast<float>(conf.false_positive_rate)) {
        conf.validate();
    }

    // Single-element operations
    void put(const std::vector<uint64_t> &point) {
        std::array<uint64_t, 8> hashes;
        uint64_t h = hash_point_single(point);
        for (int i = 0; i < 8; ++i) {
            hashes[i] = h;
        }
        impl_.Insert(hashes.data());
    }

    bool get_bool(const std::vector<uint64_t> &point) const {
        std::array<uint64_t, 8> hashes;
        uint64_t h = const_cast<PatternedSimdBloomFilter*>(this)->hash_point_single(point);
        for (int i = 0; i < 8; ++i) {
            hashes[i] = h;
        }
        uint8_t results = const_cast<external_bf::PatternedSimdBloomFilter&>(impl_).Query(hashes.data());
        return (results & 0x01) != 0;
    }

    uint64_t get_min(const std::vector<uint64_t> &point) const {
        return get_bool(point) ? 1 : 0;
    }

    // Batch operations (8 elements at once)
    void put_batch(const std::array<std::vector<uint64_t>, 8> &points) {
        std::array<uint64_t, 8> hashes;
        hash_points_batch(points, hashes);
        impl_.Insert(hashes.data());
    }

    std::array<bool, 8> get_bool_batch(const std::array<std::vector<uint64_t>, 8> &points) const {
        std::array<uint64_t, 8> hashes;
        const_cast<PatternedSimdBloomFilter*>(this)->hash_points_batch(points, hashes);
        uint8_t results = const_cast<external_bf::PatternedSimdBloomFilter&>(impl_).Query(hashes.data());

        std::array<bool, 8> out;
        for (int i = 0; i < 8; ++i) {
            out[i] = (results >> i) & 1;
        }
        return out;
    }

    void clear() {
        impl_.Reset();
    }

    uint64_t memory_usage() const {
        return impl_.bv.size() * sizeof(uint64_t) + sizeof(external_bf::MaskTable);
    }

    std::string summary() const {
        std::ostringstream oss;
        oss << "{\n"
            << "  \"type\": \"PatternedSimdBloomFilter\",\n"
            << "  \"expected_items\": " << config.expected_items << ",\n"
            << "  \"false_positive_rate\": " << config.false_positive_rate << ",\n"
            << "  \"num_bits\": " << impl_.m << ",\n"
            << "  \"num_blocks\": " << impl_.num_blocks << ",\n"
            << "  \"memory_bytes\": " << memory_usage() << ",\n"
            << "  \"simd\": \"AVX2 + MaskTable\"\n"
            << "}";
        return oss.str();
    }
};

#else // !SIMD_BF_AVX2_AVAILABLE

// Fallback stubs when AVX2 is not available
class SimdBloomFilter {
public:
    SimdBloomFilter(const SimdBFConfig &) {
        throw std::runtime_error("SIMD Bloom Filter requires AVX2. Compile with -mavx2");
    }
    void put(const std::vector<uint64_t> &) {}
    bool get_bool(const std::vector<uint64_t> &) const { return false; }
    uint64_t get_min(const std::vector<uint64_t> &) const { return 0; }
    void clear() {}
    uint64_t memory_usage() const { return 0; }
    std::string summary() const { return "{}"; }
};

class PatternedSimdBloomFilter {
public:
    PatternedSimdBloomFilter(const SimdBFConfig &) {
        throw std::runtime_error("Patterned SIMD Bloom Filter requires AVX2. Compile with -mavx2");
    }
    void put(const std::vector<uint64_t> &) {}
    bool get_bool(const std::vector<uint64_t> &) const { return false; }
    uint64_t get_min(const std::vector<uint64_t> &) const { return 0; }
    void clear() {}
    uint64_t memory_usage() const { return 0; }
    std::string summary() const { return "{}"; }
};

#endif // SIMD_BF_AVX2_AVAILABLE

// Helper to check if SIMD is available at runtime
inline bool simd_available() {
#if SIMD_BF_AVX2_AVAILABLE
    return true;
#else
    return false;
#endif
}

} // namespace globimap

#endif // SIMD_BLOOM_FILTER_HPP
