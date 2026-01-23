#ifndef CASCADE_CBF_HPP
#define CASCADE_CBF_HPP

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
    uint64_t size_ = 0;       // Actual size (for both modes)
    bool use_modulo_ = false; // true = modulo indexing, false = bitmask indexing

    // Compile-time constants
    static constexpr unsigned value_bits = ValueBits;
    static constexpr unsigned total_bits = ValueBits + 1;
    static constexpr StorageType overflow_bit = StorageType(1) << ValueBits;
    static constexpr StorageType value_mask = overflow_bit - 1;
    static constexpr StorageType max_value = value_mask;

    TypedLayer() : mask(0), logsize(0), size_(0), use_modulo_(false) {}

    /**
     * @brief Resize layer to 2^logsize counters (power-of-2 mode)
     */
    void resize(unsigned new_logsize) {
        logsize = new_logsize;
        size_ = uint64_t(1) << logsize;
        mask = size_ - 1;
        use_modulo_ = false;
        counters.resize(size_, StorageType(0));
    }

    /**
     * @brief Resize layer to exact size (arbitrary size mode)
     */
    void resize_exact(uint64_t exact_size) {
        size_ = exact_size;
        use_modulo_ = true;
        mask = 0;  // Not used in modulo mode
        logsize = 0;
        counters.resize(size_, StorageType(0));
    }

    /**
     * @brief Calculate position from hash value
     */
    inline uint64_t calc_pos(uint64_t h) const {
        return use_modulo_ ? (h % size_) : (h & mask);
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
        return size_;
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
class CascadeCBF {
    static_assert(sizeof...(Layers) > 0, "At least one layer required");

public:
    static constexpr size_t num_layers = sizeof...(Layers);

private:
    std::tuple<Layers...> layers_;
    unsigned hash_k_;
    bool minimal_increment_;
    bool use_independent_hashes_;

    // Base seeds for hashing
    static constexpr uint64_t SEED1 = 8589845122ULL;
    static constexpr uint64_t SEED2 = 8465418721ULL;
    // Seed multiplier for generating independent hash seeds (same as CMS)
    static constexpr uint64_t SEED_STEP = 0x123456789ABCDEFULL;

    // Maximum supported k value
    static constexpr unsigned MAX_K = 64;

    // =========================================================================
    // Compile-time bit shift computation for each layer
    // =========================================================================

    template <size_t LayerIdx>
    static constexpr unsigned bit_shift_for_layer() {
        if constexpr (LayerIdx == 0) {
            return 0;
        } else {
            using PrevLayer = std::tuple_element_t<LayerIdx - 1, std::tuple<Layers...>>;
            return bit_shift_for_layer<LayerIdx - 1>() + PrevLayer::value_bits;
        }
    }

    // =========================================================================
    // Compile-time layer access helpers
    // =========================================================================

    template <size_t I>
    auto& layer_mut() { return std::get<I>(layers_); }

    template <size_t I>
    const auto& layer_ref() const { return std::get<I>(layers_); }

    // =========================================================================
    // Explicit cascade increment using compile-time recursion
    // =========================================================================

    template <size_t LayerIdx = 0>
    inline void cascade_increment_at(uint64_t hash_base, uint64_t hash_mult, unsigned k_idx) {
        if constexpr (LayerIdx < num_layers) {
            auto& layer = std::get<LayerIdx>(layers_);
            uint64_t h = hash_base + (k_idx + 1) * hash_mult;
            uint64_t pos = layer.calc_pos(h);
            bool overflow = layer.increment(pos);
            if (overflow) {
                cascade_increment_at<LayerIdx + 1>(hash_base, hash_mult, k_idx);
            }
        }
    }

    // Version for independent hash mode (uses h1 directly)
    template <size_t LayerIdx = 0>
    inline void cascade_increment_direct(uint64_t h1) {
        if constexpr (LayerIdx < num_layers) {
            auto& layer = std::get<LayerIdx>(layers_);
            uint64_t pos = layer.calc_pos(h1);
            bool overflow = layer.increment(pos);
            if (overflow) {
                cascade_increment_direct<LayerIdx + 1>(h1);
            }
        }
    }

    // =========================================================================
    // Compute sum across all layers for a position (compile-time recursion)
    // =========================================================================

    template <size_t LayerIdx = 0>
    inline uint64_t compute_sum_at(uint64_t hash_base, uint64_t hash_mult, unsigned k_idx) const {
        if constexpr (LayerIdx < num_layers) {
            const auto& layer = std::get<LayerIdx>(layers_);
            uint64_t h = hash_base + (k_idx + 1) * hash_mult;
            uint64_t pos = layer.calc_pos(h);
            auto val = layer.get_value(pos);
            constexpr unsigned shift = bit_shift_for_layer<LayerIdx>();
            uint64_t contribution = static_cast<uint64_t>(val) << shift;

            if (layer.has_overflow(pos)) {
                return contribution + compute_sum_at<LayerIdx + 1>(hash_base, hash_mult, k_idx);
            }
            return contribution;
        }
        return 0;
    }

    // Version for independent hash mode
    template <size_t LayerIdx = 0>
    inline uint64_t compute_sum_direct(uint64_t h1) const {
        if constexpr (LayerIdx < num_layers) {
            const auto& layer = std::get<LayerIdx>(layers_);
            uint64_t pos = layer.calc_pos(h1);
            auto val = layer.get_value(pos);
            constexpr unsigned shift = bit_shift_for_layer<LayerIdx>();
            uint64_t contribution = static_cast<uint64_t>(val) << shift;

            if (layer.has_overflow(pos)) {
                return contribution + compute_sum_direct<LayerIdx + 1>(h1);
            }
            return contribution;
        }
        return 0;
    }

    // =========================================================================
    // Query: check if any layer has zero without overflow (early exit)
    // Returns pair<sum, is_zero>
    // =========================================================================

    template <size_t LayerIdx = 0>
    inline std::pair<uint64_t, bool> query_sum_at(uint64_t hash_base, uint64_t hash_mult,
                                                  unsigned k_idx) const {
        if constexpr (LayerIdx < num_layers) {
            const auto& layer = std::get<LayerIdx>(layers_);
            uint64_t h = hash_base + (k_idx + 1) * hash_mult;
            uint64_t pos = layer.calc_pos(h);
            auto val = layer.get_value(pos);

            // Early exit: zero in non-overflowed layer means point not present
            if (val == 0 && !layer.has_overflow(pos)) {
                return {0, true};  // is_zero = true
            }

            constexpr unsigned shift = bit_shift_for_layer<LayerIdx>();
            uint64_t contribution = static_cast<uint64_t>(val) << shift;

            if (layer.has_overflow(pos)) {
                auto [rest, is_zero] = query_sum_at<LayerIdx + 1>(hash_base, hash_mult, k_idx);
                return {contribution + rest, is_zero};
            }
            return {contribution, false};
        }
        return {0, false};
    }

    // Version for independent hash mode
    template <size_t LayerIdx = 0>
    inline std::pair<uint64_t, bool> query_sum_direct(uint64_t h1) const {
        if constexpr (LayerIdx < num_layers) {
            const auto& layer = std::get<LayerIdx>(layers_);
            uint64_t pos = layer.calc_pos(h1);
            auto val = layer.get_value(pos);

            if (val == 0 && !layer.has_overflow(pos)) {
                return {0, true};
            }

            constexpr unsigned shift = bit_shift_for_layer<LayerIdx>();
            uint64_t contribution = static_cast<uint64_t>(val) << shift;

            if (layer.has_overflow(pos)) {
                auto [rest, is_zero] = query_sum_direct<LayerIdx + 1>(h1);
                return {contribution + rest, is_zero};
            }
            return {contribution, false};
        }
        return {0, false};
    }

public:
    CascadeCBF() : hash_k_(0), minimal_increment_(false), use_independent_hashes_(false) {}

    /**
     * @brief Configure filter with hash count and layer sizes (power-of-2 mode)
     *
     * @param hash_k Number of hash functions
     * @param logsizes Array of log2(layer_size) for each layer
     * @param minimal_increment Use conservative update (MI mode)
     * @param independent_hashes Use independent hash seeds per k (slower but better for skewed data)
     */
    void configure(unsigned hash_k,
                   const std::array<unsigned, num_layers>& logsizes,
                   bool minimal_increment = false,
                   bool independent_hashes = false) {
        hash_k_ = hash_k;
        minimal_increment_ = minimal_increment;
        use_independent_hashes_ = independent_hashes;

        // Resize each layer
        size_t idx = 0;
        std::apply([&](auto&... layer) {
            ((layer.resize(logsizes[idx++])), ...);
        }, layers_);
    }

    /**
     * @brief Configure filter with exact layer sizes (arbitrary size mode)
     *
     * Uses modulo indexing instead of bitmask for arbitrary memory budgets.
     * Note: Modulo is slower than bitmask; use this for fair memory comparisons,
     * not performance benchmarks.
     *
     * @param hash_k Number of hash functions
     * @param exact_sizes Array of exact counter counts for each layer
     * @param minimal_increment Use conservative update (MI mode)
     * @param independent_hashes Use independent hash seeds per k
     */
    void configure_exact(unsigned hash_k,
                         const std::array<uint64_t, num_layers>& exact_sizes,
                         bool minimal_increment = false,
                         bool independent_hashes = false) {
        hash_k_ = hash_k;
        minimal_increment_ = minimal_increment;
        use_independent_hashes_ = independent_hashes;

        // Resize each layer with exact sizes
        size_t idx = 0;
        std::apply([&](auto&... layer) {
            ((layer.resize_exact(exact_sizes[idx++])), ...);
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
        if (use_independent_hashes_) {
            // Independent hash mode: compute separate hash for each k
            if (minimal_increment_) {
                put_minimal_increment_independent(point, len);
            } else {
                put_standard_independent(point, len);
            }
        } else {
            // Hashing trick mode: h1 + (i+1)*h2
            uint64_t h1 = SEED1, h2 = SEED2;
            hash(point, len, &h1, &h2);

            if (minimal_increment_) {
                put_minimal_increment(h1, h2);
            } else {
                put_standard(h1, h2);
            }
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
     * Uses compile-time recursion with early exit (no std::apply overhead)
     */
    uint64_t get_minp(const uint64_t* point, size_t len) const {
        if (use_independent_hashes_) {
            return get_minp_independent(point, len);
        }

        // Hashing trick mode
        uint64_t h1 = SEED1, h2 = SEED2;
        hash(point, len, &h1, &h2);

        uint64_t min_count = std::numeric_limits<uint64_t>::max();

        for (unsigned i = 0; i < hash_k_; ++i) {
            auto [total, is_zero] = query_sum_at<0>(h1, h2, i);
            if (is_zero) {
                return 0;  // Early exit: point definitely not present
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
     * @brief Check if independent hash mode is enabled
     */
    bool uses_independent_hashes() const { return use_independent_hashes_; }

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
     * Uses compile-time recursion for cascade (no std::apply overhead)
     */
    void put_standard(uint64_t h1, uint64_t h2) {
        for (unsigned i = 0; i < hash_k_; ++i) {
            cascade_increment_at<0>(h1, h2, i);
        }
    }

    /**
     * @brief Minimal increment put: only increment positions at minimum sum
     * Uses compile-time recursion (no std::apply overhead)
     *
     * Two-pass algorithm:
     * 1. Compute sum for each hash position
     * 2. Increment (with cascade) only positions at minimum
     */
    void put_minimal_increment(uint64_t h1, uint64_t h2) {
        // First pass: compute sums and find minimum
        std::array<uint64_t, MAX_K> sums;
        uint64_t min_sum = std::numeric_limits<uint64_t>::max();

        for (unsigned i = 0; i < hash_k_; ++i) {
            sums[i] = compute_sum_at<0>(h1, h2, i);
            min_sum = std::min(min_sum, sums[i]);
        }

        // Second pass: cascade increment for positions at minimum
        for (unsigned i = 0; i < hash_k_; ++i) {
            if (sums[i] == min_sum) {
                cascade_increment_at<0>(h1, h2, i);
            }
        }
    }

    // =========================================================================
    // Independent hash mode methods (truly independent hashes per k position)
    // =========================================================================

    /**
     * @brief Compute hash for k-th position using independent seed
     */
    inline void compute_hash_k(const uint64_t* point, size_t len, unsigned k,
                               uint64_t* h1, uint64_t* h2) const {
        *h1 = SEED1 + k * SEED_STEP;
        *h2 = SEED2;
        hash(point, len, h1, h2);
    }

    /**
     * @brief Standard put with independent hashes
     * Uses compile-time recursion (no std::apply overhead)
     */
    void put_standard_independent(const uint64_t* point, size_t len) {
        for (unsigned i = 0; i < hash_k_; ++i) {
            uint64_t h1, h2;
            compute_hash_k(point, len, i, &h1, &h2);
            cascade_increment_direct<0>(h1);
        }
    }

    /**
     * @brief Minimal increment put with independent hashes
     * Uses compile-time recursion (no std::apply overhead)
     */
    void put_minimal_increment_independent(const uint64_t* point, size_t len) {
        // Pre-compute all hashes
        std::array<uint64_t, MAX_K> hashes;
        for (unsigned i = 0; i < hash_k_; ++i) {
            uint64_t h1, h2;
            compute_hash_k(point, len, i, &h1, &h2);
            hashes[i] = h1;
        }

        // First pass: compute sums and find minimum
        std::array<uint64_t, MAX_K> sums;
        uint64_t min_sum = std::numeric_limits<uint64_t>::max();

        for (unsigned i = 0; i < hash_k_; ++i) {
            sums[i] = compute_sum_direct<0>(hashes[i]);
            min_sum = std::min(min_sum, sums[i]);
        }

        // Second pass: cascade increment for positions at minimum
        for (unsigned i = 0; i < hash_k_; ++i) {
            if (sums[i] == min_sum) {
                cascade_increment_direct<0>(hashes[i]);
            }
        }
    }

    /**
     * @brief Query minimum count with independent hashes
     * Uses compile-time recursion with early exit (no std::apply overhead)
     */
    uint64_t get_minp_independent(const uint64_t* point, size_t len) const {
        uint64_t min_count = std::numeric_limits<uint64_t>::max();

        for (unsigned i = 0; i < hash_k_; ++i) {
            uint64_t h1, h2;
            compute_hash_k(point, len, i, &h1, &h2);

            auto [total, is_zero] = query_sum_direct<0>(h1);
            if (is_zero) {
                return 0;
            }
            min_count = std::min(min_count, total);
        }

        return min_count;
    }
};

// Common configurations (legacy - wastes bits in storage)
using CCBF_8 = CascadeCBF<Layer8>;
using CCBF_12 = CascadeCBF<Layer12>;
using CCBF_12_20 = CascadeCBF<Layer12, Layer20>;
using CCBF_12_20_32 = CascadeCBF<Layer12, Layer20, Layer32>;
using CCBF_16_32 = CascadeCBF<Layer16, Layer32>;

// Efficient configurations (full utilization of storage bits)
using CCBF_16 = CascadeCBF<Layer16>;              // Single 16-bit layer, max 32767
using CCBF_16_16 = CascadeCBF<Layer16, Layer16>;  // Two 16-bit layers, 30 value bits total

// Recommended default: 16+16 bit, 2 layers (38% less memory than 32-bit at same accuracy)
using DefaultCascadeCBF = CCBF_16_16;

} // namespace globimap

#endif // CASCADE_CBF_HPP
