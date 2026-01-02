#ifndef COUNTING_GLOBIMAP_V2_HPP
#define COUNTING_GLOBIMAP_V2_HPP

#include "hashfn.hpp"
#include <algorithm>
#include <array>
#include <cassert>
#include <cstdint>
#include <limits>
#include <tuple>
#include <vector>

namespace globimap {

/**
 * @brief Byte-aligned layer with configurable value bits and overflow protocol
 *
 * Uses high bit as overflow flag, remaining bits for value.
 * When value bits overflow, sets overflow flag and wraps to 0.
 *
 * @tparam StorageType Underlying storage (uint16_t, uint32_t, uint64_t)
 * @tparam ValueBits Number of bits for value (e.g., 11 for 12-bit counter)
 */
template <typename StorageType, unsigned ValueBits>
struct TypedLayer {
    static_assert(ValueBits < sizeof(StorageType) * 8,
                  "ValueBits must be less than storage size");
    static_assert(std::is_unsigned_v<StorageType>,
                  "StorageType must be unsigned");

    std::vector<StorageType> counters;
    uint64_t mask;
    unsigned logsize;

    // Compile-time constants
    static constexpr unsigned value_bits = ValueBits;
    static constexpr unsigned total_bits = ValueBits + 1;
    static constexpr StorageType overflow_bit = StorageType(1) << ValueBits;
    static constexpr StorageType value_mask = overflow_bit - 1;
    static constexpr StorageType max_value = value_mask;

    TypedLayer() : mask(0), logsize(0) {}

    /**
     * @brief Resize layer to 2^logsize counters
     */
    void resize(unsigned new_logsize) {
        logsize = new_logsize;
        uint64_t size = uint64_t(1) << logsize;
        mask = size - 1;
        counters.resize(size, StorageType(0));
    }

    /**
     * @brief Get value bits (excluding overflow flag)
     */
    inline StorageType get_value(uint64_t pos) const {
        return counters[pos] & value_mask;
    }

    /**
     * @brief Check if position has overflowed to next layer
     */
    inline bool has_overflow(uint64_t pos) const {
        return counters[pos] & overflow_bit;
    }

    /**
     * @brief Get raw counter value (value + overflow bit)
     */
    inline StorageType get_raw(uint64_t pos) const {
        return counters[pos];
    }

    /**
     * @brief Increment counter at position
     * @return true if cascade to next layer is needed (overflow occurred)
     */
    inline bool increment(uint64_t pos) {
        StorageType raw = counters[pos];
        StorageType val = raw & value_mask;

        if (val < max_value) {
            // Normal increment: preserve overflow bit, increment value
            counters[pos] = (raw & overflow_bit) | (val + 1);
            return false;
        } else {
            // Overflow: wrap value to 0, set overflow bit
            counters[pos] = overflow_bit;
            return true;
        }
    }

    /**
     * @brief Memory usage in bytes
     */
    size_t byte_size() const {
        return counters.size() * sizeof(StorageType);
    }

    /**
     * @brief Number of counters
     */
    size_t size() const {
        return counters.size();
    }

    /**
     * @brief Clear all counters
     */
    void clear() {
        std::fill(counters.begin(), counters.end(), StorageType(0));
    }
};

// Common layer type aliases
using Layer8 = TypedLayer<uint8_t, 7>;      // 8-bit: 7 value + 1 overflow, max 127
using Layer12 = TypedLayer<uint16_t, 11>;   // 12-bit: 11 value + 1 overflow, max 2047
using Layer16 = TypedLayer<uint16_t, 15>;   // 16-bit: 15 value + 1 overflow, max 32767
using Layer20 = TypedLayer<uint32_t, 19>;   // 20-bit: 19 value + 1 overflow, max 524287
using Layer24 = TypedLayer<uint32_t, 23>;   // 24-bit: 23 value + 1 overflow, max 8388607
using Layer32 = TypedLayer<uint32_t, 31>;   // 32-bit: 31 value + 1 overflow
using Layer48 = TypedLayer<uint64_t, 47>;   // 48-bit: 47 value + 1 overflow
using Layer64 = TypedLayer<uint64_t, 63>;   // 64-bit: 63 value + 1 overflow

/**
 * @brief CountingGloBiMap V2 - Template-specialized multi-layer counting bloom filter
 *
 * Key improvements over V1:
 * 1. No memory waste - each layer has exactly one storage vector
 * 2. Compile-time dispatch - no runtime switch statements
 * 3. Overflow bit protocol - saturated counters continue contributing low bits
 * 4. Configurable value bits - supports 12-bit, 20-bit, etc.
 * 5. Asymmetric layer sizes - upper layers can be smaller for biased data
 *
 * @tparam Layers... TypedLayer types (e.g., Layer12, Layer20)
 */
template <typename... Layers>
class CountingGloBiMapV2 {
    static_assert(sizeof...(Layers) > 0, "At least one layer required");

public:
    static constexpr size_t num_layers = sizeof...(Layers);

private:
    std::tuple<Layers...> layers_;
    unsigned hash_k_;
    bool minimal_increment_;

    // Hash seeds
    static constexpr uint64_t SEED1 = 8589845122ULL;
    static constexpr uint64_t SEED2 = 8465418721ULL;

    // Cache for minimal increment mode
    struct HashPosition {
        size_t layer_idx;
        uint64_t position;
        uint64_t sum;
    };

public:
    CountingGloBiMapV2() : hash_k_(0), minimal_increment_(false) {}

    /**
     * @brief Configure filter with hash count and layer sizes
     *
     * @param hash_k Number of hash functions
     * @param logsizes Array of log2(layer_size) for each layer
     * @param minimal_increment Use conservative update (MI mode)
     */
    void configure(unsigned hash_k,
                   const std::array<unsigned, num_layers>& logsizes,
                   bool minimal_increment = false) {
        hash_k_ = hash_k;
        minimal_increment_ = minimal_increment;

        // Resize each layer
        size_t idx = 0;
        std::apply([&](auto&... layer) {
            ((layer.resize(logsizes[idx++])), ...);
        }, layers_);
    }

    /**
     * @brief Insert a point (increment counters)
     */
    void put(const std::vector<uint64_t>& point) {
        putp(point.data(), point.size());
    }

    /**
     * @brief Insert a point with raw pointer
     */
    void putp(const uint64_t* point, size_t len) {
        uint64_t h1 = SEED1, h2 = SEED2;
        hash(point, len, &h1, &h2);

        if (minimal_increment_) {
            put_minimal_increment(h1, h2);
        } else {
            put_standard(h1, h2);
        }
    }

    /**
     * @brief Query minimum count for a point
     */
    uint64_t get_min(const std::vector<uint64_t>& point) const {
        return get_minp(point.data(), point.size());
    }

    /**
     * @brief Query minimum count with raw pointer
     */
    uint64_t get_minp(const uint64_t* point, size_t len) const {
        uint64_t h1 = SEED1, h2 = SEED2;
        hash(point, len, &h1, &h2);

        uint64_t min_count = std::numeric_limits<uint64_t>::max();

        for (unsigned i = 0; i < hash_k_; ++i) {
            uint64_t total = 0;
            unsigned shift = 0;
            bool found_zero = false;

            // Traverse layers, accumulating bit-shifted values
            std::apply([&](const auto&... layer) {
                bool should_continue = true;
                ((should_continue && !found_zero ? [&]() {
                    uint64_t pos = (h1 + (i + 1) * h2) & layer.mask;
                    auto val = layer.get_value(pos);

                    if (val == 0 && !layer.has_overflow(pos)) {
                        // Zero in non-overflowed layer means point not present
                        found_zero = true;
                        return true;
                    }

                    total += static_cast<uint64_t>(val) << shift;
                    shift += layer.value_bits;
                    should_continue = layer.has_overflow(pos);
                    return true;
                }() : false), ...);
            }, layers_);

            if (found_zero) {
                return 0;
            }

            min_count = std::min(min_count, total);
        }

        return min_count;
    }

    /**
     * @brief Check if point is present (boolean query)
     */
    bool get_bool(const std::vector<uint64_t>& point) const {
        return get_min(point) > 0;
    }

    /**
     * @brief Total memory usage in bytes
     */
    size_t memory_usage() const {
        size_t total = 0;
        std::apply([&](const auto&... layer) {
            ((total += layer.byte_size()), ...);
        }, layers_);
        return total;
    }

    /**
     * @brief Clear all counters
     */
    void clear() {
        std::apply([](auto&... layer) {
            ((layer.clear()), ...);
        }, layers_);
    }

    /**
     * @brief Get number of hash functions
     */
    unsigned hash_k() const { return hash_k_; }

    /**
     * @brief Check if minimal increment mode is enabled
     */
    bool is_minimal_increment() const { return minimal_increment_; }

    /**
     * @brief Get layer count
     */
    static constexpr size_t layer_count() { return num_layers; }

    /**
     * @brief Access layer by index (for debugging/stats)
     */
    template <size_t I>
    const auto& get_layer() const {
        return std::get<I>(layers_);
    }

private:
    /**
     * @brief Standard put: increment all k positions
     */
    void put_standard(uint64_t h1, uint64_t h2) {
        for (unsigned i = 0; i < hash_k_; ++i) {
            // Cascade through layers until one doesn't overflow
            std::apply([&](auto&... layer) {
                bool cascaded = true;
                ((cascaded ? [&]() {
                    uint64_t pos = (h1 + (i + 1) * h2) & layer.mask;
                    cascaded = layer.increment(pos);
                    return true;
                }() : false), ...);
            }, layers_);
        }
    }

    /**
     * @brief Minimal increment put: only increment positions at minimum sum
     *
     * Two-pass algorithm:
     * 1. Compute sum for each hash position
     * 2. Increment (with cascade) only positions at minimum
     */
    void put_minimal_increment(uint64_t h1, uint64_t h2) {
        // First pass: compute sums and find minimum
        std::array<uint64_t, 64> sums;  // Max k=64
        uint64_t min_sum = std::numeric_limits<uint64_t>::max();

        for (unsigned i = 0; i < hash_k_; ++i) {
            uint64_t sum = 0;
            unsigned shift = 0;

            // Traverse layers to compute total sum
            std::apply([&](const auto&... layer) {
                bool should_continue = true;
                ((should_continue ? [&]() {
                    uint64_t pos = (h1 + (i + 1) * h2) & layer.mask;
                    sum += static_cast<uint64_t>(layer.get_value(pos)) << shift;
                    shift += layer.value_bits;
                    should_continue = layer.has_overflow(pos);
                    return true;
                }() : false), ...);
            }, layers_);

            sums[i] = sum;
            min_sum = std::min(min_sum, sum);
        }

        // Second pass: cascade increment for positions at minimum
        for (unsigned i = 0; i < hash_k_; ++i) {
            if (sums[i] == min_sum) {
                // Cascade through layers (same as standard mode)
                std::apply([&](auto&... layer) {
                    bool cascaded = true;
                    ((cascaded ? [&]() {
                        uint64_t pos = (h1 + (i + 1) * h2) & layer.mask;
                        cascaded = layer.increment(pos);
                        return true;
                    }() : false), ...);
                }, layers_);
            }
        }
    }
};

// Common configurations
using CGM_8 = CountingGloBiMapV2<Layer8>;
using CGM_12 = CountingGloBiMapV2<Layer12>;
using CGM_12_20 = CountingGloBiMapV2<Layer12, Layer20>;
using CGM_12_20_32 = CountingGloBiMapV2<Layer12, Layer20, Layer32>;
using CGM_16_32 = CountingGloBiMapV2<Layer16, Layer32>;

// Recommended default: 12+20 bit, 2 layers
using DefaultCountingGloBiMap = CGM_12_20;

} // namespace globimap

#endif // COUNTING_GLOBIMAP_V2_HPP
