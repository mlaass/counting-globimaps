#ifndef GLOBIMAP_SPECTRAL_BLOOM_FILTER_HPP
#define GLOBIMAP_SPECTRAL_BLOOM_FILTER_HPP

#include "common/bf_config.hpp"
#include "common/bf_interface.hpp"
#include "hashfn.hpp"
#include <algorithm>
#include <cassert>
#include <cstdint>
#include <limits>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace globimap {

/**
 * @brief Spectral Bloom Filter variants
 *
 * References:
 * Cohen, S., & Matias, Y. (2003). Spectral Bloom Filters.
 * SIGMOD '03, 241-252.
 */
enum SBFVariant {
    MINIMUM_SELECTION,    // MS: Standard query returns min across k positions
    MINIMAL_INCREMENT,    // MI: Conservative update - only increment min counter(s)
    RECURRING_MINIMUM     // RM: Supports deletions with secondary filter
};

/**
 * @brief Configuration for Spectral Bloom Filter
 *
 * Paper names (Cohen & Matias, 2003):
 * - MS: "Minimum Selection"
 * - MI: "Minimal Increment" (conservative update from Estan & Varghese, 2002)
 * - RM: "Recurring Minimum"
 */
struct SBFConfig {
    uint hash_k;              // Number of hash functions
    uint logsize;             // Filter size: 2^logsize counters (power-of-2 mode)
    uint counter_bits;        // Bits per counter (8, 16, 32, or 64)
    SBFVariant variant;       // Which algorithm to use
    uint64_t exact_size = 0;  // Arbitrary size (takes precedence over logsize if > 0)

    /**
     * @brief Validate configuration parameters
     * @throws std::invalid_argument if parameters are invalid
     */
    void validate() const {
        config_utils::validate_positive(hash_k, "hash_k");
        if (exact_size == 0) {
            config_utils::validate_positive(logsize, "logsize");
        }
        config_utils::validate_range(counter_bits, 8, 64, "counter_bits");

        if (counter_bits != 8 && counter_bits != 16 &&
            counter_bits != 32 && counter_bits != 64) {
            throw std::invalid_argument("counter_bits must be 8, 16, 32, or 64");
        }
    }
};

/**
 * @brief Spectral Bloom Filter Implementation
 *
 * Extends bloom filters to multisets (frequency estimation). Three variants:
 *
 * 1. Minimum Selection (MS) - Section 2.1:
 *    - Insert: increment all k counters
 *    - Query: return min across k counters
 *
 * 2. Minimal Increment (MI) - Section 2.2:
 *    - Insert: find min, only increment counter(s) at min value
 *    - Query: return min across k counters
 *    - Provides "conservative update" to reduce overcounting
 *
 * 3. Recurring Minimum (RM) - Section 3:
 *    - Uses secondary min-values filter for deletion support
 *    - Insert: MI + record min value before increment
 *    - Delete: decrement if recorded min < current counter
 *
 * @see SBFConfig for configuration parameters
 */
class SpectralBloomFilter {
private:
    SBFConfig config_;
    uint64_t size_;           // Number of counters
    uint64_t mask_;           // size_ - 1, for fast modulo (power-of-2 mode only)
    uint64_t max_value_;      // Maximum counter value
    bool use_modulo_;         // true = use % size_, false = use & mask_

    // Primary counter storage (templated by bit depth)
    std::vector<uint8_t> counters8_;
    std::vector<uint16_t> counters16_;
    std::vector<uint32_t> counters32_;
    std::vector<uint64_t> counters64_;

    // Secondary min-values storage (RM variant only)
    std::vector<uint8_t> minvals8_;
    std::vector<uint16_t> minvals16_;
    std::vector<uint32_t> minvals32_;
    std::vector<uint64_t> minvals64_;

    // Hash seeds
    static constexpr uint64_t SEED1 = 0x9e3779b97f4a7c15ULL;
    static constexpr uint64_t SEED2 = 0x85ebca6b517d3b25ULL;
    static constexpr uint64_t SEED_MINVAL1 = 0xa5b6c8d9e1f20314ULL;  // For RM secondary hash
    static constexpr uint64_t SEED_MINVAL2 = 0xb7c9d2e4f5061728ULL;

    /**
     * @brief Hash a point to get two base hash values
     */
    inline void hash_point(const std::vector<uint64_t> &point,
                          uint64_t &h1, uint64_t &h2) const {
        h1 = SEED1;
        h2 = SEED2;
        hash(point.data(), point.size(), &h1, &h2);
    }

    /**
     * @brief Hash for secondary min-values filter (RM variant)
     */
    inline void hash_point_minval(const std::vector<uint64_t> &point,
                                  uint64_t &h1, uint64_t &h2) const {
        h1 = SEED_MINVAL1;
        h2 = SEED_MINVAL2;
        hash(point.data(), point.size(), &h1, &h2);
    }

    /**
     * @brief Calculate position from hash values
     * Uses modulo for arbitrary sizes, bitmask for power-of-2 sizes
     */
    inline uint64_t calc_pos(uint64_t h1, uint64_t h2, uint i) const {
        uint64_t h = h1 + (i + 1) * h2;
        return use_modulo_ ? (h % size_) : (h & mask_);
    }

    /**
     * @brief Get counter value at position
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
     * @brief Set counter value at position
     */
    inline void set_counter(uint64_t pos, uint64_t value) {
        switch (config_.counter_bits) {
            case 8:  counters8_[pos] = static_cast<uint8_t>(value); break;
            case 16: counters16_[pos] = static_cast<uint16_t>(value); break;
            case 32: counters32_[pos] = static_cast<uint32_t>(value); break;
            case 64: counters64_[pos] = value; break;
            default: assert(false);
        }
    }

    /**
     * @brief Increment counter at position by 1 (with overflow check)
     */
    inline void increment_counter(uint64_t pos) {
        uint64_t current = get_counter(pos);
        if (current < max_value_) {
            set_counter(pos, current + 1);
        }
    }

    /**
     * @brief Decrement counter at position by 1 (with underflow check)
     */
    inline void decrement_counter(uint64_t pos) {
        uint64_t current = get_counter(pos);
        if (current > 0) {
            set_counter(pos, current - 1);
        }
    }

    /**
     * @brief Get min-value at position (RM variant only)
     */
    inline uint64_t get_minval(uint64_t pos) const {
        switch (config_.counter_bits) {
            case 8:  return minvals8_[pos];
            case 16: return minvals16_[pos];
            case 32: return minvals32_[pos];
            case 64: return minvals64_[pos];
            default: assert(false); return 0;
        }
    }

    /**
     * @brief Set min-value at position (RM variant only)
     */
    inline void set_minval(uint64_t pos, uint64_t value) {
        switch (config_.counter_bits) {
            case 8:  minvals8_[pos] = static_cast<uint8_t>(value); break;
            case 16: minvals16_[pos] = static_cast<uint16_t>(value); break;
            case 32: minvals32_[pos] = static_cast<uint32_t>(value); break;
            case 64: minvals64_[pos] = value; break;
            default: assert(false);
        }
    }

public:
    /**
     * @brief Construct a Spectral Bloom Filter with given configuration
     */
    explicit SpectralBloomFilter(const SBFConfig &conf) : config_(conf) {
        config_.validate();

        if (config_.exact_size > 0) {
            // Arbitrary size mode (modulo indexing)
            size_ = config_.exact_size;
            use_modulo_ = true;
            mask_ = 0;  // Not used in modulo mode
        } else {
            // Power-of-2 mode (bitmask indexing)
            size_ = 1ULL << config_.logsize;
            use_modulo_ = false;
            mask_ = size_ - 1;
        }
        max_value_ = config_utils::max_counter_value(config_.counter_bits);

        // Allocate primary counter storage
        switch (config_.counter_bits) {
            case 8:  counters8_.resize(size_, 0);  break;
            case 16: counters16_.resize(size_, 0); break;
            case 32: counters32_.resize(size_, 0); break;
            case 64: counters64_.resize(size_, 0); break;
            default: assert(false);
        }

        // Allocate secondary storage for RM variant
        if (config_.variant == RECURRING_MINIMUM) {
            switch (config_.counter_bits) {
                case 8:  minvals8_.resize(size_, 0);  break;
                case 16: minvals16_.resize(size_, 0); break;
                case 32: minvals32_.resize(size_, 0); break;
                case 64: minvals64_.resize(size_, 0); break;
                default: assert(false);
            }
        }
    }

    /**
     * @brief Insert a single element (algorithm depends on variant)
     *
     * MS (Cohen & Matias, 2003, Section 2.1):
     *   For i = 1 to k: C[h_i(x)] += 1
     *
     * MI (Cohen & Matias, 2003, Section 2.2):
     *   m = min{C[h_i(x)] : i = 1..k}
     *   For i = 1 to k: if C[h_i(x)] == m: C[h_i(x)] += 1
     *
     * RM (Cohen & Matias, 2003, Section 3):
     *   m = min{C[h_i(x)] : i = 1..k}
     *   For i = 1 to k:
     *     if C[h_i(x)] == m:
     *       M[h'_i(x)] = m
     *       C[h_i(x)] += 1
     */
    void put(const std::vector<uint64_t> &point) {
        uint64_t h1, h2;
        hash_point(point, h1, h2);

        if (config_.variant == MINIMUM_SELECTION) {
            // MS: Simple increment all k positions
            for (uint i = 0; i < config_.hash_k; ++i) {
                uint64_t pos = calc_pos(h1, h2, i);
                increment_counter(pos);
            }
        } else if (config_.variant == MINIMAL_INCREMENT) {
            // MI: Conservative update - only increment minimum counter(s)
            // First pass: find minimum
            uint64_t min_count = UINT64_MAX;
            for (uint i = 0; i < config_.hash_k; ++i) {
                uint64_t pos = calc_pos(h1, h2, i);
                uint64_t count = get_counter(pos);
                if (count < min_count) {
                    min_count = count;
                }
            }

            // Second pass: increment only counters at minimum
            for (uint i = 0; i < config_.hash_k; ++i) {
                uint64_t pos = calc_pos(h1, h2, i);
                if (get_counter(pos) == min_count) {
                    increment_counter(pos);
                }
            }
        } else if (config_.variant == RECURRING_MINIMUM) {
            // RM: Conservative update + record min values for deletion
            uint64_t h1_min, h2_min;
            hash_point_minval(point, h1_min, h2_min);

            // First pass: find minimum
            uint64_t min_count = UINT64_MAX;
            for (uint i = 0; i < config_.hash_k; ++i) {
                uint64_t pos = calc_pos(h1, h2, i);
                uint64_t count = get_counter(pos);
                if (count < min_count) {
                    min_count = count;
                }
            }

            // Second pass: increment counters at minimum and record min value
            for (uint i = 0; i < config_.hash_k; ++i) {
                uint64_t pos = calc_pos(h1, h2, i);
                if (get_counter(pos) == min_count) {
                    uint64_t minval_pos = calc_pos(h1_min, h2_min, i);
                    set_minval(minval_pos, min_count);  // Record min before increment
                    increment_counter(pos);
                }
            }
        }
    }

    /**
     * @brief Batch insertion
     */
    void put_all(const std::vector<std::vector<uint64_t>> &points) {
        for (const auto &point : points) {
            put(point);
        }
    }

    /**
     * @brief Query frequency estimate
     *
     * All variants (Cohen & Matias, 2003):
     *   return min{C[h_i(x)] : i = 1..k}
     *
     * Paper calls this "query(x)" or "QUERY(x)"
     */
    uint64_t get_min(const std::vector<uint64_t> &point) {
        uint64_t h1, h2;
        hash_point(point, h1, h2);

        uint64_t min_count = UINT64_MAX;
        for (uint i = 0; i < config_.hash_k; ++i) {
            uint64_t pos = calc_pos(h1, h2, i);
            uint64_t count = get_counter(pos);
            if (count < min_count) {
                min_count = count;
            }
        }

        return min_count;
    }

    /**
     * @brief Membership test
     */
    bool get_bool(const std::vector<uint64_t> &point) {
        return get_min(point) > 0;
    }

    /**
     * @brief Delete element (RM variant only)
     *
     * RM deletion (Cohen & Matias, 2003, Section 3):
     *   For i = 1 to k:
     *     if M[h'_i(x)] < C[h_i(x)]:
     *       C[h_i(x)] -= 1
     *
     * @throws std::logic_error if called on non-RM variant
     */
    void remove(const std::vector<uint64_t> &point) {
        if (config_.variant != RECURRING_MINIMUM) {
            throw std::logic_error("remove() only supported in RECURRING_MINIMUM variant");
        }

        uint64_t h1, h2;
        hash_point(point, h1, h2);

        uint64_t h1_min, h2_min;
        hash_point_minval(point, h1_min, h2_min);

        for (uint i = 0; i < config_.hash_k; ++i) {
            uint64_t pos = calc_pos(h1, h2, i);
            uint64_t minval_pos = calc_pos(h1_min, h2_min, i);

            uint64_t recorded_min = get_minval(minval_pos);
            uint64_t current_count = get_counter(pos);

            if (recorded_min < current_count) {
                decrement_counter(pos);
            }
        }
    }

    /**
     * @brief Alternative estimator: mean across k counters
     *
     * TODO: Verify exact algorithm in Cohen & Matias (2003)
     * This is a simple mean estimator; paper may describe refinements
     */
    uint64_t get_mean(const std::vector<uint64_t> &point) {
        uint64_t h1, h2;
        hash_point(point, h1, h2);

        uint64_t sum = 0;
        for (uint i = 0; i < config_.hash_k; ++i) {
            uint64_t pos = calc_pos(h1, h2, i);
            sum += get_counter(pos);
        }

        return sum / config_.hash_k;
    }

    /**
     * @brief Get memory usage in bytes
     */
    uint64_t memory_usage() const {
        uint64_t primary_memory = 0;
        switch (config_.counter_bits) {
            case 8:  primary_memory = counters8_.size(); break;
            case 16: primary_memory = counters16_.size() * 2; break;
            case 32: primary_memory = counters32_.size() * 4; break;
            case 64: primary_memory = counters64_.size() * 8; break;
        }

        if (config_.variant == RECURRING_MINIMUM) {
            return primary_memory * 2;  // Secondary filter doubles memory
        }
        return primary_memory;
    }

    /**
     * @brief Get summary statistics
     */
    std::string summary() {
        std::stringstream ss;
        ss << "Spectral Bloom Filter\n";
        ss << "  Variant: ";
        switch (config_.variant) {
            case MINIMUM_SELECTION:
                ss << "Minimum Selection (MS)\n";
                break;
            case MINIMAL_INCREMENT:
                ss << "Minimal Increment (MI) - Conservative Update\n";
                break;
            case RECURRING_MINIMUM:
                ss << "Recurring Minimum (RM) - Deletion Support\n";
                break;
        }
        ss << "  Configuration:\n";
        ss << "    k (hash functions): " << config_.hash_k << "\n";
        if (use_modulo_) {
            ss << "    Filter size: " << size_ << " counters (exact size, modulo indexing)\n";
        } else {
            ss << "    Filter size: 2^" << config_.logsize << " = " << size_ << " counters\n";
        }
        ss << "    Counter bits: " << config_.counter_bits << " bits\n";
        ss << "  Memory usage: " << config_utils::format_memory(memory_usage());
        if (config_.variant == RECURRING_MINIMUM) {
            ss << " (primary + secondary filters)";
        }
        ss << "\n";
        ss << "  Max counter value: " << max_value_ << "\n";
        ss << "  Deletion support: " << (config_.variant == RECURRING_MINIMUM ? "Yes" : "No") << "\n";
        return ss.str();
    }

    /**
     * @brief Get current variant
     */
    SBFVariant get_variant() const {
        return config_.variant;
    }
};

}  // namespace globimap

#endif  // GLOBIMAP_SPECTRAL_BLOOM_FILTER_HPP
