# Ultra-Fast Bloom Filters using SIMD Techniques

**Authors:** Jianyuan Lu, Ying Wan, Yang Li, Chuwen Zhang, Huichen Dai (Tsinghua University), Yi Wang, Gong Zhang (Huawei Future Network Theory Lab), and Bin Liu (Tsinghua University)

**Venue/Year:** IEEE/ACM IWQoS (International Symposium on Quality of Service), 2017

**PDF:** SIMD-IWQoS.pdf

## Abstract/Overview

This paper introduces Ultra-Fast Bloom Filters (UFBF), a novel Bloom filter variant that leverages SIMD (Single Instruction Multiple Data) techniques to dramatically accelerate membership checking in high-speed network applications. The motivation stems from the increasing demand for line-rate processing on 40GE, 100GE, and 400GE network links, where routers must process packets within 75-150 clock cycles at 4GHz CPU frequencies. Standard Bloom filters become performance bottlenecks due to k independent hash computations and k potential cache misses per membership check.

UFBF addresses these bottlenecks through three primary optimizations: (1) parallel hash computation using SIMD instructions to compute k hash functions simultaneously from different seeds, (2) parallel bit-testing by organizing the bit array into block-word structures that enable SIMD-based simultaneous bit checks, and (3) improved cache efficiency by encoding each element's information into a small cache-line-aligned block. The paper demonstrates that UFBF achieves 2-4x speedup over standard Bloom filters, with particularly impressive gains (4x) for positive queries at k=8 hash functions.

The authors provide comprehensive theoretical analysis of false positive probability, cache miss behavior, and performance characteristics. Experimental validation using real-world CAIDA Internet traces on Intel i7-4790 CPUs with AVX/AVX2 instruction sets confirms that UFBF delivers nearly constant membership check speed regardless of k value, while standard implementations exhibit linear degradation. The trade-off is a modest increase in false positive rate (10-98% higher depending on load factor and word size), which the authors argue is worthwhile given the dramatic performance improvements.

## Key Contributions

- **Parallel Hash Computation Algorithm**: Novel SIMD-based algorithm that computes k hash functions in parallel using the same base hash function with different initial seeds. Reduces hash computation time to approximately 1/k of standard Bloom filters by loading p seeds into SIMD registers and applying hash operations in parallel.

- **Parallel Bit-Test Architecture**: Transforms sequential bit-testing (O(k) complexity) into parallel bit-testing (O(1) complexity) by organizing the bit array into blocks of k consecutive words. Each hash function is associated with exactly one word, enabling SIMD instructions to test all k bits simultaneously.

- **Cache-Optimized Block Design**: Proves that with cache-line-aligned blocks where block size divides cache-line size, only 1 cache miss occurs per membership check (vs. k cache misses for standard Bloom filters). Provides formal proof via Theorem 1 with corollaries showing k should be powers of 2 for optimal cache efficiency.

- **Comprehensive FPR Analysis**: Derives exact false positive probability formulas accounting for binomial distribution of elements across blocks, providing numerical comparisons showing UFBF's FPR is 5-300% higher than standard Bloom filters depending on load factor and word size (32-bit vs 64-bit).

- **SIMD Hash Function Transformation Rules**: Provides systematic methodology for converting traditional hash functions (MurmurHash3, lookup3) into SIMD-vectorized versions using broadcast, parallel arithmetic operations, and SIMD registers for intermediate values.

- **Real-World Performance Validation**: Extensive benchmarks on CAIDA Internet traces (5M destination IPs, 50M flows) demonstrating UFBF achieves 60-80 MSPS (Millions Searches Per Second) with nearly constant speed across k=3 to k=8, while competitors degrade from 60 MSPS to 20 MSPS.

- **Cache Size Sensitivity Analysis**: Demonstrates UFBF maintains performance advantage across all cache regimes (L1, L2, L3, and beyond), with particularly strong relative performance when working sets exceed L3 cache where standard filters' performance degrades sharply.

- **Multi-Platform Validation**: Tests show the approach is robust across different SIMD instruction sets (SSE with p=4, AVX with p=8 parallel operations), providing guidance for adapting to different CPU architectures.

## Algorithm/Data Structure Details

### Basic Structure

UFBF consists of r blocks, each containing b bits. Each block is subdivided into k consecutive words of w bits each, thus b = k × w. The total filter size is m = r × b = r × k × w bits.

```
Bit Array: [block[0]] [block[1]] ... [block[r-1]]
                ↓
Block: [word[0]][word[1]]...[word[k-1]]  (b = k*w bits)
         ↑
       Word: w bits (32 or 64 typically)
```

**Key Design Constraint**: Each hash function h_i (where 1 ≤ i ≤ k) is associated exclusively with word[i]. Hash h_i can only address bits within its associated word, never other words in the block.

### Insertion Algorithm

```
Function Insert(element e):
    loc ← h_0(e) mod r              // Select block using base hash
    for i = 1 to k:
        bit_pos ← h_i(e) mod w       // Hash to position in word[i]
        word_offset ← (i-1) × w      // Calculate word start position
        block[loc][word_offset + bit_pos] ← 1
```

**Example** (w=4, k=3):
- Element e hashed to block[7]
- h_1(e) = 2 → set bit 2 in word[0]: `0010|______|______`
- h_2(e) = 1 → set bit 1 in word[1]: `0010|0100|______`
- h_3(e) = 3 → set bit 3 in word[2]: `0010|0100|0001`

### Algorithm 1: Parallel Hash Computation

This algorithm produces p hash values in parallel using SIMD instructions, where p depends on the SIMD register width (p=4 for SSE, p=8 for AVX).

```
Input: element e, seeds[p] = [seed_1, seed_2, ..., seed_p]
Output: hashVals[p] = [h_1(e), h_2(e), ..., h_p(e)]

1. vr_seeds ← v_load(seeds)
   // Load p seeds into 256-bit SIMD register (AVX)

2. vr_val ← v_hashFunc(vr_seeds)
   // Apply SIMD-vectorized hash function in parallel
   // Each of p lanes processes one seed independently

3. hashVals ← v_store(vr_val)
   // Store p hash results back to memory
```

**Time Complexity**: O(1) amortized per hash value when k ≤ p (constant time for up to 8 hashes with AVX).

### Algorithm 2: SIMD Hash Function Conversion

Transforms a traditional hash function into its SIMD-vectorized equivalent:

**Traditional Hash Step**:
```
val ← val OP a        // Single scalar operation
```

**SIMD-Vectorized Equivalent**:
```
1. vr_a ← v_broadcast(a)       // Replicate scalar a to all p lanes
2. vr_val ← v_OP(vr_val, vr_a) // Apply operation to all p lanes in parallel
```

**Example - MurmurHash3 Step**:
```c
// Traditional (single value)
h ^= data;
h *= 0xcc9e2d51;

// SIMD version (8 values in parallel, AVX)
vr_data ← v_broadcast(data);
vr_h ← v_xor(vr_h, vr_data);
vr_const ← v_broadcast(0xcc9e2d51);
vr_h ← v_mul(vr_h, vr_const);
```

The paper measures that SIMD-version lookup3 takes 1.78× the time of single lookup3 due to SIMD register preparation overhead, but computes 8 hash values, yielding ~4.5× speedup for k=8.

### Algorithm 3: Parallel Membership Check

The core innovation enabling O(1) bit-testing complexity:

```
Function membershipCheck(element e):
    // Step 1: Select block
    loc ← h_0(e) mod r

    // Step 2: Compute k hash values in parallel
    vr_val ← compute k hash values using Algorithm 1
    // vr_val = [h_1(e) mod w, h_2(e) mod w, ..., h_k(e) mod w]

    // Step 3: Create bit masks in parallel
    vr_a ← v_broadcast(1)           // [1, 1, ..., 1] in all lanes
    vr_a ← v_shiftLeft(vr_a, vr_val) // [1<<h_1, 1<<h_2, ..., 1<<h_k]

    // Step 4: Load k words from selected block
    vr_b ← v_load(&block[loc])       // Load all k words in parallel

    // Step 5: Parallel bit-test using SIMD AND + test
    vr_b ← v_not(vr_b)               // Bitwise NOT on all k words
    v_test(vr_a, vr_b)               // Bitwise AND, sets zero-flag if all bits are 1

    // Step 6: Check result
    if zero-flag is set:
        return POSITIVE              // Element probably in set
    else:
        return NEGATIVE              // Element definitely not in set
```

**Key Insight**: The `v_test` instruction performs bitwise AND across all k lanes simultaneously and sets a CPU zero-flag if all results are zero (meaning all k membership bits were 1). This replaces k sequential bit-checks with a single SIMD instruction.

**Intel AVX Intrinsics Example**:
```c
__m256i vr_a = _mm256_sllv_epi32(ones, vr_val);  // Parallel shift
__m256i vr_b = _mm256_load_si256(&block[loc]);    // Load 256 bits
vr_b = _mm256_xor_si256(vr_b, all_ones);          // NOT operation
int result = _mm256_testc_si256(vr_a, vr_b);      // Test all bits
```

### Cache Efficiency Theorem

**Theorem 1**: If the cache-line size is L, block size satisfies b|L (b divides L), and the bit array is L-aligned, then at most 1 cache miss occurs per membership check.

**Proof Sketch (by Contradiction)**:
1. Assume >1 cache miss occurs
2. Block spans addresses [addr_s, addr_e) where addr_s = iL + jb, addr_e = iL + (j+1)b
3. For >1 miss, ∃t such that addr_s < tL < addr_e
4. Substituting: iL + jb < tL < iL + (j+1)b
5. Since L = sb (from b|L): isb + jb < tsb < isb + (j+1)b
6. Simplifying: 0 < (t-i)s - j < 1
7. But (t-i)s - j is an integer → contradiction

**Corollary 1**: If L is a power of 2, then b must be a power of 2 for Theorem 1 to hold.

**Corollary 2**: Since b = k × w and w is typically a power of 2 (32 or 64), then k should be a power of 2 for optimal cache efficiency.

**Practical Implication**: For 64-byte cache lines (L=512 bits) and w=64 bits:
- k=2 → b=128 bits (1 block per cache line, wasted space)
- k=4 → b=256 bits (2 blocks per cache line)
- k=8 → b=512 bits (1 block per cache line, optimal)
- k=16 → b=1024 bits (block spans 2 cache lines, Theorem 1 violated)

Thus k=8 is optimal for modern CPUs.

### False Positive Probability Analysis

Let X = number of elements inserted into a specific block. X follows binomial distribution Bino(n, 1/r).

**Conditional FPR** (given X=x elements in the block):
```
Pr{FP | X=x} = [1 - (1 - 1/w)^x]^k
```
This is the probability that all k bits are set by x random insertions into k words of w bits each.

**Overall FPR** (marginalizing over X):
```
f_u = Σ(x=0 to n) Pr{X=x} · Pr{FP | X=x}
    = Σ(x=0 to n) C(n,x) · (1/r)^x · (1 - 1/r)^(n-x) · [1 - (1 - 1/w)^x]^k
```

**Comparison with Standard Bloom Filter**:
```
f_s = (1 - e^(-kn/m))^k     // Standard BF
```

The paper's Table I shows numerical results for n=10000, k=4:

| Load Factor | f_s (Standard) | f_u (w=32) | Overhead | f_u (w=64) | Overhead |
|-------------|----------------|------------|----------|------------|----------|
| 0.02 | 3.49×10^-5 | 1.39×10^-4 | +298% | 7.98×10^-5 | +128% |
| 0.10 | 1.18×10^-2 | 1.56×10^-2 | +32% | 1.37×10^-2 | +16% |
| 0.20 | 9.20×10^-2 | 1.01×10^-1 | +10% | 9.69×10^-2 | +5% |

**Key Observations**:
1. FPR overhead decreases with higher load factors
2. Doubling word size (32→64) roughly halves FPR overhead
3. At very low load factors, UFBF can have 3x higher FPR than standard
4. The overhead diminishes as load approaches 0.2

## Key Findings/Results

### Hash Computation Performance (Figure 3)

**Traditional Hash Functions** (Sequential):
- lookup3: 23 clock cycles per hash
- MurmurHash3: ~23 clock cycles per hash
- Linear scaling: k hash functions = k × 23 cycles
- For k=8: ~184 clock cycles total

**SIMD-Vectorized Hash Functions** (Parallel):
- lookup3-SIMD: 41 clock cycles for 8 hashes (1.78× overhead for preparation)
- MurmurHash3-SIMD: 41 clock cycles for 8 hashes
- Constant time regardless of k (up to p=8 for AVX)
- For k=8: ~41 clock cycles total (4.5× speedup)

**Crossover Point**: SIMD becomes faster than sequential at k≥3, with gains increasing linearly up to k=p.

### Membership Check Speed Comparison (Figure 4)

**Experimental Setup**: n=10^5 elements, m=10^6 bits (load factor 0.1), 1M queries per experiment, averaged over 1000 runs.

**Dataset1 (5M destination IPs, 4 bytes)** - Negative Check:

| k | SBF (MSPS) | OMBF (MSPS) | OHBF (MSPS) | UFBF (MSPS) | Speedup vs SBF |
|---|------------|-------------|-------------|-------------|----------------|
| 3 | 58 | 62 | 69 | 82 | 1.41× |
| 4 | 48 | 53 | 62 | 80 | 1.67× |
| 5 | 41 | 46 | 56 | 79 | 1.93× |
| 6 | 36 | 41 | 51 | 78 | 2.17× |
| 7 | 32 | 37 | 47 | 77 | 2.41× |
| 8 | 29 | 34 | 43 | 77 | 2.66× |

**Dataset2 (50M flows, 13 bytes)** - Positive Check:

| k | SBF (MSPS) | OMBF (MSPS) | OHBF (MSPS) | UFBF (MSPS) | Speedup vs SBF |
|---|------------|-------------|-------------|-------------|----------------|
| 3 | 46 | 48 | 53 | 73 | 1.59× |
| 4 | 35 | 38 | 44 | 72 | 2.06× |
| 5 | 28 | 31 | 38 | 71 | 2.54× |
| 6 | 23 | 27 | 33 | 70 | 3.04× |
| 7 | 20 | 23 | 29 | 69 | 3.45× |
| 8 | 17 | 21 | 26 | 68 | 4.00× |

**Key Insights**:
1. **UFBF maintains ~constant speed** (77-82 MSPS for negative, 68-73 MSPS for positive) regardless of k
2. **Standard BF degrades linearly** (58→29 MSPS as k increases 3→8)
3. **OHBF outperforms OMBF** due to reduced hash overhead (1 hash vs k hashes)
4. **Positive queries slower than negative** for SBF/OMBF/OHBF (must check all k bits), but nearly identical for UFBF (parallel bit-test)
5. **Peak speedup at k=8**: 4× for positive checks, 2.66× for negative checks

### Cache Size Sensitivity (Figure 5)

Tests varying m from 1M to 100M bits across L1 (256KB = 2Mb), L2 (2MB = 16Mb), L3 (8MB = 64Mb) cache boundaries.

**Negative Check (k=8, load factor 0.1)**:

| Filter Size | Cache Level | SBF | OMBF | OHBF | UFBF | UFBF Advantage |
|-------------|-------------|-----|------|------|------|----------------|
| 2M bits | L1 | 55 | 58 | 62 | 68 | +24% |
| 10M bits | L2 | 48 | 52 | 58 | 65 | +35% |
| 50M bits | L3 | 38 | 42 | 48 | 60 | +58% |
| 100M bits | DRAM | 12 | 15 | 18 | 45 | +275% |

**Critical Observation**: When filter exceeds L3 cache, OHBF performance collapses (62 → 18 MSPS), while UFBF remains robust (68 → 45 MSPS). This is because UFBF's cache-line blocking reduces random access patterns even when working from DRAM.

### False Positive Validation (Figure 6)

Compares theoretical formula f_u (Equation 3) with simulation results for n=10000, w=4, k=4:

| Load Factor | Theory | Simulation (dataset1) | Simulation (dataset2) | Error |
|-------------|--------|-----------------------|-----------------------|-------|
| 0.02 | 1.39×10^-4 | 1.39×10^-4 | 1.39×10^-4 | <0.1% |
| 0.08 | 8.11×10^-3 | 8.10×10^-3 | 8.12×10^-3 | <0.2% |
| 0.20 | 1.01×10^-1 | 1.01×10^-1 | 1.01×10^-1 | <0.1% |

**Validation**: Simulations perfectly match theory across all load factors, confirming the analytical FPR formula is exact (not approximate).

### Platform Robustness

**Intel i7-4790** (4 cores, 3.6GHz, AVX2):
- L1 D-Cache: 32KB per core
- L2 Cache: 256KB per core
- L3 Cache: 8MB shared
- Cache line: 64 bytes (512 bits)
- SIMD: AVX2 (256-bit registers, p=8 for 32-bit integers)

**Compiler**: gcc with `-mavx -mavx2` flags
**OS**: Windows 7 64-bit (unusual choice, typically Linux for HPC)

The paper's results are consistent across both datasets (IP addresses vs. flow records), suggesting robustness to data characteristics.

## Relevance to Project

### Direct Applications to CountingGloBiMap

The UFBF paper is highly relevant to the CountingGloBiMap project, which already includes several SIMD-optimized implementations. The paper provides both theoretical foundations and practical implementation strategies for the existing SIMD variants.

#### 1. **Existing SIMD Implementation Validation**

The project already contains `SimdBloomFilter` and `PatternedSimdBloomFilter` in `/home/moritz/workspace/counting-globimaps/include/simd_bloom_filter.hpp`. These implementations likely draw inspiration from this paper's techniques:

- **Parallel hash computation**: The block selection + intra-block hashing pattern matches Algorithm 1
- **Vectorized operations**: AVX2 gather instructions and batch processing align with Algorithm 3's parallel bit-test
- **Cache-line blocking**: The 256-bit block design matches Theorem 1's cache optimization

However, UFBF focuses on **membership-only** queries, while CountingGloBiMap needs **frequency estimation**. The key challenge is adapting parallel bit-test (Algorithm 3) to parallel counter-reads for `get_min()` operations.

#### 2. **Hash Function Optimization for Multi-Layer Filters**

The project's `/home/moritz/workspace/counting-globimaps/include/hashfn.hpp` already uses a double-hashing trick similar to the Less Hashing method:

```cpp
// Current implementation (from CLAUDE.md)
hash[i] = (h1 + (i+1) * h2) & mask
```

This is compatible with UFBF's approach but could be enhanced:

**Current Approach** (Sequential):
```cpp
for (int i = 0; i < k; i++) {
    uint64_t h = (h1 + (i+1) * h2) & mask;
    // Access counter at position h
}
```

**UFBF-Style Parallel** (Proposed):
```cpp
// Initialize SIMD register with [1, 2, 3, ..., 8]
__m256i indices = _mm256_setr_epi32(1, 2, 3, 4, 5, 6, 7, 8);
__m256i vr_h2 = _mm256_set1_epi32(h2);
__m256i vr_h1 = _mm256_set1_epi32(h1);

// Compute 8 hash values in parallel: h1 + [1,2,...,8] * h2
__m256i vr_hashes = _mm256_add_epi32(vr_h1, _mm256_mullo_epi32(indices, vr_h2));

// Parallel counter reads (requires gather instruction)
// This is where counting differs from membership - need to read counter values, not just bits
```

#### 3. **Counting-Specific Challenges**

UFBF's parallel bit-test (Algorithm 3, lines 8-10) works because membership checks only need a binary result (all bits set or not). Counting Bloom filters need to:

1. **Read k counter values** (not bits)
2. **Find the minimum** across k values
3. **Return the minimum count**

**Adaptation Strategy**:
```cpp
// Parallel counter reads using AVX2 gather
__m256i counters = _mm256_i32gather_epi32(
    (int*)block[loc],  // Base address
    vr_hashes,         // Offsets (from parallel hash)
    sizeof(uint32_t)   // Scale factor
);

// Parallel minimum (horizontal reduction)
uint32_t min_count = horizontal_min_epi32(counters);
```

The challenge: AVX2 gather is **significantly slower** than sequential reads for small k (Intel optimization guide shows ~20 cycle latency), potentially negating UFBF's gains for counting variants.

#### 4. **Multi-Layer Hierarchical Integration**

CountingGloBiMap uses layers with different bit depths (1, 8, 16, 32, 64 bits). UFBF's block design could optimize **intra-layer** operations:

**Current Design** (Conceptual):
```
Layer[8-bit]: [counter0][counter1]...[counter_n]  // Each 8 bits
```

**UFBF-Style Blocked Design** (Proposed):
```
Layer[8-bit]: [block0][block1]...[block_r]
                  ↓
              [word0][word1]...[word7]  // 8 words if k=8
                  ↓
              Eight 8-bit counters (64-bit word)
```

**Benefit**: When querying a point, all k counters for layer L are in the same cache line, reducing cache misses from k → 1 per layer. With typical 3-4 layers, this reduces total cache misses from ~12-16 → 3-4.

**Implementation Impact**:
- Modify `Layer` struct in `/home/moritz/workspace/counting-globimaps/include/counting_globimap.hpp`
- Add block index calculation: `block_idx = h0(point) % r`
- Constrain each hash h_i to word[i] within selected block
- Ensure block size ≤ cache line (e.g., k=8 words × 64 bits = 512 bits = 64 bytes)

#### 5. **Cascade Operations**

CountingGloBiMap's overflow cascade (increment in layer L triggers increment in layer L+1) could benefit from SIMD parallelization:

**Current Cascade** (Sequential):
```cpp
if (counter[layer_L][pos] == MAX_VALUE) {
    counter[layer_L+1][pos_next] += 1;
}
```

**UFBF-Style Parallel Cascade**:
```cpp
// Check k counters in parallel for overflow
__m256i counters = /* load k counters */;
__m256i max_vals = _mm256_set1_epi32(MAX_VALUE);
__m256i overflow_mask = _mm256_cmpeq_epi32(counters, max_vals);

// If any overflow detected, cascade (this is complex in SIMD)
int overflow_bits = _mm256_movemask_epi8(overflow_mask);
if (overflow_bits) {
    // Handle overflows (may need scalar fallback)
}
```

**Challenge**: Cascades are rare events (only at saturation), so SIMD overhead may not be justified. Better to optimize the common path (increment without cascade).

### Key Differences / Integration Points

#### Membership vs. Counting

| Aspect | UFBF (Membership) | CountingGloBiMap (Counting) |
|--------|-------------------|------------------------------|
| **Query Operation** | All bits set? (binary) | Min counter value (integer) |
| **SIMD Test** | Single `v_test` instruction | Gather + horizontal min reduction |
| **Complexity** | O(1) with SIMD | O(log k) horizontal reduction |
| **Speedup Potential** | 4× (proven) | ~2-3× (estimated, gather overhead) |

#### Hash Function Strategy

| Aspect | UFBF | CountingGloBiMap | Integration Path |
|--------|------|------------------|------------------|
| **Base Hash** | One hash h0 for block selection | Two hashes h1, h2 for double hashing | Compatible - h0 can be h1 |
| **Additional Hashes** | k different seeds, parallel computation | Synthetic: h1 + i×h2 | UFBF's seed approach may have better independence |
| **Parallelization** | Load k seeds → SIMD hash | Compute h1, h2 → sequential generation | Hybrid: parallel seed hashing for h1, h2 |

**Recommendation**: Benchmark both approaches:
- **UFBF's seed method**: Better hash independence but requires SIMD-compatible hash function
- **Current double-hashing**: Simpler, already validated in literature (Kirsch & Mitzenmacher 2008)

#### Block Size Trade-offs

| Block Size | UFBF Recommendation | CountingGloBiMap Constraint | Resolution |
|------------|---------------------|------------------------------|------------|
| **Optimal k** | Power of 2 (k=2,4,8,16) | Application-dependent (k=4-12 typical) | Use k=8 as default for SIMD path |
| **Word Size** | 32-bit (AVX: p=8), 64-bit (AVX: p=4) | Varies by layer (8/16/32/64-bit counters) | Different blocking strategies per layer |
| **Cache Alignment** | Strict (b divides L) | Flexible | Enforce for SIMD variants only |

**Layer-Specific Blocking**:
- **8-bit layer**: k=8 words × 8 bits = 64 bits (fits 8× per cache line)
- **16-bit layer**: k=8 words × 16 bits = 128 bits (fits 4× per cache line)
- **32-bit layer**: k=8 words × 32 bits = 256 bits (fits 2× per cache line)
- **64-bit layer**: k=8 words × 64 bits = 512 bits (fits 1× per cache line) ← optimal

#### False Positive vs. Estimation Error

UFBF's FPR increase (10-300%) is acceptable for membership tests but translates to **estimation error** for counting:

**Scenario**: Element with true count = 100
- Standard CBF: Estimated count ≈ 100-105 (5% error typical)
- UFBF-style blocked CBF: Estimated count ≈ 110-120 (20% error if FPR doubles)

**Mitigation**:
1. Use larger word sizes (w=64) to reduce FPR overhead (Table I shows 64-bit halves error vs. 32-bit)
2. Increase memory budget by 10-25% (Section III-D suggests this compensates)
3. Apply minimal increment strategy (already in CountingGloBiMap with `minimal_increment` flag)

### Practical Takeaways

- **Use k=8 for SIMD optimization**: AVX2 can process 8×32-bit or 4×64-bit integers in parallel, making k=8 the sweet spot for 32-bit counters (and k=4 for 64-bit counters).

- **Cache-line alignment is critical**: Theorem 1 proves that proper alignment reduces cache misses from k to 1. The project should enforce 64-byte alignment for all SIMD filter variants using `alignas(64)` or `_mm_malloc`.

- **64-bit words preferable**: Table I shows 64-bit words have ~50% lower FPR overhead than 32-bit for the same memory budget. CountingGloBiMap's 64-bit layer should be prioritized for SIMD implementation.

- **Parallel hashing has 1.78× overhead**: Algorithm 1's SIMD hash is 1.78× slower than scalar hash for k=1, but becomes 4.5× faster at k=8. This suggests minimum k=3 threshold before SIMD hashing is worthwhile.

- **Positive queries benefit most**: Figure 4 shows 4× speedup for positive checks vs 2.66× for negative at k=8. For spatial applications with high hit rates (many queries returning counts >0), SIMD optimization yields maximum benefit.

- **DRAM-bound workloads see largest gains**: Figure 5 shows UFBF maintains 45 MSPS when filter exceeds L3 cache, while OHBF drops to 18 MSPS. For large spatial datasets (e.g., full GDELT 182M events), SIMD blocking prevents performance collapse.

- **FPR overhead decreases with load**: Table I shows FPR overhead drops from +298% at load 0.02 to +10% at load 0.20. CountingGloBiMap should target load factors >0.1 when using SIMD variants.

- **Platform-independent gains**: Results hold across Intel (4× speedup) and AMD (5× speedup) platforms, suggesting robust portability. The project's AVX2 requirement (`-march=native` flag) is well-justified.

- **Power-of-2 k simplifies masking**: Corollary 2 shows k as power of 2 enables efficient bit manipulation. The project's current k values (often 4, 8) are well-chosen for SIMD.

- **MurmurHash3 is SIMD-friendly**: Figure 3 shows MurmurHash3's SIMD version has similar characteristics to lookup3. The project's use of MurmurHash3 in `/home/moritz/workspace/counting-globimaps/include/murmur.hpp` is compatible with UFBF techniques.

### Research Applications

#### 1. **Spatial Query Acceleration**

UFBF's techniques directly apply to GloBiMap's rasterization operations:

**Current Approach** (`globimap.hpp` rasterization):
```cpp
for (y = y_start; y < y_end; y++) {
    for (x = x_start; x < x_end; x++) {
        bool present = bf.get({x, y});  // k hash + k cache misses
        raster[y][x] = present;
    }
}
```

**UFBF-Accelerated Approach** (Batch SIMD):
```cpp
for (y = y_start; y < y_end; y++) {
    for (x = x_start; x < x_end; x += 8) {  // Process 8 points in parallel
        // Prepare 8 point coordinates in SIMD registers
        __m256i vr_x = _mm256_setr_epi32(x, x+1, x+2, ..., x+7);
        __m256i vr_y = _mm256_set1_epi32(y);

        // Parallel hash computation for 8 points
        __m256i vr_blocks = parallel_hash_block_select(vr_x, vr_y);

        // Parallel bit-test (requires scatter-gather)
        __m256i vr_results = parallel_membership_check(vr_blocks);

        // Store results
        _mm256_storeu_si256((__m256i*)&raster[y][x], vr_results);
    }
}
```

**Expected Speedup**: 3-4× for spatial rasterization (common operation in spatial queries), particularly beneficial for the polygon mask experiments (`globimap_test_polygons_mask.cpp`).

#### 2. **Multi-Category SIMD Optimization**

The project's multi-category support (CLAUDE.md Section "Multi-Category Support") could leverage SIMD for batch category queries:

**Sequential Category Queries**:
```cpp
for (int cat = 0; cat < 4; cat++) {
    uint64_t count = filter.get_min({x, y, cat});
    category_counts[cat] = count;
}
```

**SIMD Batch Category Queries**:
```cpp
// Query 4 categories in parallel (AVX2 with 64-bit integers, p=4)
__m256i vr_categories = _mm256_setr_epi64x(0, 1, 2, 3);
__m256i vr_coords = /* broadcast (x, y) to all lanes */;

// Parallel hash + gather (complex, may need library support)
__m256i vr_counts = parallel_get_min_multi_category(vr_coords, vr_categories);

// Store results
_mm256_storeu_si256((__m256i*)category_counts, vr_counts);
```

**Application**: The GDELT multi-category benchmark (`globimap_test_multicategory_dataset.cpp`) with 4 QuadClass categories is perfectly sized for AVX2's 4×64-bit parallelism.

#### 3. **Hierarchical Layer Parallel Queries**

CountingGloBiMap's multi-layer structure (typically 3-4 layers) could benefit from parallel queries across layers:

**Sequential Layer Cascade**:
```cpp
uint64_t count = 0;
for (int layer = 0; layer < num_layers; layer++) {
    uint64_t layer_min = get_min_in_layer(point, layer);
    count = max(count, layer_min);  // Cascade logic
    if (layer_min < MAX_VALUE) break;  // Early exit
}
```

**SIMD Parallel Layer Query** (Speculative):
```cpp
// Query all layers simultaneously (ignores early exit, but fast)
__m256i vr_layer_counts = parallel_query_all_layers(point);

// Horizontal max reduction to find final count
uint64_t count = horizontal_max_epi64(vr_layer_counts);
```

**Trade-off**: Loses early-exit optimization but eliminates branch mispredictions. Beneficial when most queries hit the highest layer (high counts), which is common for "hot" spatial locations.

#### 4. **Dataset Conversion Pipeline Acceleration**

The project's CSV-to-HDF5 conversion (`datasets/utils/csv_to_hdf5.py`) could be accelerated with a C++ SIMD-based insertion:

**Python Sequential Insertion**:
```python
for row in csv_reader:
    lat, lon = float(row['lat']), float(row['lon'])
    filter.put([lat, lon])  # k hash + k memory writes
```

**C++ SIMD Batch Insertion**:
```cpp
// Process 8 coordinates simultaneously
for (int i = 0; i < num_points; i += 8) {
    __m256d vr_lats = _mm256_loadu_pd(&lats[i]);    // Load 4 doubles (AVX-512: 8)
    __m256d vr_lons = _mm256_loadu_pd(&lons[i]);

    // Parallel hash computation for 8 points
    parallel_put_batch(filter, vr_lats, vr_lons);
}
```

**Expected Speedup**: 2-3× for large dataset insertion (GDELT 1.9M events, COVID-19 1.8M sampled events).

#### 5. **K-Parameter Sensitivity Analysis**

The project includes `globimap_test_datasets_for_k.cpp` for k-value sweeps. UFBF's analysis suggests:

**Current Sweep**: k ∈ {3, 4, 5, 6, 7, 8, 9, 10, 11, 12}

**UFBF-Informed Sweep**:
- **Primary comparison**: k ∈ {4, 8} (powers of 2, optimal cache efficiency)
- **Extended sweep**: k ∈ {2, 4, 8, 16} (all powers of 2 within AVX2 capability)
- **Expected finding**: k=8 should show best throughput, k=4 best accuracy (lower FPR)

This aligns with the project's observation (CLAUDE.md): "UFBF experiences better cache efficiency when k is a power of 2".

#### 6. **Benchmark Infrastructure Enhancement**

The existing `bloom_filter_benchmark` executable could be extended with UFBF-specific tests:

**New Benchmark Categories**:
1. **Hash computation microbenchmark**: Measure cycles for k hashes (sequential vs SIMD)
2. **Cache sensitivity**: Test performance across m ∈ {L1, L2, L3, DRAM} boundaries
3. **Positive/negative split**: Separate timing for hit/miss queries (UFBF shows constant time)
4. **SIMD parallelism scaling**: Vary p ∈ {1, 2, 4, 8} to measure parallel efficiency
5. **Load factor sensitivity**: Measure FPR overhead at different load factors (validate Table I)

**Integration Point**: Extend `experiments/src/bloom_filter_benchmark.cpp` with UFBF variants alongside existing cache-optimal filters.

### Implementation Reference

**Primary Target**: `/home/moritz/workspace/counting-globimaps/include/simd_bloom_filter.hpp`

This file already implements SIMD-based bloom filters. Based on the project's CLAUDE.md:

- **SimdBloomFilter**: Processes 8 elements in parallel using AVX-256, uses gather instructions for efficient block lookups
- **PatternedSimdBloomFilter**: Pre-generated mask patterns with rotation, better FPR than standard SimdBloomFilter

These implementations appear to already incorporate UFBF's key ideas (parallel hash, gather instructions, block-based design). The paper provides:

1. **Theoretical validation**: Theorem 1 justifies the cache-line blocking strategy
2. **FPR analysis**: Equation 3 can be used to predict PatternedSimdBloomFilter's accuracy
3. **Performance bounds**: Figure 4 data provides realistic speedup expectations (2-4×)
4. **Optimization guidance**: k=8, w=64, cache-line alignment recommendations

**Recommended Enhancements**:

1. **Add UFBF variant to comparison** (`experiments/src/compare_all_implementations.cpp`):
   ```cpp
   // Add to existing comparison suite
   filters.push_back(new UFBFAdapter(memory_budget, k=8, w=64));
   ```

2. **Validate FPR formula** in unit tests (`tests/test_cache_optimal_bf.cpp`):
   ```cpp
   // Test that measured FPR matches Equation 3 prediction
   double theoretical_fpr = compute_ufbf_fpr(n, r, w, k);
   double measured_fpr = run_fpr_experiment(filter, n);
   ASSERT_NEAR(theoretical_fpr, measured_fpr, 0.01);
   ```

3. **Implement counting variant** (new file: `include/simd_counting_bloom_filter.hpp`):
   ```cpp
   class SimdCountingBloomFilter {
       // Adapt UFBF's parallel bit-test (Algorithm 3) to parallel counter-read
       uint64_t get_min(const std::vector<uint64_t>& point) {
           // Use _mm256_i32gather_epi32 for parallel counter loads
           // Use horizontal min reduction to find minimum
       }
   };
   ```

4. **Add hash computation benchmarks** to validate Figure 3 claims:
   ```cpp
   // Measure cycles for k hash computations
   benchmark_hash_computation("MurmurHash3-Sequential", k);
   benchmark_hash_computation("MurmurHash3-SIMD", k);
   ```

5. **Extend reporting** (`reports/generate_reports.sh`) with SIMD-specific analysis:
   - Hash computation speedup vs k (reproduce Figure 3)
   - Membership check throughput vs k (reproduce Figure 4)
   - Cache sensitivity analysis (reproduce Figure 5)
   - FPR validation (reproduce Figure 6)

**Integration with CountingGloBiMap**:

The most promising integration point is adapting UFBF's parallel query mechanism for CountingGloBiMap's hierarchical layers:

```cpp
// In counting_globimap.hpp
#ifdef USE_SIMD
template<>
uint64_t CountingGloBiMap<>::get_min(const std::vector<uint64_t>& point) {
    if (hash_k == 8) {  // SIMD-optimized path
        return get_min_simd_k8(point);
    } else {
        return get_min_sequential(point);  // Fallback
    }
}

uint64_t get_min_simd_k8(const std::vector<uint64_t>& point) {
    // Implement UFBF Algorithm 3 adapted for counters
    // 1. Parallel hash computation (Algorithm 1)
    // 2. Parallel counter gather
    // 3. Horizontal min reduction
}
#endif
```

This would provide a direct speedup path for the high-performance query operations needed in spatial applications while maintaining backward compatibility for non-SIMD platforms.

**Estimated Implementation Effort**:
- **Phase 1** (1-2 weeks): Add UFBF membership-only variant, validate against paper's benchmarks
- **Phase 2** (2-3 weeks): Extend to counting variant with AVX2 gather instructions
- **Phase 3** (1 week): Integrate into CountingGloBiMap as optional SIMD path
- **Phase 4** (1 week): Comprehensive benchmarking and reporting

**Expected Performance Gains**:
- **Membership queries**: 2-4× speedup (proven by paper)
- **Counting queries**: 1.5-2.5× speedup (estimated, limited by gather latency and horizontal reduction)
- **Spatial rasterization**: 3-4× speedup (batch processing of adjacent coordinates)
- **Multi-category queries**: 2-3× speedup (parallel category access)

The paper provides strong theoretical and empirical foundations for SIMD optimization in bloom filter applications, directly supporting the project's existing SIMD implementations and offering clear paths for further performance optimization.
