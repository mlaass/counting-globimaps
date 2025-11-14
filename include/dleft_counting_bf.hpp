#ifndef GLOBIMAP_DLEFT_COUNTING_BF_HPP
#define GLOBIMAP_DLEFT_COUNTING_BF_HPP

#include "common/bf_config.hpp"
#include "common/bf_interface.hpp"
#include "hashfn.hpp"
#include <cassert>
#include <cstdint>
#include <limits>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace globimap {

/**
 * @brief Configuration for d-Left Counting Bloom Filter
 *
 * References:
 * Bonomi, F., Mitzenmacher, M., Panigrahy, R., Singh, S., Varghese, G. (2006).
 * An Improved Construction for Counting Bloom Filters. ESA 2006.
 * Paper name: "d-Left Counting Bloom Filter" or "d-Left CBF"
 */
struct DLeftCBFConfig {
    uint num_buckets;         // Total number of buckets (must be divisible by d)
    uint d;                   // Number of hash table segments (typically 3-4)
    uint slots_per_bucket;    // Slots in each bucket (typically 3-4)
    uint counter_bits;        // Bits per counter (typically 2-4)
    uint fingerprint_bits;    // Bits for fingerprint (typically 4-8)

    /**
     * @brief Validate configuration parameters
     */
    void validate() const {
        config_utils::validate_positive(num_buckets, "num_buckets");
        config_utils::validate_positive(d, "d");
        config_utils::validate_positive(slots_per_bucket, "slots_per_bucket");
        config_utils::validate_positive(counter_bits, "counter_bits");
        config_utils::validate_positive(fingerprint_bits, "fingerprint_bits");

        if (num_buckets % d != 0) {
            throw std::invalid_argument("num_buckets must be divisible by d");
        }

        if (d < 2 || d > 8) {
            throw std::invalid_argument("d should be in range [2, 8]");
        }

        if (counter_bits > 16) {
            throw std::invalid_argument("counter_bits should be <= 16 for d-Left CBF");
        }

        if (fingerprint_bits > 16) {
            throw std::invalid_argument("fingerprint_bits should be <= 16");
        }
    }
};

/**
 * @brief d-Left Counting Bloom Filter Implementation
 *
 * Uses d-left hashing with buckets and fingerprints for better cache locality
 * and memory efficiency. Each bucket contains multiple slots, and each slot
 * stores a fingerprint and a small counter.
 *
 * Key features:
 * - 50%+ memory savings vs. standard CBF (Bonomi et al., 2006)
 * - Better cache locality due to bucket structure
 * - Supports deletions natively
 * - Deterministic lookup (check exactly d buckets)
 *
 * Algorithm (Bonomi et al., 2006, Algorithm 1):
 * - Insert: Hash to d buckets, find leftmost with empty/matching slot, increment
 * - Query: Hash to d buckets, search for matching fingerprint, return count
 * - Delete: Find matching fingerprint, decrement counter
 *
 * @see DLeftCBFConfig for configuration parameters
 */
class DLeftCountingBloomFilter {
private:
    /**
     * @brief A single slot in a bucket (fingerprint + counter)
     */
    struct Slot {
        uint16_t fingerprint;  // Element fingerprint
        uint16_t counter;      // Count value
        bool occupied;         // Whether slot is in use

        Slot() : fingerprint(0), counter(0), occupied(false) {}
    };

    /**
     * @brief A bucket containing multiple slots
     */
    struct Bucket {
        std::vector<Slot> slots;

        explicit Bucket(uint num_slots) : slots(num_slots) {}
    };

    DLeftCBFConfig config_;
    uint buckets_per_segment_;     // num_buckets / d
    uint max_counter_value_;       // (1 << counter_bits) - 1
    uint fingerprint_mask_;        // (1 << fingerprint_bits) - 1

    std::vector<std::vector<Bucket>> segments_;  // d segments, each with buckets

    // Hash seeds for d segments
    std::vector<uint64_t> segment_seeds_;
    static constexpr uint64_t BASE_SEED = 0x9e3779b97f4a7c15ULL;
    static constexpr uint64_t FP_SEED = 0xc2b2ae3d27d4eb4fULL;  // For fingerprint

    /**
     * @brief Hash a point to get fingerprint and d bucket indices
     */
    void hash_point(const std::vector<uint64_t> &point,
                   uint16_t &fingerprint,
                   std::vector<uint> &bucket_indices) const {
        // Compute fingerprint
        uint64_t h_fp = FP_SEED;
        uint64_t h_fp2 = FP_SEED;
        hash(point.data(), point.size(), &h_fp, &h_fp2);
        fingerprint = static_cast<uint16_t>(h_fp & fingerprint_mask_);

        // Ensure fingerprint is never 0 (reserved for empty)
        if (fingerprint == 0) {
            fingerprint = 1;
        }

        // Compute bucket index in each segment
        bucket_indices.resize(config_.d);
        for (uint i = 0; i < config_.d; ++i) {
            uint64_t h1 = segment_seeds_[i];
            uint64_t h2 = segment_seeds_[i];
            hash(point.data(), point.size(), &h1, &h2);
            bucket_indices[i] = static_cast<uint>(h1 % buckets_per_segment_);
        }
    }

    /**
     * @brief Find slot with matching fingerprint in a bucket
     * @return Pointer to slot if found, nullptr otherwise
     */
    Slot* find_slot_in_bucket(Bucket &bucket, uint16_t fingerprint) {
        for (auto &slot : bucket.slots) {
            if (slot.occupied && slot.fingerprint == fingerprint) {
                return &slot;
            }
        }
        return nullptr;
    }

    /**
     * @brief Find an empty slot in a bucket
     * @return Pointer to empty slot if found, nullptr otherwise
     */
    Slot* find_empty_slot(Bucket &bucket) {
        for (auto &slot : bucket.slots) {
            if (!slot.occupied) {
                return &slot;
            }
        }
        return nullptr;
    }

public:
    /**
     * @brief Construct a d-Left CBF with given configuration
     */
    explicit DLeftCountingBloomFilter(const DLeftCBFConfig &conf) : config_(conf) {
        config_.validate();

        buckets_per_segment_ = config_.num_buckets / config_.d;
        max_counter_value_ = (1U << config_.counter_bits) - 1;
        fingerprint_mask_ = (1U << config_.fingerprint_bits) - 1;

        // Initialize d segments
        segments_.resize(config_.d);
        for (uint i = 0; i < config_.d; ++i) {
            segments_[i].reserve(buckets_per_segment_);
            for (uint j = 0; j < buckets_per_segment_; ++j) {
                segments_[i].emplace_back(config_.slots_per_bucket);
            }
        }

        // Initialize hash seeds for each segment
        segment_seeds_.resize(config_.d);
        for (uint i = 0; i < config_.d; ++i) {
            segment_seeds_[i] = BASE_SEED + (i * 0x123456789ABCDEFULL);
        }
    }

    /**
     * @brief Insert element (Bonomi et al., 2006, Algorithm 1)
     *
     * Algorithm:
     * 1. Compute fingerprint and d bucket positions
     * 2. Search d buckets from left to right for:
     *    - Matching fingerprint (increment if found)
     *    - Empty slot (insert new entry)
     * 3. If all buckets full, handle overflow
     *
     * Paper reference: Section 3, Algorithm 1
     */
    void put(const std::vector<uint64_t> &point) {
        uint16_t fingerprint;
        std::vector<uint> bucket_indices;
        hash_point(point, fingerprint, bucket_indices);

        // Try each segment from left to right (d-left hashing)
        for (uint i = 0; i < config_.d; ++i) {
            Bucket &bucket = segments_[i][bucket_indices[i]];

            // Check for matching fingerprint first
            Slot *existing_slot = find_slot_in_bucket(bucket, fingerprint);
            if (existing_slot != nullptr) {
                if (existing_slot->counter < max_counter_value_) {
                    existing_slot->counter++;
                }
                return;
            }

            // Check for empty slot
            Slot *empty_slot = find_empty_slot(bucket);
            if (empty_slot != nullptr) {
                empty_slot->occupied = true;
                empty_slot->fingerprint = fingerprint;
                empty_slot->counter = 1;
                return;
            }
        }

        // All buckets full - overflow handling
        // TODO: Verify exact behavior in Bonomi et al., 2006, Section 3
        // For now: increment counter in first bucket's first slot
        Bucket &first_bucket = segments_[0][bucket_indices[0]];
        if (first_bucket.slots[0].counter < max_counter_value_) {
            first_bucket.slots[0].counter++;
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
     * @brief Query count (Bonomi et al., 2006)
     *
     * Paper calls this "query(x)" or "getCount(x)"
     * Returns exact count if fingerprint found, 0 otherwise
     */
    uint64_t get_min(const std::vector<uint64_t> &point) const {
        uint16_t fingerprint;
        std::vector<uint> bucket_indices;
        const_cast<DLeftCountingBloomFilter*>(this)->hash_point(point, fingerprint, bucket_indices);

        // Search all d buckets for matching fingerprint
        for (uint i = 0; i < config_.d; ++i) {
            const Bucket &bucket = segments_[i][bucket_indices[i]];
            for (const auto &slot : bucket.slots) {
                if (slot.occupied && slot.fingerprint == fingerprint) {
                    return slot.counter;
                }
            }
        }

        return 0;  // Not found
    }

    /**
     * @brief Membership test
     */
    bool get_bool(const std::vector<uint64_t> &point) const {
        return get_min(point) > 0;
    }

    /**
     * @brief Delete element (decrement counter)
     *
     * Searches for matching fingerprint and decrements counter.
     * If counter reaches 0, marks slot as unoccupied.
     */
    void remove(const std::vector<uint64_t> &point) {
        uint16_t fingerprint;
        std::vector<uint> bucket_indices;
        hash_point(point, fingerprint, bucket_indices);

        // Search all d buckets for matching fingerprint
        for (uint i = 0; i < config_.d; ++i) {
            Bucket &bucket = segments_[i][bucket_indices[i]];
            for (auto &slot : bucket.slots) {
                if (slot.occupied && slot.fingerprint == fingerprint) {
                    if (slot.counter > 0) {
                        slot.counter--;
                        if (slot.counter == 0) {
                            slot.occupied = false;
                            slot.fingerprint = 0;
                        }
                    }
                    return;
                }
            }
        }
    }

    /**
     * @brief Calculate load factor (proportion of occupied slots)
     */
    double load_factor() const {
        uint total_slots = config_.num_buckets * config_.slots_per_bucket;
        uint occupied_slots = 0;

        for (const auto &segment : segments_) {
            for (const auto &bucket : segment) {
                for (const auto &slot : bucket.slots) {
                    if (slot.occupied) {
                        occupied_slots++;
                    }
                }
            }
        }

        return static_cast<double>(occupied_slots) / total_slots;
    }

    /**
     * @brief Get memory usage in bytes
     */
    uint64_t memory_usage() const {
        // Each slot: fingerprint (2 bytes) + counter (2 bytes) + occupied flag (~1 byte)
        // Approximation: 5 bytes per slot
        uint total_slots = config_.num_buckets * config_.slots_per_bucket;
        return total_slots * 5;  // Conservative estimate
    }

    /**
     * @brief Get summary statistics
     */
    std::string summary() {
        std::stringstream ss;
        ss << "d-Left Counting Bloom Filter\n";
        ss << "  Configuration:\n";
        ss << "    d (segments): " << config_.d << "\n";
        ss << "    Total buckets: " << config_.num_buckets << "\n";
        ss << "    Buckets per segment: " << buckets_per_segment_ << "\n";
        ss << "    Slots per bucket: " << config_.slots_per_bucket << "\n";
        ss << "    Total slots: " << (config_.num_buckets * config_.slots_per_bucket) << "\n";
        ss << "    Counter bits: " << config_.counter_bits << " (max value: " << max_counter_value_ << ")\n";
        ss << "    Fingerprint bits: " << config_.fingerprint_bits << "\n";
        ss << "  Memory usage: " << config_utils::format_memory(memory_usage()) << "\n";
        ss << "  Load factor: " << (load_factor() * 100.0) << "%\n";
        ss << "  Deletion support: Yes\n";
        ss << "  Cache locality: Excellent (bucket-based)\n";
        return ss.str();
    }
};

}  // namespace globimap

#endif  // GLOBIMAP_DLEFT_COUNTING_BF_HPP
