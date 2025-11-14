#ifndef GLOBIMAP_VARIABLE_INCREMENT_BF_HPP
#define GLOBIMAP_VARIABLE_INCREMENT_BF_HPP

#include "common/bf_config.hpp"
#include "common/bf_interface.hpp"
#include "hashfn.hpp"
#include <cassert>
#include <cstdint>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace globimap {

/**
 * @brief Configuration for Variable-Increment Counting Bloom Filter
 *
 * References:
 * Rottenstreich, O., Kanizo, Y., & Keslassy, I. (2014). The Variable-Increment
 * Counting Bloom Filter. IEEE/ACM Trans. Netw., 22(4), 1092-1105.
 * Paper name: "Variable-Increment" (VI-CBF)
 */
struct VICBFConfig {
    uint hash_k;              // Number of hash functions (typically 7-10)
    uint logsize;             // Filter size: 2^logsize counters
    uint counter_bits;        // Bits per counter (8, 16, 32, or 64)
    uint increment_L;         // Increment base (must be power of 2, typically 2-8)

    /**
     * @brief Validate configuration parameters
     * @throws std::invalid_argument if parameters are invalid
     */
    void validate() const {
        config_utils::validate_positive(hash_k, "hash_k");
        config_utils::validate_positive(logsize, "logsize");
        config_utils::validate_range(counter_bits, 8, 64, "counter_bits");

        // counter_bits should be 8, 16, 32, or 64
        if (counter_bits != 8 && counter_bits != 16 &&
            counter_bits != 32 && counter_bits != 64) {
            throw std::invalid_argument("counter_bits must be 8, 16, 32, or 64");
        }

        config_utils::validate_power_of_two(increment_L, "increment_L");

        if (increment_L < 2 || increment_L > 256) {
            throw std::invalid_argument("increment_L should be in range [2, 256]");
        }
    }
};

/**
 * @brief Variable-Increment Counting Bloom Filter Implementation
 *
 * This implementation uses variable increments drawn from the set D_L = {L, L+1, ..., 2L-1}
 * where L is a power of 2. This approach achieves approximately 50% memory reduction
 * compared to standard counting bloom filters.
 *
 * Key features:
 * - Simple configuration: only 4 parameters
 * - 50% memory savings vs. standard CBF (from paper)
 * - Single-layer design with predictable memory usage
 * - No deletion support (counters only increment)
 *
 * Algorithm (from Rottenstreich et al., 2014, Section III):
 * - Insert: For each of k positions, add a random increment from [L, 2L-1]
 * - Query: Return minimum counter value across k positions
 *
 * @see VICBFConfig for configuration parameters
 */
class VariableIncrementBloomFilter {
private:
    VICBFConfig config_;
    uint64_t size_;           // 2^logsize
    uint64_t mask_;           // size_ - 1, for fast modulo
    uint64_t max_value_;      // Maximum counter value

    // Counter storage (templated by bit depth)
    std::vector<uint8_t> counters8_;
    std::vector<uint16_t> counters16_;
    std::vector<uint32_t> counters32_;
    std::vector<uint64_t> counters64_;

    // Hash seeds
    static constexpr uint64_t SEED1 = 0x9e3779b97f4a7c15ULL;  // Golden ratio
    static constexpr uint64_t SEED2 = 0x85ebca6b517d3b25ULL;
    static constexpr uint64_t SEED3 = 0xc2b2ae3d27d4eb4fULL;  // For increment hash

    /**
     * @brief Hash a point to get two base hash values
     * @param point The element to hash
     * @param h1 Output: first hash value
     * @param h2 Output: second hash value
     */
    inline void hash_point(const std::vector<uint64_t> &point,
                          uint64_t &h1, uint64_t &h2) const {
        h1 = SEED1;
        h2 = SEED2;
        hash(point.data(), point.size(), &h1, &h2);
    }

    /**
     * @brief Compute variable increment for a specific hash position
     *
     * Formula from Rottenstreich et al. (2014), Section III-A:
     * inc = L + (hash3(point, i) % L)
     * This produces values in range [L, 2L-1]
     *
     * @param point The element being inserted
     * @param hash_index Which of the k hash functions (0 to k-1)
     * @return Increment value in range [L, 2L-1]
     */
    inline uint64_t compute_increment(const std::vector<uint64_t> &point,
                                     uint hash_index) const {
        // Use a third independent hash with seed based on hash_index
        uint64_t h1 = SEED3 + hash_index;
        uint64_t h2 = SEED3;
        hash(point.data(), point.size(), &h1, &h2);

        // inc = L + (hash % L), where hash % L gives [0, L-1]
        uint64_t inc = config_.increment_L + (h1 % config_.increment_L);
        return inc;
    }

    /**
     * @brief Get counter value at position i (templated by bit depth)
     */
    inline uint64_t get_counter(uint64_t pos) const {
        switch (config_.counter_bits) {
            case 8:  return counters8_[pos];
            case 16: return counters16_[pos];
            case 32: return counters32_[pos];
            case 64: return counters64_[pos];
            default: assert(false); return 0;
        }
    }

    /**
     * @brief Increment counter at position by inc, checking for overflow
     */
    inline void increment_counter(uint64_t pos, uint64_t inc) {
        switch (config_.counter_bits) {
            case 8:
                if (counters8_[pos] <= max_value_ - inc) {
                    counters8_[pos] += static_cast<uint8_t>(inc);
                } else {
                    counters8_[pos] = static_cast<uint8_t>(max_value_);
                }
                break;
            case 16:
                if (counters16_[pos] <= max_value_ - inc) {
                    counters16_[pos] += static_cast<uint16_t>(inc);
                } else {
                    counters16_[pos] = static_cast<uint16_t>(max_value_);
                }
                break;
            case 32:
                if (counters32_[pos] <= max_value_ - inc) {
                    counters32_[pos] += static_cast<uint32_t>(inc);
                } else {
                    counters32_[pos] = static_cast<uint32_t>(max_value_);
                }
                break;
            case 64:
                if (counters64_[pos] <= max_value_ - inc) {
                    counters64_[pos] += inc;
                } else {
                    counters64_[pos] = max_value_;
                }
                break;
            default:
                assert(false);
        }
    }

public:
    /**
     * @brief Construct a Variable-Increment CBF with given configuration
     * @param conf Configuration parameters
     * @throws std::invalid_argument if configuration is invalid
     */
    explicit VariableIncrementBloomFilter(const VICBFConfig &conf) : config_(conf) {
        config_.validate();

        size_ = 1ULL << config_.logsize;
        mask_ = size_ - 1;
        max_value_ = config_utils::max_counter_value(config_.counter_bits);

        // Allocate counter storage based on bit depth
        switch (config_.counter_bits) {
            case 8:  counters8_.resize(size_, 0);  break;
            case 16: counters16_.resize(size_, 0); break;
            case 32: counters32_.resize(size_, 0); break;
            case 64: counters64_.resize(size_, 0); break;
            default: assert(false);
        }
    }

    /**
     * @brief Insert a single element into the filter
     *
     * Algorithm (Rottenstreich et al., 2014, Section III):
     * 1. Hash element to get base hashes h1, h2
     * 2. For each of k positions:
     *    - Compute position: pos = (h1 + i*h2) & mask
     *    - Compute increment: inc = L + (hash3(element, i) % L)
     *    - Add inc to counter at position (with overflow check)
     *
     * @param point The element to insert
     */
    void put(const std::vector<uint64_t> &point) {
        uint64_t h1, h2;
        hash_point(point, h1, h2);

        for (uint i = 0; i < config_.hash_k; ++i) {
            uint64_t pos = (h1 + (i + 1) * h2) & mask_;
            uint64_t inc = compute_increment(point, i);
            increment_counter(pos, inc);
        }
    }

    /**
     * @brief Batch insertion for performance
     *
     * Inserts multiple elements. Can be parallelized with OpenMP.
     *
     * @param points Vector of elements to insert
     */
    void put_all(const std::vector<std::vector<uint64_t>> &points) {
        for (const auto &point : points) {
            put(point);
        }
    }

    /**
     * @brief Query minimum counter value (cardinality estimation)
     *
     * Returns the minimum counter value across all k hash positions.
     * This provides an estimate of the element's frequency.
     *
     * Note: Due to variable increments, the counter value is approximately
     * 1.5L times the actual insertion count (expected increment is 1.5L).
     *
     * @param point The element to query
     * @return Estimated count (minimum across k positions)
     */
    uint64_t get_min(const std::vector<uint64_t> &point) {
        uint64_t h1, h2;
        hash_point(point, h1, h2);

        uint64_t min_count = UINT64_MAX;
        for (uint i = 0; i < config_.hash_k; ++i) {
            uint64_t pos = (h1 + (i + 1) * h2) & mask_;
            uint64_t count = get_counter(pos);
            if (count < min_count) {
                min_count = count;
            }
        }

        return min_count;
    }

    /**
     * @brief Membership test
     *
     * Returns true if the element was likely inserted (min count > 0).
     * May return false positives but never false negatives.
     *
     * @param point The element to query
     * @return true if likely present, false if definitely not
     */
    bool get_bool(const std::vector<uint64_t> &point) {
        return get_min(point) > 0;
    }

    /**
     * @brief Get memory usage in bytes
     * @return Total memory consumed by counter arrays
     */
    uint64_t memory_usage() const {
        switch (config_.counter_bits) {
            case 8:  return counters8_.size();
            case 16: return counters16_.size() * 2;
            case 32: return counters32_.size() * 4;
            case 64: return counters64_.size() * 8;
            default: return 0;
        }
    }

    /**
     * @brief Get summary statistics
     * @return Human-readable summary string
     */
    std::string summary() {
        std::stringstream ss;
        ss << "Variable-Increment Counting Bloom Filter\n";
        ss << "  Configuration:\n";
        ss << "    k (hash functions): " << config_.hash_k << "\n";
        ss << "    Filter size: 2^" << config_.logsize << " = " << size_ << " counters\n";
        ss << "    Counter bits: " << config_.counter_bits << " bits\n";
        ss << "    Increment base L: " << config_.increment_L << "\n";
        ss << "    Increment range: [" << config_.increment_L << ", "
           << (2 * config_.increment_L - 1) << "]\n";
        ss << "  Memory usage: " << config_utils::format_memory(memory_usage()) << "\n";
        ss << "  Max counter value: " << max_value_ << "\n";
        return ss.str();
    }
};

}  // namespace globimap

#endif  // GLOBIMAP_VARIABLE_INCREMENT_BF_HPP
