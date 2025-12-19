# The Variable-Increment Counting Bloom Filter

**Authors:** Ori Rottenstreich, Yossi Kanizo, Isaac Keslassy (Technion)
**Venue/Year:** IEEE INFOCOM 2012
**PDF:** infocom12_variable.pdf

## Abstract/Overview

This paper introduces a novel method to improve the memory efficiency of Counting Bloom Filters (CBFs) using variable increments. Unlike standard CBFs that increment counters by 1 on insertion, the Variable-Increment CBF (VI-CBF) increments counters by a hashed variable value from a predetermined set D. During queries, the exact counter value is examined to determine if a specific increment could be part of the sum, enabling more precise membership testing. The method achieves lower false positive rates and overflow probabilities than CBF while maintaining constant-time operations and practical hardware implementation.

## Key Contributions

- **Variable-Increment Method**: First application of variable increments to improve CBF efficiency in networking applications
- **Two Novel Schemes**:
  - **Bh-CBF**: Uses Bh sequences (first network application of these mathematical sequences) with paired counters (fixed + variable increment)
  - **VI-CBF**: Simplified single-counter design with variable increments
- **Simple Set D = [L, 2L-1]**: Hardware-friendly increment set requiring no lookup tables
- **Theoretical Guarantees**: Proved VI-CBF always achieves lower false positive rate than CBF for m ≥ 10 counters
- **Hierarchical Extension**: ML-VI-HCBF combines variable increments with multi-layer compression for 2x memory savings
- **Practical Validation**: Real-world trace experiments showing up to 10x improvement in false positive rate

## Algorithm/Data Structure Details

### Core Concept

**Standard CBF**: Increments k hashed counters by +1 on insertion, checks if all k counters > 0 for membership.

**VI-CBF**: Uses two hash function sets:
- **H = {h1, ..., hk}**: Points to counter positions (range {1, ..., m})
- **G = {g1, ..., gk}**: Selects increment values from set D (range {1, ..., ℓ})

**Insertion**: For element x, increment counter at position hi(x) by increment vgi(x) ∈ D

**Query**: For element y, check if counter value c at hi(y) could contain increment vgi(y):
- If c - vgi(y) cannot be formed as a sum from D, then y ∉ S (definitely not in set)
- Otherwise, continue checking next hash function

### The L Parameter and Increment Range [L, 2L-1]

**Set Definition**: DL = [L, 2L-1] = {L, L+1, ..., 2L-1} where L = 2^i

**Why This Range Works** (from Lemma 1):
- **c = 0**: No elements hashed to this position → y ∉ S
- **c ∈ [L, 2L-1]**: Sum must be single element (minimum of 2 elements = 2L). If c ≠ vgi(y), then y ∉ S
- **c ∈ [2L, 3L-1]**: Sum of exactly 2 elements. Can deduce if vgi(y) is part of sum
- **c ≥ 3L**: Cannot exclude membership (counter "saturated" for this check)

**Practical Value**: L = 4 recommended, giving D4 = {4, 5, 6, 7}
- Requires only 3 bits to encode increment (log2(7) = 3)
- Counters use 7-8 bits instead of 4 bits in CBF (but half as many counters needed)
- **No lookup table required** - simple comparison: is (c - v) = 0 or (c - v) ≥ L?

### Bh Sequences

**Definition**: Set D = {v1, v2, ..., vℓ} is a Bh sequence if all sums of h elements (with repetition) are distinct.

**Example B3 Sequence**: D = {1, 4, 8, 13}
- All sums of 3 elements are unique: {3, 6, 9, 10, 12, 13, 15, 16, 17, 18, 20, 21, 22, 24, 25, 27, 29, 30, 34, 39}
- If counter = 25 and only 3 elements hashed, can deduce sum is 4+8+13

**Bh-CBF**: Uses paired counters:
- c1: Fixed increment (counts number of elements)
- c2: Variable increment (weighted sum from Bh sequence)
- Query checks if c2 can be decomposed with vgi(y) given exactly c1 elements
- Requires lookup table of size h² · vℓ bits

### Hash Function Implementation

**Hashing Trick**: Only needs 2k hash functions instead of k:
- Traditional CBF: k hash functions, each requiring ⌈log2(m)⌉ random bits
- VI-CBF: k functions for positions + k functions for increments
  - Position hashes: ⌈log2(m)⌉ bits each (but m is halved, so -1 bit)
  - Increment hashes: ⌈log2(L)⌉ bits each (typically 2 bits for L=4)
  - Total: k · (⌈log2(m)⌉ + 1) bits vs. k · ⌈log2(m)⌉ for CBF

## Key Findings/Results

### Theoretical Bounds

**Theorem 2 - False Positive Rate**: For VI-CBF with D = DL:
```
FPR = [1 - (1 - 1/m)^(nk) - ((L-1)/L) · (1 - 1/m)^(nk-1)
       - ((L-1)(L+1))/(6L²) · (1 - 1/m)^(nk-2)]^k
```

**Theorem 3 - Comparison to CBF** (same memory budget):
1. VI-CBF achieves lower FPR than CBF for m ≥ 10 counters
2. As memory increases (α → ∞), FPR ratio VI-CBF/CBF → 0
3. Overflow probability is lower (37 insertions to overflow 8-bit counter with max increment 7, vs. 16 for 4-bit CBF)

### Experimental Results (Real OC192 Backbone Traces)

**Memory Savings** (for FPR = 10^-3, n = 2000 elements):
- Standard CBF: 14.1 KB
- VI-CBF (D = DL): 10.97 KB (**22% reduction**)
- VI-CBF (general D): 9.31 KB (**34% reduction**)
- ML-VI-HCBF (hierarchical): 6.60 KB (**53% reduction**, 2x improvement)

**False Positive Rate Improvements** (same memory):
- At 30 bits/element: CBF = 2.8%, VI-CBF = 0.38% (**7.3x improvement**)
- At 50 bits/element: CBF = 0.26%, VI-CBF = 0.011% (**23x improvement**, >10x)

**Optimal L Value**: L = 4 and L = 8 perform best
- L = 2: Too small, limited discrimination
- L = 16: Too large, requires more counter bits, reduces total counters

### Hardware Implementation

**Pipeline Complexity**: Constant per-element operations
- CBF: Hash → Lookup → Compare to 0
- VI-CBF: 2 Hashes → Lookup → Subtract and compare

**Memory Throughput**: Compatible with Blocked Bloom Filter technique (single memory word access per element)

**Lookup Table Requirements**:
- VI-CBF with D = DL: **No table needed** (simple arithmetic check)
- VI-CBF with general D: ~225 bits (0.03 KB) for D = {8, 12, 14, 15}
- Bh-CBF: h² · vℓ bits (e.g., 135 bits for h=3, max value 15)

## Relevance to Project

### Direct Implementation Connection

The **CountingGloBiMap** project already implements a version inspired by this paper:

1. **Variable-Increment CBF** (`include/variable_increment_bf.hpp`):
   - Implements the simple VI-CBF with D = [L, 2L-1]
   - Configuration: `VICBFConfig{k, logsize, counter_bits, L}`
   - **WARNING in project**: Provides 4-5x overcounting for frequency estimation (310% error)
   - Recommended usage: **Membership testing only** via `get_bool()`, NOT frequency via `get_min()`

2. **Why Overcounting Occurs**:
   - Paper's variable increments are intended for **membership testing**, not exact counting
   - Increment range [L, 2L-1] means each insertion adds between L and 2L-1 to counters
   - For frequency estimation, this random increment causes systematic overestimation
   - Example from paper: True count = 1, but counter shows ~4-7 (one increment ∈ [4,7])

### Key Insights for Project Improvements

**For Membership Testing**:
- VI-CBF is excellent when only need to know "is element present?"
- 22-34% memory savings vs. standard CBF at same FPR
- Hardware-friendly with no lookup tables needed (D = DL)

**For Frequency Estimation** (cardinality):
- Use **Spectral BF (MI variant)** or **Count-Min Sketch** instead
- These provide conservative updates without variable increments
- Project correctly implements these alternatives for accurate counting

**Multi-Layer Compression**:
- ML-VI-HCBF technique could be applied to CountingGloBiMap
- Current implementation has multi-layer cascading (1-bit → 8-bit → 16-bit → 32-bit → 64-bit)
- Could potentially add variable increments to layers for additional compression
- Trade-off: Memory savings vs. counting accuracy (would need conservative update variants)

### Implementation Considerations

**Hash Function Optimization**:
- Paper uses double hashing: h[i] = (h1 + i·h2) mod m
- Project already uses similar MurmurHash-based approach in `include/hashfn.hpp`
- Only needs k·(⌈log2(m)⌉ + 1) random bits vs. k·⌈log2(m)⌉

**Error Correction Potential**:
- VI-CBF's ability to detect "impossible sums" could complement GloBiMap's error correction
- Could reduce false positives in spatial rasterization queries
- Would need careful integration with existing cascade overflow mechanism

**Benchmark Comparison Opportunity**:
- Add VI-CBF to `experiments/src/compare_all_implementations.cpp`
- Compare against Spectral BF, Count-Min Sketch for membership-only workloads
- Show memory/FPR trade-off curves as in paper's Figure 4
