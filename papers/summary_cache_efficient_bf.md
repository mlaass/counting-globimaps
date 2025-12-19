# Cache-, Hash-, and Space-Efficient Bloom Filters

**Authors:** Felix Putze, Peter Sanders, Johannes Singler (Karlsruhe Institute of Technology)

**Venue/Year:** ACM Journal of Experimental Algorithmics (JEA), Volume 14, September 2009

**PDF:** 1498698.1594230.pdf

## Abstract/Overview

This paper proposes several new variants of Bloom filters that improve cache efficiency, reduce hash function requirements, and provide better space efficiency compared to standard Bloom filters. The authors present a comprehensive analysis of false-positive rates, cache miss behavior, and hash bit requirements, along with highly optimized implementations. The key insight is that by concentrating hash operations within cache-line-sized blocks and using precomputed bit patterns, significant performance improvements can be achieved with only modest increases in false-positive rates.

## Key Contributions

- **Blocked Bloom Filters (blo)**: Partitions the bit array into cache-line-sized blocks (512 bits). First hash selects the block, remaining k-1 hashes probe within that block. Reduces cache misses from k to 1 per operation.

- **Bit Pattern Bloom Filters (pat)**: Uses precomputed random k-bit patterns stored in a lookup table. Replaces k hash evaluations with a single table lookup, enabling SIMD vectorization.

- **Multiplexing Patterns (pat[x])**: Achieves larger pattern variety by OR-ing x patterns with k/x bits each, reducing table size while maintaining low FPR.

- **Multiblocking (blo[X])**: Distributes hash operations across X blocks, setting k/X bits per block. Improves FPR compared to single large block.

- **Golomb-Compressed Sequence (gcs)**: Space-optimal Bloom filter replacement using Golomb coding for compressed storage of hash values. Approaches information-theoretic minimum of n log(1/f) bits.

- **Comprehensive FPR Analysis**: Derives analytical formulas accounting for non-uniform block occupancy (modeled as Poisson distribution), pattern table collisions, and multiblocking effects.

## Algorithm/Data Structure Details

### Blocked Bloom Filter

- **Structure**: Array of b blocks, each B bits (typically 512 bits = 64 bytes cache line)
- **Hash strategy**: h1 selects block, h2...hk probe within block
- **FPR formula**:
  ```
  f_blo(B, c, k) = Σ Poisson(B/c, i) · f_inner(B, i, k)
  ```
  where occupancies follow Poisson(B/c) distribution

### Bit Pattern Method

- **Pattern table**: Ω precomputed k-bit patterns of width B
- **Collision probability**: p_coll(n, Ω) = 1 - (1 - 1/Ω)^n
- **FPR bound**:
  ```
  f_pat ≤ p_coll + (1 - p_coll) · f_std
  ```
- **Implementation trick**: Store filter in inverted form, reducing membership test to `(p ∧ ¬f) = 0` (single AND + zero test)

### Golomb-Compressed Sequence

- **Approach**: Store sorted hash values as Golomb-coded differences
- **Rationale**: Hash value gaps are geometrically distributed with p=1/c, optimal for Golomb coding
- **Space**: Approaches n log(ef) bits (information-theoretic minimum)
- **Access structure**: Divides range into blocks of size I, stores pointers for random access
- **Trade-off**: Small I gives fast queries, large I saves space

### Hash Function Optimization

- **Double hashing trick**: Compute h1, h2 once, generate k hashes as:
  ```
  hash[i] = (h1 + (i+1) · h2) mod m
  ```
- **Hash bits required**:
  - Standard: k log m (insert/positive), 2 log m (negative)
  - Blocked[X]: X log(m/B) + k log B
  - Pattern[x,X]: X(log(m/B) + x log Ω)
  - GCS/CH: log(n/f) for all operations

## Key Findings/Results

### Performance Results (Intel Xeon 5140, 2.33GHz, 4MB L2 cache)

- **Maximum speedup**: ~4x for blocked variants vs. standard (c=20, k=14)
- **Cache miss reduction**: 13.96x fewer misses for blo[1], but only 3-4x speedup due to prefetching
- **Best for positive queries**: pat[1,2] up to 2x faster than standard at low FPR
- **Pattern table size**: Ω=64K (4MB table) optimal for 64-byte cache lines

### FPR Analysis

- **Standard Bloom filter**: f = (1 - e^(-kn/m))^k ≈ (1/2)^k for optimal k = ln(2)·c
- **Blocked overhead**: FPR increases due to non-uniform occupancy
  - c=8: FPR 0.0231 vs 0.0215 (7% increase)
  - c=20: FPR triples from 0.0000671 to 0.000194
  - Compensation: Increase c by 12-25% for c<20
- **Pattern collisions**: Limit FPR at c=34 to ~2.3×10^-4 for Ω=64K

### Space Efficiency

- **Standard Bloom filter**: 1.44x information-theoretic minimum
- **GCS**: Reaches 29.14 bits/element (optimal) with large index interval I
- **Compact Hash (CH)**: 30.80 bits/element minimum (α→1)
- **Trade-off table** (Table I): Shows required c' to match standard FPR
  - c=8→9 (+12%), c=16→18 (+12%), c=32→64 (+100%)

### Cache Efficiency Validation

- **blo[1]**: 1 cache miss per operation (vs 14 for std with k=14)
- **Pattern variants**: 1.7x more misses when table equals cache size (non-LRU eviction)
- **Negative queries**: Already efficient (~2 misses for std), minimal improvement

### Platform Comparison

- **AMD Opteron 844** (1.8GHz, 1MB cache): Even better speedups (~5x vs 4x on Intel)
- **Robustness**: Results consistent across architectures

## Relevance to Project

### Direct Applications to CountingGloBiMap

1. **Cache-Optimal Design Patterns**: The blocked approach is directly applicable to CountingGloBiMap's hierarchical layers. Current implementation could benefit from cache-line alignment and blocking strategies.

2. **Hash Function Optimization**: The double-hashing trick (h_i = h1 + i·h2) is already used in the codebase (`hashfn.hpp`). This paper validates that approach and provides theoretical backing.

3. **SIMD Opportunities**: The bit pattern method's use of SIMD instructions (mentioned in project as `SimdBloomFilter` and `PatternedSimdBloomFilter`) comes directly from this paper's techniques.

4. **Multi-Layer FPR Analysis**: The Poisson-based analysis for non-uniform occupancy could inform better layer sizing in CountingGloBiMap's hierarchical design.

5. **Space-Time Trade-offs**: The gcs approach provides a template for reducing CountingGloBiMap's memory footprint during serialization/transmission.

### Connections to Existing Implementations

- **BlockedBloomFilter** (`blocked_bloom_filter.hpp`): Directly implements the blo[1] variant from this paper
- **RegisterBlockedBloomFilter** (`register_blocked_bf.hpp`): Uses 64-bit registers similar to pattern-based approach
- **SimdBloomFilter** (`simd_bloom_filter.hpp`): Implements SIMD vectorization ideas from Section 3.1

### Performance Insights for Benchmarking

- **Expected speedups**: 3-4x for spatial queries when using blocked variants
- **Memory overhead**: Budget 12-25% extra memory to compensate FPR degradation
- **Optimal k selection**: Paper's formulas could optimize CountingGloBiMap's hash_k parameter

### Future Research Directions

1. **Hybrid Multi-Layer Blocking**: Combine cache-line blocking with CountingGloBiMap's bit-depth layers
2. **Dynamic GCS for Counting**: Adapt Golomb coding for compressed multi-layer storage
3. **Pattern-Based Cascading**: Use precomputed patterns for overflow cascade operations
4. **Multiblocking for Categories**: Apply blo[X] to multi-category isolation

### Theoretical Validation

- **False-positive bounds**: Provides rigorous analysis missing from some earlier counting BF papers
- **Cache model**: Validates that k cache misses → actual performance (though prefetching matters)
- **Information theory**: Establishes hard limits for space (n log(1/f) bits) that counting variants must respect

This paper is foundational for understanding the cache-optimal bloom filter implementations already in the project and provides analytical tools for optimizing CountingGloBiMap's performance-accuracy trade-offs.
