/*
 * Blocked Bloom Filter Adapter
 *
 * Wraps cache-optimal bloom filter implementations from:
 * https://github.com/save-buffer/bloomfilter_benchmarks
 * https://save-buffer.github.io/bloom_filter.html
 *
 * These are membership-only filters (no counting).
 * get_min() returns 0 or 1 for compatibility with counting interface.
 *
 * Supports two intra-block strategies:
 * - DOUBLE_HASH: h_i = (h1 + i*h2) % 256 (default)
 * - PATTERN_LOOKUP: Pre-computed k-bit patterns (15-25% fewer instructions)
 *
 * Supports configurable hash functions via template parameter:
 * - BlockedBloomFilter<MurmurHasher> (default)
 * - BlockedBloomFilter<XXH3Hasher> (1.5x faster)
 * - BlockedBloomFilter<WyHasher> (1.2x faster)
 */

#ifndef BLOCKED_BLOOM_FILTER_HPP
#define BLOCKED_BLOOM_FILTER_HPP

#include "hash_functions.hpp"
#include "hashfn.hpp"  // For BasicBloomFilterAdapter backward compatibility
#include "pattern_table.hpp"
#include "external/bloom_filters.h"
#include <cmath>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <vector>

namespace globimap {

// Intra-block hashing strategy (shared with register_blocked_bf.hpp)
enum class IntraBlockStrategy {
    DOUBLE_HASH,     // h_i = (h1 + i*h2) % block_size
    PATTERN_LOOKUP   // Pre-computed k-bit patterns
};

// ============================================================================
// Configuration Structs
// ============================================================================

struct BlockedBFConfig {
    // Direct parameters (preferred for research - set these directly)
    uint64_t num_blocks = 0;             // If > 0, use directly (each block = 256 bits = 32 bytes)
    unsigned hash_k = 0;                 // If > 0, use directly

    // Legacy derived parameters (only used if num_blocks == 0)
    uint64_t expected_items = 10000;     // n
    double false_positive_rate = 0.01;   // epsilon

    // Intra-block hashing strategy
    IntraBlockStrategy intra_strategy = IntraBlockStrategy::DOUBLE_HASH;
    size_t pattern_table_size = 1024;    // 256, 512, 1024, or 2048

    void validate() const {
        if (num_blocks > 0) {
            // Direct mode - validate direct params
            if (hash_k == 0) {
                throw std::invalid_argument("hash_k must be > 0 when using direct num_blocks");
            }
        } else {
            // Legacy mode - validate derived params
            if (expected_items == 0) {
                throw std::invalid_argument("expected_items must be > 0");
            }
            if (false_positive_rate <= 0.0 || false_positive_rate >= 1.0) {
                throw std::invalid_argument("false_positive_rate must be in (0, 1)");
            }
        }
        if (intra_strategy == IntraBlockStrategy::PATTERN_LOOKUP) {
            if (pattern_table_size != 256 && pattern_table_size != 512 &&
                pattern_table_size != 1024 && pattern_table_size != 2048) {
                throw std::invalid_argument(
                    "pattern_table_size must be 256, 512, 1024, or 2048");
            }
        }
    }

    uint64_t computed_bits() const {
        if (num_blocks > 0) {
            return num_blocks * 256;  // 256 bits per block
        }
        return static_cast<uint64_t>(-1.44 * expected_items * std::log2(false_positive_rate) + 0.5);
    }

    uint computed_k() const {
        if (hash_k > 0) {
            return hash_k;
        }
        return static_cast<uint>(-std::log2(false_positive_rate) + 0.5);
    }

    uint64_t computed_num_blocks() const {
        if (num_blocks > 0) {
            return num_blocks;
        }
        return (computed_bits() + 255) / 256;
    }

    std::string to_string() const {
        std::ostringstream oss;
        if (num_blocks > 0) {
            oss << "BlockedBFConfig(num_blocks=" << num_blocks
                << ", hash_k=" << hash_k;
        } else {
            oss << "BlockedBFConfig(n=" << expected_items
                << ", fpr=" << false_positive_rate
                << ", blocks=" << computed_num_blocks()
                << ", k=" << computed_k();
        }
        oss << ", strategy=";
        if (intra_strategy == IntraBlockStrategy::PATTERN_LOOKUP) {
            oss << "PATTERN_LOOKUP[" << pattern_table_size << "]";
        } else {
            oss << "DOUBLE_HASH";
        }
        oss << ")";
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
 *
 * Supports two intra-block strategies:
 * - DOUBLE_HASH: Standard h_i = (h1 + i*h2) % 256
 * - PATTERN_LOOKUP: Pre-computed 256-bit patterns (15-25% fewer instructions)
 *
 * Template Parameters:
 *   Hasher - Hash function struct (MurmurHasher, XXH3Hasher, WyHasher)
 */
template<typename Hasher = DefaultHasher>
class BlockedBloomFilter {
public:
    BlockedBFConfig config;

private:
    static constexpr size_t BLOCK_BITS = 256;
    static constexpr size_t BLOCK_BYTES = 32;

    std::vector<uint8_t> blocks_;
    size_t num_blocks_;
    unsigned k_;

    // Pattern tables for different sizes (256-bit patterns)
    std::unique_ptr<PatternTable256<256>> pt_256_;
    std::unique_ptr<PatternTable256<512>> pt_512_;
    std::unique_ptr<PatternTable256<1024>> pt_1024_;
    std::unique_ptr<PatternTable256<2048>> pt_2048_;

    static constexpr uint64_t SEED1 = 0x9e3779b97f4a7c15ULL;
    static constexpr uint64_t SEED2 = 0x85ebca6b517d3b25ULL;

    inline void hash_point(const std::vector<uint64_t> &point,
                           uint32_t &h1, uint32_t &h2) const {
        uint64_t hash1 = SEED1, hash2 = SEED2;
        Hasher::hash(point.data(), point.size(), &hash1, &hash2);
        h1 = static_cast<uint32_t>(hash1);
        h2 = static_cast<uint32_t>(hash2);
    }

    /// Get block pointer for given hash
    uint8_t* get_block(uint32_t h1) {
        size_t block_idx = h1 % num_blocks_;
        return blocks_.data() + block_idx * BLOCK_BYTES;
    }

    const uint8_t* get_block(uint32_t h1) const {
        size_t block_idx = h1 % num_blocks_;
        return blocks_.data() + block_idx * BLOCK_BYTES;
    }

    /// Apply double-hash pattern to block
    void apply_double_hash(uint8_t* block, uint32_t h1, uint32_t h2) {
        for (unsigned i = 1; i <= k_; ++i) {
            uint32_t bit_pos = (h1 + i * h2) % BLOCK_BITS;
            uint32_t byte_idx = bit_pos / 8;
            uint32_t bit_idx = bit_pos % 8;
            block[byte_idx] |= (1 << bit_idx);
        }
    }

    /// Check double-hash pattern in block
    bool check_double_hash(const uint8_t* block, uint32_t h1, uint32_t h2) const {
        for (unsigned i = 1; i <= k_; ++i) {
            uint32_t bit_pos = (h1 + i * h2) % BLOCK_BITS;
            uint32_t byte_idx = bit_pos / 8;
            uint32_t bit_idx = bit_pos % 8;
            if (!((block[byte_idx] >> bit_idx) & 1)) {
                return false;
            }
        }
        return true;
    }

    /// Get pattern from table
    const Pattern256* get_pattern(uint32_t h2) const {
        if (pt_256_) return &pt_256_->get(h2);
        if (pt_512_) return &pt_512_->get(h2);
        if (pt_1024_) return &pt_1024_->get(h2);
        if (pt_2048_) return &pt_2048_->get(h2);
        return nullptr;
    }

public:
    BlockedBloomFilter(const BlockedBFConfig &conf) : config(conf) {
        conf.validate();

        // Use direct params if set, otherwise derive from expected_items/fpr
        if (conf.num_blocks > 0) {
            // Direct mode
            num_blocks_ = conf.num_blocks;
            k_ = conf.hash_k;
        } else {
            // Legacy mode - derive from expected_items and false_positive_rate
            size_t m = static_cast<size_t>(-1.44 * conf.expected_items *
                                            std::log2(conf.false_positive_rate) + 0.5);
            k_ = static_cast<unsigned>(-std::log2(conf.false_positive_rate) + 0.5);
            num_blocks_ = (m + BLOCK_BITS - 1) / BLOCK_BITS;
        }

        // Allocate blocks (256 bits = 32 bytes each)
        blocks_.resize(num_blocks_ * BLOCK_BYTES, 0);

        // Initialize pattern table if needed
        if (conf.intra_strategy == IntraBlockStrategy::PATTERN_LOOKUP) {
            switch (conf.pattern_table_size) {
                case 256:
                    pt_256_ = std::make_unique<PatternTable256<256>>(k_);
                    break;
                case 512:
                    pt_512_ = std::make_unique<PatternTable256<512>>(k_);
                    break;
                case 1024:
                    pt_1024_ = std::make_unique<PatternTable256<1024>>(k_);
                    break;
                case 2048:
                    pt_2048_ = std::make_unique<PatternTable256<2048>>(k_);
                    break;
            }
        }
    }

    void put(const std::vector<uint64_t> &point) {
        uint32_t h1, h2;
        hash_point(point, h1, h2);
        uint8_t* block = get_block(h1);

        if (config.intra_strategy == IntraBlockStrategy::PATTERN_LOOKUP) {
            auto* pattern = get_pattern(h2);
            if (pattern) {
                pattern->apply(block);
            }
        } else {
            apply_double_hash(block, h1, h2);
        }
    }

    bool get_bool(const std::vector<uint64_t> &point) const {
        uint32_t h1, h2;
        const_cast<BlockedBloomFilter*>(this)->hash_point(point, h1, h2);
        const uint8_t* block = get_block(h1);

        if (config.intra_strategy == IntraBlockStrategy::PATTERN_LOOKUP) {
            auto* pattern = get_pattern(h2);
            if (pattern) {
                return pattern->matches(block);
            }
            return false;
        } else {
            return check_double_hash(block, h1, h2);
        }
    }

    uint64_t get_min(const std::vector<uint64_t> &point) const {
        return get_bool(point) ? 1 : 0;
    }

    void clear() {
        std::fill(blocks_.begin(), blocks_.end(), 0);
    }

    uint64_t memory_usage() const {
        size_t mem = blocks_.size();
        if (pt_256_) mem += pt_256_->memory_bytes();
        if (pt_512_) mem += pt_512_->memory_bytes();
        if (pt_1024_) mem += pt_1024_->memory_bytes();
        if (pt_2048_) mem += pt_2048_->memory_bytes();
        return mem;
    }

    uint64_t num_blocks() const {
        return num_blocks_;
    }

    unsigned hash_k() const {
        return k_;
    }

    std::string summary() const {
        std::ostringstream oss;
        oss << "{\n"
            << "  \"type\": \"BlockedBloomFilter\",\n"
            << "  \"hasher\": \"" << Hasher::name << "\",\n"
            << "  \"expected_items\": " << config.expected_items << ",\n"
            << "  \"false_positive_rate\": " << config.false_positive_rate << ",\n"
            << "  \"num_blocks\": " << num_blocks_ << ",\n"
            << "  \"hash_k\": " << k_ << ",\n"
            << "  \"intra_strategy\": \"";
        if (config.intra_strategy == IntraBlockStrategy::PATTERN_LOOKUP) {
            oss << "PATTERN_LOOKUP[" << config.pattern_table_size << "]";
        } else {
            oss << "DOUBLE_HASH";
        }
        oss << "\",\n"
            << "  \"memory_bytes\": " << memory_usage() << "\n"
            << "}";
        return oss.str();
    }
};

// ============================================================================
// Type Aliases for Common Hasher Configurations
// ============================================================================

using BlockedBloomFilterMurmur = BlockedBloomFilter<MurmurHasher>;
using BlockedBloomFilterXXH3 = BlockedBloomFilter<XXH3Hasher>;
using BlockedBloomFilterWyhash = BlockedBloomFilter<WyHasher>;

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
