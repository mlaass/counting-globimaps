/*
 * Register-Blocked Bloom Filter Adapter
 *
 * Wraps RegisterBlockedBloomFilter from:
 * https://github.com/save-buffer/bloomfilter_benchmarks
 *
 * Uses 64-bit registers as atomic units with combined masks.
 * More cache-efficient than byte-level blocked filters.
 *
 * Supports two intra-block strategies:
 * - DOUBLE_HASH: h_i = (h1 + i*h2) % 64 (default)
 * - PATTERN_LOOKUP: Pre-computed k-bit patterns (15-25% fewer instructions)
 *
 * Supports configurable hash functions via template parameter:
 * - RegisterBlockedBloomFilter<0, MurmurHasher> (default)
 * - RegisterBlockedBloomFilter<0, XXH3Hasher> (1.5x faster)
 * - RegisterBlockedBloomFilter<0, WyHasher> (1.2x faster)
 */

#ifndef REGISTER_BLOCKED_BF_HPP
#define REGISTER_BLOCKED_BF_HPP

#include "hash_functions.hpp"
#include "pattern_table.hpp"
#include "blocked_bloom_filter.hpp"  // For IntraBlockStrategy enum
#include "external/bloom_filters.h"
#include <cmath>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <vector>

namespace globimap {

// IntraBlockStrategy enum defined in blocked_bloom_filter.hpp

// ============================================================================
// Configuration
// ============================================================================

struct RegisterBlockedBFConfig {
    uint64_t expected_items = 10000;     // n
    double false_positive_rate = 0.01;   // epsilon
    int compensation = 0;                // Bit allocation tuning (-2 to +4 typical)

    // Intra-block hashing strategy
    IntraBlockStrategy intra_strategy = IntraBlockStrategy::DOUBLE_HASH;
    size_t pattern_table_size = 1024;    // 256, 512, 1024, or 2048

    void validate() const {
        if (expected_items == 0) {
            throw std::invalid_argument("expected_items must be > 0");
        }
        if (false_positive_rate <= 0.0 || false_positive_rate >= 1.0) {
            throw std::invalid_argument("false_positive_rate must be in (0, 1)");
        }
        if (compensation < -10 || compensation > 10) {
            throw std::invalid_argument("compensation should be in [-10, 10]");
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
        double bits_per_val = -1.44 * std::log2(false_positive_rate) + compensation;
        return static_cast<uint64_t>(bits_per_val * expected_items + 0.5);
    }

    uint computed_k() const {
        return static_cast<uint>(-std::log2(false_positive_rate) + 0.5);
    }

    std::string to_string() const {
        std::ostringstream oss;
        oss << "RegisterBlockedBFConfig(n=" << expected_items
            << ", fpr=" << false_positive_rate
            << ", comp=" << compensation
            << ", bits=" << computed_bits()
            << ", k=" << computed_k()
            << ", strategy=";
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
// RegisterBlockedBloomFilter Adapter
// ============================================================================

/**
 * Register-blocked bloom filter using 64-bit atomic units.
 *
 * Constructs a single 64-bit mask combining all hash positions,
 * then applies it with a single OR (insert) or AND (query) operation.
 * Very cache-efficient for modern CPUs.
 *
 * Supports two intra-block strategies:
 * - DOUBLE_HASH: Standard h_i = (h1 + i*h2) % 64
 * - PATTERN_LOOKUP: Pre-computed patterns (15-25% fewer instructions)
 *
 * Template Parameters:
 *   Compensation - Adjusts bits per value:
 *     - Negative: fewer bits, higher FPR, less memory
 *     - Positive: more bits, lower FPR, more memory
 *   Hasher - Hash function struct (MurmurHasher, XXH3Hasher, WyHasher)
 */
template <int Compensation = 0, typename Hasher = DefaultHasher>
class RegisterBlockedBloomFilter {
public:
    RegisterBlockedBFConfig config;

private:
    std::vector<uint64_t> blocks_;
    size_t num_blocks_;
    unsigned k_;

    // Pattern table (only used if PATTERN_LOOKUP strategy)
    std::unique_ptr<PatternTable64<256>> pt_256_;
    std::unique_ptr<PatternTable64<512>> pt_512_;
    std::unique_ptr<PatternTable64<1024>> pt_1024_;
    std::unique_ptr<PatternTable64<2048>> pt_2048_;

    static constexpr uint64_t SEED1 = 0x9e3779b97f4a7c15ULL;
    static constexpr uint64_t SEED2 = 0x85ebca6b517d3b25ULL;

    inline void hash_point(const std::vector<uint64_t> &point,
                           uint32_t &h1, uint32_t &h2) const {
        uint64_t hash1 = SEED1, hash2 = SEED2;
        Hasher::hash(point.data(), point.size(), &hash1, &hash2);
        h1 = static_cast<uint32_t>(hash1);
        h2 = static_cast<uint32_t>(hash2);
    }

    /// Construct mask using double-hashing
    uint64_t construct_mask_double_hash(uint32_t h1, uint32_t h2) const {
        uint64_t mask = 0;
        for (unsigned i = 1; i <= k_; ++i) {
            uint32_t bit_pos = (h1 + i * h2) % 64;
            mask |= (1ULL << bit_pos);
        }
        return mask;
    }

    /// Construct mask using pattern table lookup
    uint64_t construct_mask_pattern(uint32_t h2) const {
        if (pt_256_) return pt_256_->get(h2);
        if (pt_512_) return pt_512_->get(h2);
        if (pt_1024_) return pt_1024_->get(h2);
        if (pt_2048_) return pt_2048_->get(h2);
        return 0;  // Should never happen
    }

    /// Get block index for given hash
    size_t get_block_idx(uint32_t h1) const {
        return h1 % num_blocks_;
    }

public:
    RegisterBlockedBloomFilter(const RegisterBlockedBFConfig &conf)
        : config(conf) {
        conf.validate();

        // Compute number of bits and k
        double bits_per_val = -1.44 * std::log2(conf.false_positive_rate) + Compensation;
        size_t m = static_cast<size_t>(bits_per_val * conf.expected_items + 0.5);
        k_ = static_cast<unsigned>(-std::log2(conf.false_positive_rate) + 0.5);

        // Allocate blocks (64 bits each)
        num_blocks_ = (m + 63) / 64;
        blocks_.resize(num_blocks_, 0);

        // Initialize pattern table if needed
        if (conf.intra_strategy == IntraBlockStrategy::PATTERN_LOOKUP) {
            switch (conf.pattern_table_size) {
                case 256:
                    pt_256_ = std::make_unique<PatternTable64<256>>(k_);
                    break;
                case 512:
                    pt_512_ = std::make_unique<PatternTable64<512>>(k_);
                    break;
                case 1024:
                    pt_1024_ = std::make_unique<PatternTable64<1024>>(k_);
                    break;
                case 2048:
                    pt_2048_ = std::make_unique<PatternTable64<2048>>(k_);
                    break;
            }
        }
    }

    void put(const std::vector<uint64_t> &point) {
        uint32_t h1, h2;
        hash_point(point, h1, h2);

        size_t block_idx = get_block_idx(h1);
        uint64_t mask;

        if (config.intra_strategy == IntraBlockStrategy::PATTERN_LOOKUP) {
            mask = construct_mask_pattern(h2);
        } else {
            mask = construct_mask_double_hash(h1, h2);
        }

        blocks_[block_idx] |= mask;
    }

    bool get_bool(const std::vector<uint64_t> &point) const {
        uint32_t h1, h2;
        const_cast<RegisterBlockedBloomFilter*>(this)->hash_point(point, h1, h2);

        size_t block_idx = get_block_idx(h1);
        uint64_t mask;

        if (config.intra_strategy == IntraBlockStrategy::PATTERN_LOOKUP) {
            mask = construct_mask_pattern(h2);
        } else {
            mask = construct_mask_double_hash(h1, h2);
        }

        return (blocks_[block_idx] & mask) == mask;
    }

    uint64_t get_min(const std::vector<uint64_t> &point) const {
        return get_bool(point) ? 1 : 0;
    }

    void clear() {
        std::fill(blocks_.begin(), blocks_.end(), 0ULL);
    }

    uint64_t memory_usage() const {
        size_t mem = blocks_.size() * sizeof(uint64_t);
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
            << "  \"type\": \"RegisterBlockedBloomFilter\",\n"
            << "  \"hasher\": \"" << Hasher::name << "\",\n"
            << "  \"expected_items\": " << config.expected_items << ",\n"
            << "  \"false_positive_rate\": " << config.false_positive_rate << ",\n"
            << "  \"compensation\": " << Compensation << ",\n"
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
// Type Aliases for Common Configurations
// ============================================================================

// Default compensation with different hashers
using RegisterBlockedBF = RegisterBlockedBloomFilter<0>;
using RegisterBlockedBFCompact = RegisterBlockedBloomFilter<-2>;
using RegisterBlockedBFAccurate = RegisterBlockedBloomFilter<2>;

// XXH3 variants (faster)
using RegisterBlockedBFXXH3 = RegisterBlockedBloomFilter<0, XXH3Hasher>;
using RegisterBlockedBFXXH3Compact = RegisterBlockedBloomFilter<-2, XXH3Hasher>;
using RegisterBlockedBFXXH3Accurate = RegisterBlockedBloomFilter<2, XXH3Hasher>;

// wyhash variants (simple & fast)
using RegisterBlockedBFWyhash = RegisterBlockedBloomFilter<0, WyHasher>;
using RegisterBlockedBFWyhashCompact = RegisterBlockedBloomFilter<-2, WyHasher>;
using RegisterBlockedBFWyhashAccurate = RegisterBlockedBloomFilter<2, WyHasher>;

} // namespace globimap

#endif // REGISTER_BLOCKED_BF_HPP
