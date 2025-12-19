# Performance-Optimal Filtering: Bloom Overtakes Cuckoo at High Throughput

**Authors:** Harald Lang, Thomas Neumann, Alfons Kemper (Technical University of Munich), Peter Boncz (Centrum Wiskunde & Informatica)

**Venue/Year:** PVLDB, 12(5): 502-515, 2019

**PDF:** p502-lang.pdf

## Abstract/Overview

This paper introduces the concept of **performance-optimal filtering** to determine which Bloom or Cuckoo filter configuration best accelerates a particular workload. While traditional filter research focuses on space-precision tradeoffs, this work demonstrates that the optimal filter choice depends on multiple factors: the false-positive rate, memory footprint, absolute lookup cost, work saved per lookup, and the actual rate of negative lookups in the workload. The key finding is that in high-throughput scenarios (where filters avoid CPU cache misses, network messages, or local disk I/O), blocked Bloom filters outperform Cuckoo filters despite having higher false-positive rates, due to their significantly lower lookup costs.

## Key Contributions

- **Formal definition of performance-optimal filtering** based on overhead minimization: ρ(F) = t_l(F) + f(F) × t_w
- **Two new Bloom filter variants**:
  - **Register-blocked Bloom filters**: Blocks reduced to 32-bit or 64-bit CPU register size for extreme CPU efficiency
  - **Cache-sectorized Bloom filters**: Spreads bits across entire cache lines while maintaining sequential access patterns
- **Magic modulo technique**: Allows arbitrary filter sizes (not just powers of two) using multiply-shift operations instead of expensive division, enabling finer-grained memory control
- **SIMD implementations**: AVX2/AVX-512 optimized implementations for both Bloom and Cuckoo filters achieving up to 10x speedups
- **Extensive experimental analysis**: Over 15 million experiments across 4 hardware platforms (Intel Xeon, Knights Landing, Skylake-X, AMD Ryzen) establishing skylines of performance-optimal configurations
- **Open-source release**: All code and experiments published for reproducibility

## Algorithm/Data Structure Details

### Blocked Bloom Filters

**Basic Concept**: Split Bloom filter into equally-sized blocks (typically 512 bits = cache line size). Each key sets all k bits within a single block, ensuring at most one cache miss per operation.

**Hash Bit Reduction**: Reduces required hash bits from k·log₂(m) to k·log₂(B) + log₂(m/B), improving computational efficiency.

**False-positive rate** (Equation 3):
```
f_blocked(m,n,k,B) = Σ(i=0 to ∞) Poisson(i, n/m × B) × f_std(B, i, k)
```

### Register-Blocked Bloom Filters

**Innovation**: Reduce block size to 32-bit or 64-bit (native register size), allowing all k bits to be tested in a single comparison instruction.

**Tradeoff**: Sacrifices memory bandwidth efficiency (only 1/8 or 1/16 of cache line accessed) and precision for maximum CPU efficiency.

**Performance**: Best for high-throughput, CPU cache-resident filters.

### Cache-Sectorized Bloom Filters

**Problem**: Standard sectorization requires k ≥ number of sectors, making 512-bit blocks impractical (would need k ≥ 16 for 32-bit words).

**Solution**: Partition blocks into word-sized sectors, then logically group sectors. For a key, set k/z bits in each of z groups, with bits concentrated in one sector per group.

**Advantages**:
- Spreads bits across entire cache line (better false-positive rate)
- Supports lower k values (k must be multiple of z, not total sectors)
- Maintains sequential access pattern within groups
- Achieves up to 48% overhead reduction vs. plain sectorization

### Magic Modulo Implementation

**Standard approach**: Restrict sizes to powers of 2, use bitwise AND instead of modulo (hash(key) & mask).

**Magic modulo**: Replace modulo operation with multiply-shift sequence:
```
i = h - (mulhi_u32(h, magicNo) >> shiftAmount) × h
```

**Benefits**:
- Allows arbitrary filter sizes (overhead ≤ 0.0134%)
- Avoids performance cliffs at cache boundaries
- Works with SIMD (no integer division needed)
- Enables fine-grained memory budget control

## Key Findings/Results

### Performance Skylines

**High-throughput scenarios** (t_w ≤ 10³-10⁵ cycles):
- **Bloom filters dominate** due to lower lookup costs (2-4 cycles per lookup)
- Register-blocked (B=32, k=3-5) for smallest filters
- Cache-sectorized (B=512, k=6-8, z=2) for larger filters
- Up to **3x faster than Cuckoo** filters

**Low-throughput scenarios** (t_w > 10⁵ cycles):
- **Cuckoo filters dominate** due to lower false-positive rates (0.00005-0.0002 vs. 0.0001-0.01 for Bloom)
- Signature length l=16, bucket size b=2 performance-optimal
- Up to **5x faster than Bloom**

### SIMD Performance

**Speedups over scalar implementations**:
- Intel Skylake-X (AVX-512): 8-10x speedup
- Intel Knights Landing (AVX-512): 6-8x speedup
- Intel Xeon (AVX2): 4-6x speedup
- AMD Ryzen (AVX2): < 2x speedup (gather instruction poorly performing)

**Absolute performance** (L1-resident, single thread):
- Register-blocked Bloom: 1-2 cycles/lookup
- Cache-sectorized Bloom: 3-4 cycles/lookup
- Cuckoo filter: 4-6 cycles/lookup (AVX2)

### Configuration Recommendations

**Bloom filter (high-throughput)**:
- k ∈ {6, 8} for cache-sectorized (sweet spot)
- k ∈ {3, 4, 5} for register-blocked
- k > 11 never performance-optimal
- z = 2 (access 2 words per cache line) most common for cache-sectorization

**Cuckoo filter (low-throughput)**:
- l = 16 bits (maximize signature length)
- b = 2 (not b=4 as previously thought)

### Real-World Use Cases

**Bloom territory (high-throughput)**:
- Selective joins (cache miss per probe)
- Hash table lookups
- Network routing
- Object caches
- Distributed semi-joins

**Cuckoo territory (low-throughput)**:
- LSM-tree disk seeks (magnetic/NVMe)
- Cloud storage (S3, Parquet reads)
- Large data pushdown over network (100MB+)

## Relevance to Project

### Direct Application to CountingGloBiMap

1. **Cache-sectorization applicable**: The project's blocked Bloom filters (512-bit blocks) could benefit from cache-sectorization with z=2 or z=4, potentially reducing overhead by 15-48% for spatial query workloads.

2. **Register-blocking for hot paths**: For very high-throughput spatial membership testing, register-blocked variants (32/64-bit blocks) could provide 2-3x speedup.

3. **Magic modulo implementation**: The current project uses power-of-2 sizes. Adopting magic modulo would enable:
   - Finer-grained memory budgets (important for hierarchical layers)
   - Avoiding performance cliffs at cache boundaries
   - Better utilization of limited memory

4. **SIMD optimization opportunity**: The project uses OpenMP parallelization but likely lacks SIMD within threads. Adding AVX2/AVX-512 SIMD could provide 4-10x additional speedup for batch spatial queries.

### Performance-Optimal Filtering Model

The **overhead minimization formula** ρ(F) = t_l + f × t_w is directly applicable:

- **t_w for spatial applications**:
  - Avoiding rasterization of polygon: ~10²-10³ cycles (high-throughput → Bloom)
  - Avoiding disk I/O for shapefile access: ~10⁶-10⁸ cycles (low-throughput → Cuckoo)
  - Avoiding network fetch for remote tile: ~10⁷-10⁹ cycles (low-throughput → Cuckoo)

### Connections to Existing Implementations

- **BlockedBloomFilter** (`blocked_bloom_filter.hpp`): Directly implements blocking but lacks sectorization
- **RegisterBlockedBloomFilter** (`register_blocked_bf.hpp`): Uses 64-bit registers similar to pattern-based approach
- **SimdBloomFilter** (`simd_bloom_filter.hpp`): Implements SIMD vectorization ideas

### Key Takeaway

**For high-throughput spatial cardinality estimation** (the project's core use case), **cache-sectorized Bloom filters with SIMD should outperform Cuckoo filters** despite higher false-positive rates, due to 2-3x lower lookup costs.
