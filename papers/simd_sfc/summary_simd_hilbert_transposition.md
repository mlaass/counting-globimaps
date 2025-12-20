# Cache-oblivious Hilbert Curve-based Blocking Scheme for Matrix Transposition

**Authors:** João Nuno Ferreira Alves, Luís Manuel Silveira Russo, Alexandre P. Francisco (INESC-ID, Portugal)

**Venue/Year:** ACM Transactions on Mathematical Software, Volume 48, Issue 4, December 2022

**PDF:** 3555353.pdf

## Abstract/Overview

This paper addresses out-of-place transposition of general rectangular matrices with a cache-oblivious approach. Traditional matrix transposition algorithms suffer from poor memory locality, causing excessive cache misses when one matrix is stored in row-major order but accessed in column-major order. The authors present a fast SIMD Hilbert space-filling curve generator that runs in O(N) time with constant memory overhead, combined with a cache-oblivious blocking scheme that partitions matrices into blocks traversed according to Hilbert or Z-order curves. The solution outperforms Intel MKL when combined with software prefetching techniques, providing a portable design that doesn't require hardware-specific tuning.

## Key Contributions

- **SIMD Hilbert Curve Generator**: O(N) incremental generation using bit-wise XOR operations with SSE/AVX512 vectorization. Achieves 3.68×10⁹ points/second, 6.90× faster than FUR-Hilbert.

- **Cache-Oblivious Blocking Scheme**: Partitions n×m matrix into 2×2 to 4×4 blocks traversed via Hilbert or Z-order curves. Reduces cache misses through spatial locality without requiring cache size knowledge.

- **Software Prefetching Integration**: Uses GCC `__builtin_prefetch()` with locality=3, achieving 31% speedup and 3.17× memory stall reduction.

- **Outperforms Intel MKL**: Achieves 6.75 GiB/s throughput (64% of STREAM COPY efficiency) on good-sized square matrices, surpassing MKL significantly.

- **Elegant Mathematical Foundation**: Rigorous proofs (Propositions 2-10) establish correctness of axiom swapping, quadrant transformations, and block boundary computation.

## Algorithm/Data Structure Details

### SIMD Hilbert Curve Generator (Algorithm 1)

Uses axiom swapping between iterations (H ↔ A) to avoid recomputation:

```
H_{l+1} = A_l ↑ xor₁(A_l) → xor₁(A_l) ↓ xor₂(A_l)
A_{l+1} = H_l → xor₁(H_l) ↑ xor₁(H_l) ← xor₂(H_l)
```

Key operations:
- `xor₁` (XOR by 1): Reflection along y=x axis
- `xor₂` (XOR by 2): 180° rotation

**Direction encoding**: Symbols {→, ↑, ←, ↓} encoded as base-4 digits {0, 1, 2, 3}, stored in compact bit-array where each `uint8_t` holds 4 directions (2 bits each).

**Movement computation**:
```cpp
j := j - ((d - 1) % 2)  // horizontal movement
i := i - ((d - 2) % 2)  // vertical movement
```

### Blocking Scheme (Algorithm 2)

- Partitions matrix into 2^(log₂(min(n,m))-1) × 2^(log₂(min(n,m))-1) blocks
- Block sizes range from 2×2 to 4×4 (proven by Proposition 7)
- Blocks traversed in order defined by Hilbert or Z-order curve
- Within each block, elements accessed in row-major order

**Block boundary computation** (Proposition 7):
```
System of equations:
2^(log₂(length)-1) = a + b
length = 2a + 3b  (if length ≤ (3/2)·2^(log₂(length)))
length = 3a + 4b  (otherwise)
```

### Data Structures

1. **Bit-array for curve storage**: Dynamic array of `uint8_t`, each storing 4 directions in little-endian format. Requires 4^(level-1) bytes for iteration `level`.

2. **Connecting directions (s₁, s₂, s₃)**: Stored as `uint8_t` variables, updated via XOR by bit-mask 0b01000000.

3. **SSE/AVX512 vectors**: `__m128i` for parallel XOR operations on multiple directions.

## Key Findings/Results

### Hilbert Curve Generation Performance

| Method | Points/Second | Speedup |
|--------|--------------|---------|
| SSE (this paper) | 3.68×10⁹ | baseline |
| Auto-vectorized | 3.51×10⁹ | 0.95× |
| AVX512 | ~2.6×10⁹ | 0.71× |
| FUR-Hilbert | 5.3×10⁸ | 0.14× (6.90× slower) |
| Non-recursive L-System | 1.3×10⁸ | 0.04× (27.63× slower) |

### Matrix Transposition Performance (Intel Xeon Gold 6130)

| Matrix Type | Throughput | Efficiency | Stall Reduction |
|-------------|-----------|------------|-----------------|
| Good square | 6.75 GiB/s | 64% | 3.17× |
| Ugly square | 6.15 GiB/s | 58% | 1.75× |
| Good rectangular | 6.47 GiB/s | 61% | 3.08× |

- **Prefetching speedup**: 28-31% improvement
- **Comparison**: Outperformed Intel MKL on good/ugly square and good rectangular matrices
- **STREAM COPY baseline**: 10.6 GiB/s

### Throughput Calculation
```
throughput = (3 × matrix_elements × sizeof(float)) / (2³⁰ × time) GiB/s
```
Factor of 3 accounts for 2 reads + 1 write per element.

## Relevance to Project

### Direct Applications to Spatial-Blocked Bloom Filter (SBBF)

1. **Space-Filling Curve Integration**: The SIMD Hilbert generator could be used for SBBF's spatial indexing, providing locality-preserving traversal during construction and queries. The proposal in `docs/` mentions using space-filling curves for block indexing.

2. **Cache-Oblivious Design**: The blocking scheme's principle—achieving good cache performance without knowing cache parameters—directly applies to CountingGloBiMap's multi-layer design when processing large datasets.

3. **SIMD Bit Manipulation**: The XOR-based direction encoding and SSE vectorization techniques could apply to bitmap operations in bloom filter implementations (`simd_bloom_filter.hpp`).

4. **Blocking for Spatial Queries**: The 2×2 to 4×4 block sizes could inspire spatial query patterns for GloBiMap's rasterization operations.

### Connections to Existing Implementations

- **SimdBloomFilter** (`simd_bloom_filter.hpp`): SIMD vectorization patterns are directly applicable
- **GloBiMap rasterization** (`globimap.hpp`): Hilbert curve traversal could improve cache behavior during spatial queries
- **SBBF proposal** (`docs/`): Paper validates space-filling curve approach for spatial locality

### Technical Insights

1. **Prefetching Effectiveness**: 31% speedup from software prefetching validates proactive memory access patterns for large data structures.

2. **Cache Miss Analysis**: Using PMU counters to measure memory stalls provides methodology for benchmarking bloom filter implementations.

3. **Z-order Degradation**: Z-order blocking degrades for matrices >12,100×12,100, suggesting Hilbert curves are superior for large-scale spatial applications.

### Limitations Noted

- Only addresses out-of-place operations (in-place transposition not covered)
- Matrix dimensions must be divisible by 4 (requires padding otherwise)
- Prefetching hurts performance on "ugly" rectangular matrices
- Memory overhead: Storing entire curve requires 2N bits

### Future Research Directions

1. **Hybrid Curve-Layer Design**: Combine Hilbert curve spatial locality with CountingGloBiMap's bit-depth layers
2. **Streaming Spatial Queries**: Apply prefetching patterns to GloBiMap polygon rasterization
3. **Multi-Category Locality**: Use space-filling curves for cache-efficient multi-category queries
4. **Parallelization**: Paper suggests shared-memory parallelization as future work—applicable to OpenMP-enabled CountingGloBiMap

**Code availability:** https://github.com/JoaoAlves95/HPC-Cache-Oblivious-Transposition
