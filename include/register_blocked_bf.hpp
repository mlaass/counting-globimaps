/*
 * Register-Blocked Bloom Filter Adapter
 *
 * Wraps RegisterBlockedBloomFilter from:
 * https://github.com/save-buffer/bloomfilter_benchmarks
 *
 * Uses 64-bit registers as atomic units with combined masks.
 * More cache-efficient than byte-level blocked filters.
 */

#ifndef REGISTER_BLOCKED_BF_HPP
#define REGISTER_BLOCKED_BF_HPP

#include "hashfn.hpp"
#include "external/bloom_filters.h"
#include <cmath>
#include <sstream>
#include <stdexcept>
#include <vector>

namespace globimap {

// ============================================================================
// Configuration
// ============================================================================

struct RegisterBlockedBFConfig {
    uint64_t expected_items = 10000;     // n
    double false_positive_rate = 0.01;   // epsilon
    int compensation = 0;                // Bit allocation tuning (-2 to +4 typical)

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
            << ", k=" << computed_k() << ")";
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
 * Template parameter Compensation adjusts bits per value:
 * - Negative: fewer bits, higher FPR, less memory
 * - Positive: more bits, lower FPR, more memory
 */
template <int Compensation = 0>
class RegisterBlockedBloomFilter {
public:
    RegisterBlockedBFConfig config;

private:
    external_bf::RegisterBlockedBloomFilter<Compensation> impl_;
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
    RegisterBlockedBloomFilter(const RegisterBlockedBFConfig &conf)
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
        const_cast<RegisterBlockedBloomFilter*>(this)->hash_point(point, h1, h2);
        return const_cast<external_bf::RegisterBlockedBloomFilter<Compensation>&>(impl_).Query(h1, h2);
    }

    uint64_t get_min(const std::vector<uint64_t> &point) const {
        return get_bool(point) ? 1 : 0;
    }

    void clear() {
        impl_.Reset();
    }

    uint64_t memory_usage() const {
        return impl_.bv.size() * sizeof(uint64_t);
    }

    uint64_t num_blocks() const {
        return impl_.num_blocks;
    }

    std::string summary() const {
        std::ostringstream oss;
        oss << "{\n"
            << "  \"type\": \"RegisterBlockedBloomFilter\",\n"
            << "  \"expected_items\": " << config.expected_items << ",\n"
            << "  \"false_positive_rate\": " << config.false_positive_rate << ",\n"
            << "  \"compensation\": " << Compensation << ",\n"
            << "  \"num_bits\": " << impl_.m << ",\n"
            << "  \"num_blocks\": " << impl_.num_blocks << ",\n"
            << "  \"hash_k\": " << impl_.k << ",\n"
            << "  \"memory_bytes\": " << memory_usage() << "\n"
            << "}";
        return oss.str();
    }
};

// Common instantiations
using RegisterBlockedBF = RegisterBlockedBloomFilter<0>;
using RegisterBlockedBFCompact = RegisterBlockedBloomFilter<-2>;
using RegisterBlockedBFAccurate = RegisterBlockedBloomFilter<2>;

} // namespace globimap

#endif // REGISTER_BLOCKED_BF_HPP
