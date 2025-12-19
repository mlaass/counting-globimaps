# Bloom Filters via d-Left Hashing and Dynamic Bit Reassignment

**Authors:** Flavio Bonomi (Cisco Systems), Michael Mitzenmacher (Harvard University), Rina Panigrahy (Stanford University), Sushil Singh (Cisco Systems), George Varghese (UC San Diego and Cisco Systems)

**Venue/Year:** Extended Abstract, 2006 (likely presented at a networking or algorithms conference)

**PDF:** aller2006.pdf

## Abstract/Overview

This paper introduces a novel Bloom filter construction based on d-left hashing combined with dynamic bit reassignment. The key innovation is allowing fingerprint sizes to flexibly change based on bucket load, rather than using fixed-size fingerprints. This approach yields fewer false positives than standard Bloom filters for sufficiently large filters (starting at 16 bits per element), while maintaining similar implementation complexity suitable for hardware deployment.

## Key Contributions

- **Dynamic Bit Reassignment**: Introduced a technique where fingerprint sizes within a bucket vary dynamically based on the number of elements in that bucket (e.g., a bucket with 4 elements gets 15-bit fingerprints from 60 bits total, while a bucket with 6 elements gets 10-bit fingerprints)

- **d-Left Bloom Filter (d-left BF)**: A new Bloom filter variant that outperforms standard Bloom filters at ≥16 bits per element, achieving ~2x better false positive rates at 16 bits/element and ~3x better at 20 bits/element

- **Semi-Sorting Optimization**: A clever bit-saving trick that tracks the number of fingerprints beginning with 0 (or 00/01/10/11), allowing implicit storage of the first bit(s) of fingerprints, effectively adding 1-2 bits to fingerprint length without additional space

- **Dynamic d-Left Counting Bloom Filter (ddlCBF)**: Extended the d-left CBF construction from prior work with dynamic bit reassignment, achieving ~3x improvement in false positive rates over the static dlCBF

- **Hardware-Friendly Design**: All techniques are designed for practical hardware implementation using 64-bit or 128-bit memory blocks with simple bit-level operations

## Algorithm/Data Structure Details

### d-Left Hashing Foundation

The construction uses d-left hashing with 3 subtables (found empirically optimal):
- Hash table partitioned into 3 equally-sized subtables
- Each element hashes to one bucket in each subtable
- Element placed in the least-loaded bucket (power of two choices)
- With average load of 4 per bucket, maximum load is 6 with high probability (overflow probability < 10^-30)

### Dynamic Bit Reassignment Mechanism

**Basic Configuration (64-bit buckets):**
- 60 bits for fingerprints, 4 bits for counter
- Fingerprint size f(a) = 60/a where a is the current bucket load
- Bucket with 4 elements: each fingerprint gets 15 bits
- Bucket with 6 elements: each fingerprint gets 10 bits

**Semi-Sorting Optimization:**
- Track number of fingerprints beginning with 0 (the "0-count")
- Keep fingerprints semi-sorted (0-prefixed fingerprints first)
- First bit becomes implicit, effectively adding 1 bit to fingerprint
- For 4-bit counter: track 0-count only for loads 4 and 5 (most common cases)
- For 8-bit counter (128-bit buckets): can track first 2 bits (00/01/10/11 counts)

**Bucket Grouping:**
- Group multiple buckets (e.g., 4 buckets) to share bits
- Allows fuller buckets to "borrow" bits from less-full buckets
- Natural fit for hardware where memory reads are in large blocks (e.g., 256 bits)
- Provides small but non-trivial gains in false positive rates

### False Positive Calculation

For d-left Bloom filter with dynamic bit reassignment:

```
F = Σ_a Q_1a · a · 2^(-f(a)) + Σ_b Q_2b · b · 2^(-f(b)) + Σ_c Q_3c · c · 2^(-f(c))
```

Where:
- Q_ij = fraction of buckets in subtable i with load j
- f(x) = fingerprint size when bucket load is x
- Each subtable has different load distribution due to tie-breaking

### Implementation Details

**Insertion:**
1. Hash element to 3 buckets (one per subtable)
2. Check if fingerprint already exists in any bucket (optional optimization)
3. Place fingerprint in least-loaded bucket
4. All fingerprints in that bucket shrink proportionally

**Lookup:**
1. Hash element to 3 buckets in parallel
2. Search each bucket for matching fingerprint
3. Return true if found in any bucket

**Hashing:** Uses significantly less hashing than standard Bloom filters (3 hash functions vs. 11-14 for equivalent false positive rates), unless double-hashing techniques are employed.

## Key Findings/Results

### Performance at 16 Bits per Element (64-bit buckets)

| Implementation | False Positive Rate | Improvement |
|----------------|---------------------|-------------|
| Standard Bloom filter (k=11) | 0.000459 | Baseline |
| d-Left BF (basic, fixed fingerprints) | 0.000894 | 2x worse |
| d-Left BF + dynamic reassignment | 0.000894 | 2x worse |
| d-Left BF + dynamic + semi-sorting | **0.000448** | **~2% better** |

### Performance at 20 Bits per Element (128-bit buckets)

| Implementation | False Positive Rate | Improvement |
|----------------|---------------------|-------------|
| Standard Bloom filter (k=14) | 0.0000671 | Baseline |
| d-Left BF + dynamic + semi-sorting (1-bit) | 0.0000426 | 1.6x better |
| d-Left BF + dynamic + semi-sorting (2-bit) | **0.0000225** | **~3x better** |
| d-Left BF + grouping (4 buckets) | 0.0000204 | ~3.3x better |

### Load Distribution (3-Left Hashing, avg load = 4)

| Load | 0 | 1 | 2 | 3 | 4 | 5 | 6 | 7 |
|------|---|---|---|---|---|---|---|---|
| Fraction | 2.3e-05 | 6.0e-04 | 1.1e-02 | 1.5e-01 | **66%** | 18% | 2.3e-05 | 5.6e-31 |

The highly concentrated distribution around load 4-5 is critical for dynamic bit reassignment effectiveness.

### Counting Bloom Filter Results (ddlCBF)

Experimental setup: 4 subtables, 2048 buckets each, average load 6, 120 bits for fingerprints, ~1 Mbit total size, tested after 2^20 random delete/insert operations.

| Implementation | False Positive Rate | Improvement |
|----------------|---------------------|-------------|
| d-Left CBF (static) | 0.001459 | Baseline (2x better than standard CBF) |
| ddlCBF (single remainder size) | 0.000473 | 3x better than dlCBF |
| ddlCBF (dual remainder sizes) | 0.000311 | 4.7x better than dlCBF |

### Implementation Complexity

- **Hashing:** 3 hash functions vs. 11-14 for standard Bloom filters (unless double-hashing used)
- **Parallelization:** Trivially parallelizable (3 independent subtable lookups)
- **Bit Operations:** More complex than standard Bloom filter, but still practical for hardware with 64/128-bit blocks
- **Trade-off Point:** Becomes competitive at 16 bits/element; unlikely to beat standard Bloom filters at ≤8 bits/element where standard BF is near-optimal

## Relevance to Project

### Direct Applications to CountingGloBiMap

1. **Multi-Layer Design Inspiration**: The CountingGloBiMap's cascading layer design (1-bit → 8-bit → 16-bit → 32-bit) shares philosophical similarities with dynamic bit reassignment - both adapt storage to count magnitudes rather than using fixed allocations.

2. **Cache-Friendly Architecture**: The d-left hashing approach with bucket-based organization is inherently cache-friendly, similar to the cache-optimal bloom filters (BlockedBloomFilter, RegisterBlockedBF) already implemented in this project.

3. **Potential Hybrid Approach**: Could combine d-left hashing with the existing CountingGloBiMap layer structure for improved cache locality while maintaining counting capability.

4. **Semi-Sorting Technique**: The semi-sorting bit-saving trick could potentially be adapted to CountingGloBiMap's lower layers (1-bit and 8-bit) to save metadata bits or enable longer hash masks.

### Comparison with Existing Implementations

**Already Implemented:**
- **d-Left Counting Bloom Filter** (`dleft_counting_bf.hpp`) - Based on [Bonomi et al. 2006], implements the foundational d-left hashing with fingerprints, but uses *static* fingerprint sizes (this paper's improvement)

**Potential Enhancement:**
- Could implement a **dynamic d-left CBF** variant that incorporates dynamic bit reassignment and semi-sorting from this paper to improve the existing dleft_counting_bf.hpp by ~3x in false positive rate

**Relevant to:**
- **Spectral Bloom Filter** - Both use variable counter strategies
- **BlockedBloomFilter** - Similar cache-line aligned bucket approach
- **Count-Min Sketch** - Alternative to CBF, but doesn't use d-left hashing

### Key Insights for Spatial Data (GDELT, COVID-19)

1. **Underloaded Performance**: The paper emphasizes that d-left BF performs "quite well when underloaded" - highly relevant for sparse spatial data where many spatial bins remain empty

2. **Hardware Implementation**: Designed for hardware with 64/128-bit memory blocks - aligns well with modern CPU cache lines (64 bytes) for spatial grid processing

3. **False Positive Reduction**: At 20 bits/element, achieves 3x better FPR than standard Bloom filter - could significantly improve accuracy for spatial cardinality estimation

4. **Batch Operations**: Mentions potential for "balancing batches in an offline fashion" - relevant for preprocessing large spatial datasets like GDELT

### Research Directions

1. **Spatial d-Left GloBiMap**: Combine d-left hashing bucket structure with spatial rasterization for cache-friendly spatial queries

2. **Dynamic Layer Thresholds**: Adapt the CountingGloBiMap cascade_factor based on per-bucket load distributions, inspired by dynamic bit reassignment

3. **Multi-Category Optimization**: Use bucket grouping technique to improve category isolation while reducing memory overhead

4. **Benchmark Comparison**: Add dynamic d-left CBF to the existing comparison suite alongside the 10 current implementations
