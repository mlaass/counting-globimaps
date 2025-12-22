/*
 * DEPRECATED: Use hash_functions.hpp with template hashers instead.
 *
 * This file uses #define guards which only compile ONE hash function at a time.
 * The new hash_functions.hpp provides template-based hashers with zero overhead:
 *
 *   #include "hash_functions.hpp"
 *   BlockedBloomFilter<XXH3Hasher> bf(...);      // Use XXH3
 *   BlockedBloomFilter<MurmurHasher> bf2(...);   // Use MurmurHash3
 *   BlockedBloomFilter<> bf3(...);               // Default (MurmurHash3)
 *
 * This file is maintained only for backward compatibility with:
 * - globimap.hpp (binary bloom filter)
 * - counting_globimap.hpp (counting filter)
 * - Other legacy code using the hash() function directly
 */

#ifndef HASHFN_HPP
#define HASHFN_HPP

// =============================================================================
// Hash function selection (uncomment ONE):
// =============================================================================
// GLOBIMAP_USE_MURMUR  - MurmurHash3 (3-5 GB/s) - Default, widely compatible
// GLOBIMAP_USE_XXH3    - XXH3 (30+ GB/s) - Fastest, auto-vectorizes (SSE2/AVX2)
// GLOBIMAP_USE_WYHASH  - wyhash (25 GB/s) - Simple, excellent for short keys
// =============================================================================

#define GLOBIMAP_USE_MURMUR
// #define GLOBIMAP_USE_XXH3
// #define GLOBIMAP_USE_WYHASH

//#define GLOBIMAP_USE_MURMUR_PREFIX
//#define GLOBIMAP_USE_DJB64

#include <cstring>

// =============================================================================
// XXH3 - Extremely fast hash (30+ GB/s with vectorization)
// =============================================================================
#ifdef GLOBIMAP_USE_XXH3
#define XXH_INLINE_ALL
#include "xxhash.h"

inline void hash(const uint64_t *data, const size_t len, uint64_t *v1,
                 uint64_t *v2) {
  // XXH3_128bits produces two independent 64-bit hash values
  XXH128_hash_t h = XXH3_128bits_withSeed(data, len * sizeof(uint64_t), *v1);
  *v1 = h.low64;
  *v2 = h.high64;
}
#endif

// =============================================================================
// wyhash - Fast and simple hash (25 GB/s)
// =============================================================================
#ifdef GLOBIMAP_USE_WYHASH
#include "wyhash.h"

inline void hash(const uint64_t *data, const size_t len, uint64_t *v1,
                 uint64_t *v2) {
  // wyhash produces a single 64-bit value; use different seeds for h1/h2
  *v1 = wyhash(data, len * sizeof(uint64_t), *v1, _wyp);
  *v2 = wyhash(data, len * sizeof(uint64_t), *v2, _wyp);
}
#endif

// =============================================================================
// MurmurHash3 - Classic hash (3-5 GB/s) - Default
// =============================================================================
#ifdef GLOBIMAP_USE_MURMUR
#include "murmur.hpp"

#ifdef GLOBIMAP_USE_MURMUR_PREFIX
inline void hash(const uint64_t *data, size_t len, uint64_t *v1, uint64_t *v2,
                 const char *prefix = "") {
  uint64_t hash[2];
  char buffer[1024];
  snprintf(buffer, 1024, "%s-%lu-%lu", prefix, data[0], data[1]);
  murmur::MurmurHash3_x86_128(data, strlen(buffer), *v1,
                              (void *)hash); // len * sizeof(uint64_t)
  *v1 = hash[0];
  *v2 = hash[1];
}
#else
inline void hash(const uint64_t *data, const size_t len, uint64_t *v1,
                 uint64_t *v2) {
  uint64_t hash[2];
  /*    char buffer[1024];
      snprintf(buffer, 1024,"%s-%lu-%lu", prefix,data[0],data[1]);*/
  murmur::MurmurHash3_x64_128(data, len * sizeof(uint64_t), *v1,
                              (void *)hash);
  *v1 = hash[0];
  *v2 = hash[1];
}
#endif
#endif

#ifdef GLOBIMAP_USE_DJB64

// This should not be used, it would need careful evaluation on many layers.
// Stays here for completeness!!!

inline void hash(uint32_t *data, size_t len, uint32_t *v1, uint32_t *v2) {
  const char *prefix1 = "p1";
  const char *prefix2 = "x8";
  char *str = reinterpret_cast<char *>(data);
  uint32_t hash1 = 5381;
  uint32_t hash2 = 5381;
  for (size_t i = 0; i < 2; i++) {
    hash1 = ((hash1 << 5) + hash1) + prefix1[i]; /* hash * 33 + c */
    hash2 = ((hash2 << 5) + hash2) + prefix2[i]; /* hash * 33 + c */
  }

  for (size_t i = 0; i < len * sizeof(uint32_t); i++) {
    hash1 = ((hash1 << 5) + hash1) + *str; /* hash * 33 + c */
    hash2 = ((hash2 << 5) + hash2) + *str; /* hash * 33 + c */
    str++;
  }
  //    *v1 = static_cast<uint32_t>((hash & 0xFFFFFFFF00000000LL) >> 32);
  //    *v2 = static_cast<uint32_t>(hash & 0xFFFFFFFFLL) ;
  *v1 = hash1;
  *v2 = hash2;
}
#endif

#ifdef GLOBIMAP_USE_LOOKUP3
// This should not be used, it would need careful evaluation on many layers. Not
// published, google for lookup3.h ;-)
#include "lookup3.hpp"

inline void hash(uint32_t *data, size_t len, uint32_t *v1, uint32_t *v2) {
  hashword2(data, len, v1, v2);
}
#endif

#endif