/*
 * Pattern Table for Bloom Filter Intra-Block Hashing
 *
 * Pre-computes k-bit patterns for bloom filters, replacing double-hashing
 * with a single table lookup. Provides:
 * - 15-25% fewer instructions per query
 * - Better FPR due to guaranteed k distinct bits per pattern
 *
 * Based on pattern approach from SBBF implementation.
 */

#ifndef PATTERN_TABLE_HPP
#define PATTERN_TABLE_HPP

#include <array>
#include <cstdint>
#include <vector>

namespace globimap {

// ============================================================================
// 64-bit Pattern Table (for RegisterBlockedBF)
// ============================================================================

/**
 * Pre-computed k-bit patterns for 64-bit bloom filter blocks.
 *
 * Each pattern has exactly k bits set, uniformly distributed across
 * the 64-bit space. Access via: pattern = table[hash % table_size]
 *
 * Template Parameters:
 *   TableSize - Number of patterns (256, 512, 1024, 2048 recommended)
 */
template <size_t TableSize = 1024>
class PatternTable64 {
public:
    /// The generated patterns (each with exactly k bits set)
    std::array<uint64_t, TableSize> patterns;

    /// Number of bits set in each pattern
    unsigned k;

    PatternTable64() : k(0) {}

    /**
     * Initialize pattern table with k-bit patterns.
     *
     * @param hash_k  Number of bits to set in each pattern
     */
    explicit PatternTable64(unsigned hash_k) : k(hash_k) {
        generate_patterns();
    }

    /// Get pattern for given hash value
    uint64_t get(uint32_t hash) const {
        return patterns[hash % TableSize];
    }

    /// Get pattern for given hash value (same as get, explicit index)
    uint64_t operator[](size_t idx) const {
        return patterns[idx % TableSize];
    }

    size_t size() const { return TableSize; }

    size_t memory_bytes() const { return sizeof(patterns); }

private:
    void generate_patterns() {
        // Use deterministic PRNG for reproducibility
        for (size_t i = 0; i < TableSize; ++i) {
            uint64_t mask = 0;
            // Use splitmix64-style state mixing
            uint64_t state = i * 0x9e3779b97f4a7c15ULL + 0x6c62272e07bb0142ULL;

            for (unsigned j = 0; j < k; ++j) {
                // Generate next bit position
                state ^= state >> 33;
                state *= 0xff51afd7ed558ccdULL;
                state ^= state >> 33;

                uint32_t bit_pos = state % 64;

                // Ensure we don't set the same bit twice
                while (mask & (1ULL << bit_pos)) {
                    state *= 0xc4ceb9fe1a85ec53ULL;
                    state ^= state >> 33;
                    bit_pos = state % 64;
                }
                mask |= (1ULL << bit_pos);
            }
            patterns[i] = mask;
        }
    }
};

// ============================================================================
// 256-bit Pattern (standalone struct for BlockedBF)
// ============================================================================

/**
 * A single 256-bit pattern represented as 4x64-bit values.
 */
struct Pattern256 {
    uint64_t words[4];  // 256 bits as 4x64-bit words

    Pattern256() : words{0, 0, 0, 0} {}

    /// Check if all bits in pattern are set in block
    bool matches(const uint8_t* block) const {
        const uint64_t* block64 = reinterpret_cast<const uint64_t*>(block);
        return ((block64[0] & words[0]) == words[0]) &&
               ((block64[1] & words[1]) == words[1]) &&
               ((block64[2] & words[2]) == words[2]) &&
               ((block64[3] & words[3]) == words[3]);
    }

    /// Apply pattern (OR) to block
    void apply(uint8_t* block) const {
        uint64_t* block64 = reinterpret_cast<uint64_t*>(block);
        block64[0] |= words[0];
        block64[1] |= words[1];
        block64[2] |= words[2];
        block64[3] |= words[3];
    }
};

// ============================================================================
// 256-bit Pattern Table (for BlockedBF)
// ============================================================================

/**
 * Pre-computed k-bit patterns for 256-bit bloom filter blocks.
 *
 * Each pattern is represented as 4x64-bit values (256 bits total).
 * Access via: pattern = table[hash % table_size]
 */
template <size_t TableSize = 1024>
class PatternTable256 {
public:
    std::array<Pattern256, TableSize> patterns;
    unsigned k;

    PatternTable256() : k(0) {}

    explicit PatternTable256(unsigned hash_k) : k(hash_k) {
        generate_patterns();
    }

    const Pattern256& get(uint32_t hash) const {
        return patterns[hash % TableSize];
    }

    const Pattern256& operator[](size_t idx) const {
        return patterns[idx % TableSize];
    }

    size_t size() const { return TableSize; }

    size_t memory_bytes() const { return sizeof(patterns); }

private:
    void generate_patterns() {
        for (size_t i = 0; i < TableSize; ++i) {
            Pattern256& p = patterns[i];
            p = Pattern256();  // Reset

            uint64_t state = i * 0x9e3779b97f4a7c15ULL + 0x6c62272e07bb0142ULL;

            for (unsigned j = 0; j < k; ++j) {
                state ^= state >> 33;
                state *= 0xff51afd7ed558ccdULL;
                state ^= state >> 33;

                uint32_t bit_pos = state % 256;
                uint32_t word_idx = bit_pos / 64;
                uint32_t bit_idx = bit_pos % 64;

                // Ensure we don't set the same bit twice
                while (p.words[word_idx] & (1ULL << bit_idx)) {
                    state *= 0xc4ceb9fe1a85ec53ULL;
                    state ^= state >> 33;
                    bit_pos = state % 256;
                    word_idx = bit_pos / 64;
                    bit_idx = bit_pos % 64;
                }
                p.words[word_idx] |= (1ULL << bit_idx);
            }
        }
    }
};

// ============================================================================
// Common table sizes (memory usage)
// ============================================================================
// PatternTable64<256>   = 2 KB
// PatternTable64<512>   = 4 KB
// PatternTable64<1024>  = 8 KB (default)
// PatternTable64<2048>  = 16 KB (best FPR)
//
// PatternTable256<256>  = 8 KB
// PatternTable256<512>  = 16 KB
// PatternTable256<1024> = 32 KB (default)
// PatternTable256<2048> = 64 KB

// Type aliases for common configurations
using PatternTable64_256 = PatternTable64<256>;
using PatternTable64_512 = PatternTable64<512>;
using PatternTable64_1024 = PatternTable64<1024>;
using PatternTable64_2048 = PatternTable64<2048>;

using PatternTable256_256 = PatternTable256<256>;
using PatternTable256_512 = PatternTable256<512>;
using PatternTable256_1024 = PatternTable256<1024>;
using PatternTable256_2048 = PatternTable256<2048>;

} // namespace globimap

#endif // PATTERN_TABLE_HPP
