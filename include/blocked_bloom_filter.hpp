/*
 * Blocked Bloom Filter Adapter
 *
 * Wraps cache-optimal bloom filter implementations from:
 * https://github.com/save-buffer/bloomfilter_benchmarks
 * https://save-buffer.github.io/bloom_filter.html
 *
 * These are membership-only filters (no counting).
 * get_min() returns 0 or 1 for compatibility with counting interface.
 */

#ifndef BLOCKED_BLOOM_FILTER_HPP
#define BLOCKED_BLOOM_FILTER_HPP

#include "hashfn.hpp"
#include "external/bloom_filters.h"
#include <cmath>
#include <sstream>
#include <stdexcept>
#include <vector>

namespace globimap {

// ============================================================================
// Configuration Structs
// ============================================================================

struct BlockedBFConfig {
    uint64_t expected_items = 10000;     // n
    double false_positive_rate = 0.01;   // epsilon

    void validate() const {
        if (expected_items == 0) {
            throw std::invalid_argument("expected_items must be > 0");
        }
        if (false_positive_rate <= 0.0 || false_positive_rate >= 1.0) {
            throw std::invalid_argument("false_positive_rate must be in (0, 1)");
        }
    }

    uint64_t computed_bits() const {
        return static_cast<uint64_t>(-1.44 * expected_items * std::log2(false_positive_rate) + 0.5);
    }

    uint computed_k() const {
        return static_cast<uint>(-std::log2(false_positive_rate) + 0.5);
    }

    std::string to_string() const {
        std::ostringstream oss;
        oss << "BlockedBFConfig(n=" << expected_items
            << ", fpr=" << false_positive_rate
            << ", bits=" << computed_bits()
            << ", k=" << computed_k() << ")";
        return oss.str();
    }
};

// ============================================================================
// BlockedBloomFilter Adapter
// ============================================================================

/**
 * Cache-optimized blocked bloom filter.
 *
 * Organizes bit vector into 256-bit cache-line aligned blocks.
 * First hash determines block; remaining hashes probe within that block.
 * Reduces cache misses through spatial locality.
 */
class BlockedBloomFilter {
public:
    BlockedBFConfig config;

private:
    external_bf::BlockedBloomFilter impl_;
    static constexpr uint64_t SEED1 = 0x9e3779b97f4a7c15ULL;
    static constexpr uint64_t SEED2 = 0x85ebca6b517d3b25ULL;

    inline void hash_point(const std::vector<uint64_t> &point,
                           uint32_t &h1, uint32_t &h2) const {
        uint64_t hash1 = SEED1, hash2 = SEED2;
        hash(point.data(), point.size(), &hash1, &hash2);
        h1 = static_cast<uint32_t>(hash1);
        h2 = static_cast<uint32_t>(hash2);
    }

public:
    BlockedBloomFilter(const BlockedBFConfig &conf)
        : config(conf),
          impl_(static_cast<int>(conf.expected_items),
                static_cast<float>(conf.false_positive_rate)) {
        conf.validate();
    }

    void put(const std::vector<uint64_t> &point) {
        uint32_t h1, h2;
        hash_point(point, h1, h2);
        impl_.Insert(h1, h2);
    }

    bool get_bool(const std::vector<uint64_t> &point) const {
        uint32_t h1, h2;
        const_cast<BlockedBloomFilter*>(this)->hash_point(point, h1, h2);
        return const_cast<external_bf::BlockedBloomFilter&>(impl_).Query(h1, h2);
    }

    uint64_t get_min(const std::vector<uint64_t> &point) const {
        return get_bool(point) ? 1 : 0;
    }

    void clear() {
        impl_.Reset();
    }

    uint64_t memory_usage() const {
        return impl_.bv.size();
    }

    std::string summary() const {
        std::ostringstream oss;
        oss << "{\n"
            << "  \"type\": \"BlockedBloomFilter\",\n"
            << "  \"expected_items\": " << config.expected_items << ",\n"
            << "  \"false_positive_rate\": " << config.false_positive_rate << ",\n"
            << "  \"num_bits\": " << impl_.m << ",\n"
            << "  \"num_blocks\": " << impl_.num_blocks << ",\n"
            << "  \"hash_k\": " << impl_.k << ",\n"
            << "  \"memory_bytes\": " << memory_usage() << "\n"
            << "}";
        return oss.str();
    }
};

// ============================================================================
// BasicBloomFilter Adapter (for comparison baseline)
// ============================================================================

/**
 * Standard bloom filter (non-blocked baseline).
 * Included for performance comparison.
 */
class BasicBloomFilterAdapter {
public:
    BlockedBFConfig config;

private:
    external_bf::BasicBloomFilter impl_;
    static constexpr uint64_t SEED1 = 0x9e3779b97f4a7c15ULL;
    static constexpr uint64_t SEED2 = 0x85ebca6b517d3b25ULL;

    inline void hash_point(const std::vector<uint64_t> &point,
                           uint32_t &h1, uint32_t &h2) const {
        uint64_t hash1 = SEED1, hash2 = SEED2;
        hash(point.data(), point.size(), &hash1, &hash2);
        h1 = static_cast<uint32_t>(hash1);
        h2 = static_cast<uint32_t>(hash2);
    }

public:
    BasicBloomFilterAdapter(const BlockedBFConfig &conf)
        : config(conf),
          impl_(static_cast<int>(conf.expected_items),
                static_cast<float>(conf.false_positive_rate)) {
        conf.validate();
    }

    void put(const std::vector<uint64_t> &point) {
        uint32_t h1, h2;
        hash_point(point, h1, h2);
        impl_.Insert(h1, h2);
    }

    bool get_bool(const std::vector<uint64_t> &point) const {
        uint32_t h1, h2;
        const_cast<BasicBloomFilterAdapter*>(this)->hash_point(point, h1, h2);
        return const_cast<external_bf::BasicBloomFilter&>(impl_).Query(h1, h2);
    }

    uint64_t get_min(const std::vector<uint64_t> &point) const {
        return get_bool(point) ? 1 : 0;
    }

    void clear() {
        impl_.Reset();
    }

    uint64_t memory_usage() const {
        return impl_.bv.size();
    }

    std::string summary() const {
        std::ostringstream oss;
        oss << "{\n"
            << "  \"type\": \"BasicBloomFilter\",\n"
            << "  \"expected_items\": " << config.expected_items << ",\n"
            << "  \"false_positive_rate\": " << config.false_positive_rate << ",\n"
            << "  \"num_bits\": " << impl_.m << ",\n"
            << "  \"hash_k\": " << impl_.k << ",\n"
            << "  \"memory_bytes\": " << memory_usage() << "\n"
            << "}";
        return oss.str();
    }
};

} // namespace globimap

#endif // BLOCKED_BLOOM_FILTER_HPP
