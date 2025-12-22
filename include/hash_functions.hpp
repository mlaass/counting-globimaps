/*
 * Template Hash Function Wrappers
 *
 * Provides zero-overhead compile-time hash function selection via templates.
 * Use as template parameter to bloom filter classes:
 *
 *   RegisterBlockedBloomFilter<XXH3Hasher> fast_bf(...);
 *   BlockedBloomFilter<MurmurHasher> compat_bf(...);
 *
 * Available hashers:
 * - MurmurHasher: MurmurHash3 128-bit (default, best compatibility)
 * - XXH3Hasher: XXH3 128-bit (1.5x faster, vectorized)
 * - WyHasher: wyhash 64-bit x2 (1.2x faster, simple)
 */

#ifndef HASH_FUNCTIONS_HPP
#define HASH_FUNCTIONS_HPP

#include "murmur.hpp"

#define XXH_INLINE_ALL
#include "xxhash.h"

#include "wyhash.h"

#include <cstdint>
#include <cstddef>

namespace globimap {

// ============================================================================
// MurmurHash3 Hasher (Default)
// ============================================================================

/**
 * MurmurHash3 128-bit hasher.
 *
 * The original default hash function. Good distribution, widely tested.
 * Throughput: ~3-5 GB/s on modern CPUs.
 */
struct MurmurHasher {
    static constexpr const char* name = "MurmurHash3";

    static void hash(const uint64_t* data, size_t len,
                     uint64_t* v1, uint64_t* v2) {
        uint64_t out[2];
        murmur::MurmurHash3_x64_128(data, len * sizeof(uint64_t), *v1, out);
        *v1 = out[0];
        *v2 = out[1];
    }
};

// ============================================================================
// XXH3 Hasher (Fast)
// ============================================================================

/**
 * XXH3 128-bit hasher.
 *
 * Vectorized hash function using SSE2/AVX2 when available.
 * 1.5-2x faster than MurmurHash3 for typical bloom filter inputs.
 * Throughput: ~30+ GB/s on modern CPUs with SIMD.
 */
struct XXH3Hasher {
    static constexpr const char* name = "XXH3";

    static void hash(const uint64_t* data, size_t len,
                     uint64_t* v1, uint64_t* v2) {
        XXH128_hash_t h = XXH3_128bits_withSeed(data, len * sizeof(uint64_t), *v1);
        *v1 = h.low64;
        *v2 = h.high64;
    }
};

// ============================================================================
// wyhash Hasher (Simple & Fast)
// ============================================================================

/**
 * wyhash 64-bit hasher (called twice for two hashes).
 *
 * Very simple and fast hash function. Good for small inputs.
 * 1.2x faster than MurmurHash3 for typical bloom filter inputs.
 * Throughput: ~25 GB/s on modern CPUs.
 */
struct WyHasher {
    static constexpr const char* name = "wyhash";

    static void hash(const uint64_t* data, size_t len,
                     uint64_t* v1, uint64_t* v2) {
        *v1 = wyhash(data, len * sizeof(uint64_t), *v1, _wyp);
        *v2 = wyhash(data, len * sizeof(uint64_t), *v2, _wyp);
    }
};

// ============================================================================
// Default Hasher
// ============================================================================

/**
 * Default hasher for backward compatibility.
 * MurmurHash3 is chosen for its stability and wide testing.
 */
using DefaultHasher = MurmurHasher;

} // namespace globimap

#endif // HASH_FUNCTIONS_HPP
