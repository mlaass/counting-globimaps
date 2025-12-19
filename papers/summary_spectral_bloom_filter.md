# Spectral Bloom Filters

**Authors:** Saar Cohen, Yossi Matias
**Venue/Year:** SIGMOD 2003 (June 9-12, 2003, San Diego, CA)
**PDF:** sbf-sigmod-03.pdf

## Abstract/Overview

Spectral Bloom Filters extend classical Bloom filters from set membership testing to multiset operations, enabling frequency estimation (counting occurrences of elements). While standard Bloom filters answer "Is element x in the set?", Spectral Bloom Filters answer "How many times does element x appear?" The paper introduces three algorithmic variants that trade off between accuracy, memory efficiency, and support for deletions.

## Key Contributions

- **Multiset Extension**: Generalizes Bloom filters from binary (present/absent) to counting applications, supporting frequency queries on streaming data
- **Three Algorithmic Variants**:
  - **MS (Minimum Selection)**: Baseline algorithm - increment all k counters on insert, return minimum on query
  - **MI (Minimal Increment)**: Conservative update strategy that reduces overcounting by only incrementing counters at the minimum value
  - **RM (Recurring Minimum)**: Extends MI with deletion support using a secondary min-values filter
- **Theoretical Analysis**: Provides error bounds and space complexity analysis for each variant
- **Practical Applications**: Demonstrates use cases including aggregate queries, iceberg queries, joins (Spectral Bloomjoins), and bifocal sampling
- **Data Structures**: Novel encoding schemes including variable-length arrays, coarse offset vectors, and Elias encoding for space efficiency

## Algorithm/Data Structure Details

### Core Structure
- **Primary Storage**: Array of counters (8, 16, 32, or 64 bits per counter)
- **Size**: 2^logsize counters
- **Hash Functions**: k hash functions generated via double hashing: `h_i(x) = (h1(x) + i * h2(x)) mod size`
- **Query Method**: Return minimum counter value across k hash positions

### Variant 1: Minimum Selection (MS)
**Insert(x)**:
```
for i = 1 to k:
    counters[h_i(x)] += 1
```

**Query(x)**:
```
return min(counters[h_1(x)], ..., counters[h_k(x)])
```

**Characteristics**:
- Simplest variant, standard counting Bloom filter approach
- Prone to overcounting due to hash collisions
- No deletion support

### Variant 2: Minimal Increment (MI)
**Insert(x)** (Conservative Update):
```
min_val = min(counters[h_1(x)], ..., counters[h_k(x)])
for i = 1 to k:
    if counters[h_i(x)] == min_val:
        counters[h_i(x)] += 1
```

**Query(x)**:
```
return min(counters[h_1(x)], ..., counters[h_k(x)])
```

**Characteristics**:
- Reduces overcounting by only incrementing minimum-valued counters
- Based on conservative update principle from Estan & Varghese (2002)
- Significantly better accuracy than MS for frequency estimation
- Still no deletion support

### Variant 3: Recurring Minimum (RM)
**Data Structure**:
- Primary counters array (same as MI)
- Secondary min-values array (records minimum before each increment)

**Insert(x)**:
```
min_val = min(counters[h_1(x)], ..., counters[h_k(x)])
for i = 1 to k:
    minvals[g_i(x)] = min(minvals[g_i(x)], min_val)  // g_i are separate hash functions
    if counters[h_i(x)] == min_val:
        counters[h_i(x)] += 1
```

**Delete(x)**:
```
recorded_min = max(minvals[g_1(x)], ..., minvals[g_k(x)])
current_min = min(counters[h_1(x)], ..., counters[h_k(x)])
if recorded_min < current_min:
    for i = 1 to k:
        counters[h_i(x)] -= 1
```

**Characteristics**:
- Supports deletions by tracking historical minimum values
- Uses secondary filter with independent hash functions
- More complex but enables sliding window maintenance

### Space Optimization Techniques
1. **Variable-Length Counters**: Different bit widths (8/16/32/64) based on expected frequencies
2. **Coarse Offset Vectors**: Multi-level indexing for sparse arrays
3. **Elias Encoding**: Variable-length encoding for compact storage of counter values

## Key Findings/Results

### Experimental Setup
- Datasets: Real-world web query logs, synthetic Zipf distributions
- Metrics: Additive error, multiplicative error, false positive rate
- Compared against: Hash-based counting, Count-Min Sketch

### Performance Results
1. **Accuracy**:
   - **MI variant** provides ~2-5x better accuracy than MS for frequency queries
   - Error grows sub-linearly with stream size
   - Conservative update significantly reduces overcounting from hash collisions

2. **Space Efficiency**:
   - Spectral BF uses 50-75% less memory than hash tables for same accuracy
   - Counter overflow is rare with proper bit-width selection
   - MI variant achieves better accuracy per bit than MS

3. **Query Performance**:
   - Constant-time queries: O(k) hash operations
   - Cache-efficient due to compact representation
   - Deletion support (RM) adds ~2x overhead

4. **Applications**:
   - **Iceberg Queries**: Finding frequent items with threshold filtering
   - **Spectral Bloomjoins**: Semijoin optimization reducing network traffic by 40-60%
   - **Sliding Windows**: RM variant enables time-decay and window maintenance

### Theoretical Bounds
- **False Positive Probability**: Inherits from standard Bloom filter analysis
- **Additive Error**: Bounded by ε·N where N is total insertions, ε depends on k and filter size
- **Space Complexity**: O(n/ε) for n distinct elements with error bound ε

## Relevance to Project

### Direct Implementation
The CountingGloBiMap project implements all three Spectral Bloom Filter variants in `/home/moritz/workspace/counting-globimaps/include/spectral_bloom_filter.hpp`:

```cpp
enum SBFVariant {
    MINIMUM_SELECTION,    // MS: Standard min-count query
    MINIMAL_INCREMENT,    // MI: Conservative update (recommended)
    RECURRING_MINIMUM     // RM: Supports deletions
};
```

### Project Usage
1. **Multi-Category Support**: SBF handles variable-length point vectors for tracking events by category at spatial locations
2. **Frequency Estimation**: MI variant provides accurate cardinality estimation for sparse spatial data (GDELT, COVID-19 datasets)
3. **Memory Efficiency**: Compact counter arrays (8/16/32/64-bit) enable processing millions of spatial events
4. **Comparison Baseline**: Used in benchmarks against CountingGloBiMap, d-Left CBF, Count-Min Sketch

### Key Integration Points
- **Conservative Update**: MI variant's minimal increment strategy inspired CountingGloBiMap's `minimal_increment` option
- **Multi-Layer Design**: Spectral BF's single-layer counters contrast with CountingGloBiMap's cascading multi-layer hierarchy
- **Hash Function Design**: Double hashing scheme `h_i = (h1 + i*h2) mod size` used across all implementations
- **Error Bounds**: Provides theoretical foundation for comparing practical accuracy across filter types

### Performance in Project Benchmarks
From GDELT multi-category dataset (1.9M events):
- **Memory**: 2 MB (configurable)
- **Insert Time**: 0.21s (fastest in benchmark)
- **Query Time**: 0.50 μs per query
- **Accuracy**: 0% mean error with MI variant, perfect category isolation
- **Best Use Case**: Frequency estimation with minimal memory, when deletions not needed

### Comparison with Other Implementations
| Implementation | Deletion Support | Accuracy (MI) | Memory Efficiency | Use Case |
|----------------|------------------|---------------|-------------------|----------|
| Spectral BF (MI) | No | Excellent | High | Frequency queries, compact storage |
| Spectral BF (RM) | Yes | Excellent | Medium | Sliding windows, time-decay |
| CountingGloBiMap | No | Excellent | Medium | Multi-layer, varying magnitudes |
| Count-Min Sketch | No | Good | Highest | Provable error bounds |
| d-Left CBF | Yes | Good | High | Cache-friendly, deterministic |

---

**Implementation Reference**: `/home/moritz/workspace/counting-globimaps/include/spectral_bloom_filter.hpp`
**Test Suite**: `/home/moritz/workspace/counting-globimaps/tests/test_spectral_bloom_filter.cpp`
**Benchmark**: `experiments/src/globimap_test_multicategory_dataset.cpp`
