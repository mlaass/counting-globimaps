# Using Evolutionary Algorithms to Find Cache-Friendly Generalized Morton Layouts for Arrays

**Authors:** Stephen Nicholas Swatman, Ana-Lucia Varbanescu, Andy D. Pimentel, Andreas Salzburger, Attila Krasznahorkay

**Venue/Year:** ICPE '24 (15th ACM/SPEC International Conference on Performance Engineering), May 7-11, 2024, London, United Kingdom

**PDF:** ICPE24.pdf

**DOI:** https://doi.org/10.1145/3629526.3645034

## Abstract/Overview

This paper presents a novel approach to optimizing cache performance for multi-dimensional array layouts using evolutionary algorithms. The authors explore a large family of array layouts based on generalized Morton curves (Z-order curves) and demonstrate that genetic algorithms can efficiently discover cache-friendly layouts that significantly outperform canonical row-major and column-major layouts.

The core insight is that the classical Morton layout, which provides balanced locality in multiple dimensions through bit-interleaving of array indices, can be generalized by varying the order in which bits are interleaved. This creates an enormous design space of possible layouts with widely varying cache performance characteristics. Rather than exhaustively searching this space—which becomes computationally infeasible for realistic array sizes—the authors propose using genetic algorithms guided by a cache simulation-based fitness function.

The methodology is validated across eight real-world computational kernels (matrix multiplication variants, stencil computations, and matrix decompositions) on two different processor architectures (Intel Haswell and AMD Zen 3). Results demonstrate that the evolutionary approach successfully discovers layouts with significant performance improvements, achieving speedups of up to 10× in extreme cases (Crout decomposition) and 63-294% for matrix multiplication kernels, while maintaining computational overhead comparable to canonical layouts.

## Key Contributions

1. **Characterization of generalized Morton layout family**: Formalization of a very large family of multi-dimensional array layouts based on arbitrary bit-interleaving patterns, generalizing the classical Morton order. For a 4096×4096 array, this yields 2,704,156 possible layouts; for a 256×256×256 3D array, 9,465,511,770 layouts.

2. **Chromosomal representation for layouts**: Development of a sound and complete encoding scheme for Morton-like layouts using sequences [i₀, ..., iₙ₋₁] that indicate which input dimension contributes each bit in the output address, enabling efficient genetic algorithm operations.

3. **Cache simulation-based fitness function**: Design of a fitness function using pycachesim that estimates cache performance without requiring execution on target hardware, enabling hardware-software co-design scenarios and reducing noise from cache pollution effects.

4. **Validation of correlation between simulation and reality**: Empirical demonstration that the simulation-based fitness function correlates moderately to strongly (Spearman's ρₛ ranging from -0.405 to -0.953) with actual execution time on real hardware across diverse kernels.

5. **Efficient hardware implementation**: Demonstration that generalized Morton layouts can be computed efficiently using BMI2 instruction set extensions (PDEP instruction), with computational cost within 1 cycle of canonical layouts on Intel Haswell and AMD Zen 3 architectures.

6. **Proof-of-concept evolutionary methodology**: Implementation and evaluation of a (λ, μ)-ES genetic algorithm with ordered crossover (OX) and inversion-based mutation that successfully discovers high-fitness layouts in 4 out of 8 test kernels within 20 generations.

7. **Real-world performance validation**: Demonstration of significant performance improvements on actual hardware: 63-294% speedup for matrix multiplication, 30-113% for transposed matrix multiplication, 1-4% for Cholesky decomposition, and 263-1007% for Crout decomposition.

8. **SIMD compatibility analysis**: Analysis showing that generalized Morton layouts can support vectorization by constraining the least significant bits to come from a single dimension, enabling efficient SIMD operations while maintaining improved cache behavior.

## Algorithm/Data Structure Details

### Generalized Morton Layout Definition

**Classical Morton Layout (2D example):**
```
Index (3, 5) = (011₂, 101₂)
Interleave bits: 100111₂ = 39₁₀
```

**Generalized Morton Layout:**
Instead of fixed round-robin bit interleaving, allow arbitrary interleaving patterns. Example 3D layout with custom pattern:
```
f(011₂, 101₂, 100₂) =
    00001100₂  (from index 1, bits selected)
  ∨ 00010000₂  (from index 2, bits selected)
  ∨ 10000000₂  (from index 0, bits selected)
  = 10011100₂ = 313₁₀
```

### Layout Characterization Scheme

**Notation:** Layouts denoted as sequences [i₀, i₁, ..., iₙ₋₁] where:
- iⱼ indicates which input dimension contributes the j-th bit of output address
- Least significant bit first (j=0 is LSB)
- Each input dimension contributes exactly as many bits as needed for its size

**Example for 8×8 array (3 bits per dimension):**
- Row-major: [0,0,0,1,1,1] - all x-bits before y-bits
- Column-major: [1,1,1,0,0,0] - all y-bits before x-bits
- Standard Morton: [0,1,0,1,0,1] - alternating bits

**Design space size:** For n-dimensional array with bᵢ bits in dimension i:
```
|layouts| = (Σᵢ₌₀ⁿ⁻¹ bᵢ)! / Πᵢ₌₀ⁿ⁻¹(bᵢ!)
```

Examples:
- 4×4 (2D): (2+2)!/(2!·2!) = 6 layouts
- 4096×4096 (2D): (12+12)!/(12!·12!) = 2,704,156 layouts
- 256×256×256 (3D): (8+8+8)!/(8!·8!·8!) = 9,465,511,770 layouts

### Hardware-Accelerated Index Calculation

**Canonical Layout Implementation:**
```cpp
// n-dimensional canonical layout
address = Σᵢ₌₀ⁿ⁻¹ (Πⱼ₌₀ⁱ⁻¹ Nⱼ) · xᵢ

// Requires n-1 multiplications + n-1 additions
// Latency: 3 cycles (IMUL), throughput: 1 cycle
```

**Generalized Morton Layout Implementation:**
```cpp
// Using BMI2 PDEP (Parallel Bit Deposit) instruction
// For each dimension i, deposit bits to specified positions
address = PDEP(x₀, mask₀) | PDEP(x₁, mask₁) | ... | PDEP(xₙ₋₁, maskₙ₋₁)

// Requires n PDEP operations + n-1 OR operations
// PDEP latency: 3 cycles, throughput: 1 cycle (same as IMUL)
// OR latency: 1 cycle, throughput: 0.25 cycles
```

**Performance comparison (via OSACA static analysis):**
- Canonical layouts: ~2-10 cycles per iteration (depending on dimensions)
- Morton layouts: ~3-11 cycles per iteration (1 cycle overhead)
- Overhead is small relative to cache miss savings

**Hardware support:**
- Intel: Haswell (2013) and later
- AMD: Full hardware support since Zen 3 (2020), microcode emulation in earlier CPUs

### Genetic Algorithm Configuration

**Algorithm Type:** (λ, μ)-Evolution Strategy

**Parameters:**
- Population size: μ = 20
- Offspring per generation: λ = 20
- Mutation rate: 25%
- Generations: 20
- Initial population: 2 canonical layouts (row-major and column-major)

**Chromosomal Representation:**
Direct encoding as layout sequence [i₀, i₁, ..., iₙ₋₁]

**Crossover Operator:** Ordered Crossover (OX)
```
Parent1: [0,1,0,1,0,1]
Parent2: [0,0,0,1,1,1]

1. Select crossover points: |[0,1,|0,1|,0,1]|
2. Copy middle segment from Parent1 to Child
3. Fill remaining positions from Parent2 in order

Child:   [0,0,0,1,1,1] (example result)
```

**Mutation Operator:** Inversion-based mutation
```
Original: [0,1,0,1,0,1]

1. Select random segment: [0,|1,0,1|,0,1]
2. Reverse the segment:    [0,|1,0,1|,0,1]

Mutated:  [0,1,0,1,0,1] (segment reversed)
```

**Selection Strategy:** Fitness-based selection (higher fitness = better cache performance)

### Fitness Function Design

**Cache Simulation Framework:** pycachesim (from Kerncraft toolkit)

**Simulation Process:**
1. Trace all memory load/store operations for a given kernel
2. Simulate cache hierarchy (L1, L2, L3, main memory)
3. Count hits and misses at each level
4. Compute weighted cycle count based on latencies

**Cycle Calculation:**
```
C(I; A, H) = Mₕᵢₜ(I; A, H) · Mₗₐₜ(H) + Σᵢ L^i_hit(I; A, H) · L^i_lat(H)

where:
  I = array layout
  A = access pattern
  H = cache hierarchy
  Mₕᵢₜ = main memory hits
  Mₗₐₜ = main memory latency
  L^i_hit = L-i cache hits
  L^i_lat = L-i cache latency
```

**Fitness Function (higher is better):**
```
F(I; A, H) = (L1_hit(I; A, H) + L1_miss(I; A, H)) / (L1_lat(H) · C(I; A, H))

Interpretation:
  Numerator = total memory accesses
  Denominator = total cycles to retrieve data (normalized by L1 latency)
  Result ≈ memory accesses per cycle
  Bounded by 1/L1_lat (achieved when all accesses hit L1)
```

**Advantages of simulation-based fitness:**
- Deterministic (no run-to-run variance from cache pollution)
- No need for target hardware access
- Enables hardware-software co-design for future processors
- Fast evaluation compared to actual execution

**Disadvantages:**
- Not cycle-accurate
- Ignores computation time
- Simplified cache models (assumes LRU replacement)
- Imperfect correlation with reality

### Cache Hierarchy Modeling

**Intel Xeon E5-2660 v3 (Haswell) Configuration:**
```yaml
L1: 32 KB, 8-way, 64B line, LRU, latency: 4 cycles
L2: 256 KB, 8-way, 64B line, LRU, latency: 12 cycles
L3: 25 MB, 16-way, 64B line, LRU, latency: 36 cycles
Memory: latency: 200 cycles
```

**AMD EPYC 7413 (Zen 3) Configuration:**
```yaml
L1: 32 KB, 8-way, 64B line, LRU, latency: 7 cycles (FP)
L2: 512 KB, 8-way, 64B line, LRU, latency: 12 cycles
L3: 32 MB, 16-way, 64B line, LRU, latency: 46 cycles
Memory: latency: 200 cycles
```

**Simplifications:**
- Real hardware uses tree-PLRU (Intel) or adaptive insertion (L3)
- Simulation assumes pure LRU for all levels
- Single-threaded model (no cache sharing effects)
- Optimistic latencies (fastest load-to-use path)

## Key Findings/Results

### Fitness Function Validation

**Correlation with Running Time (100 random layouts per kernel):**

**Intel Xeon E5-2660 v3:**
| Access Pattern | Pearson ρₚ | Spearman ρₛ |
|----------------|-----------|-------------|
| MMijk(9;4) | -0.672 | -0.480 |
| MMTijk(9,9;4) | -0.810 | -0.896 |
| MMikj(9;4) | -0.845 | -0.815 |
| MMTikj(9,9;4) | -0.777 | -0.744 |
| Jacobi2D(13,13;4) | -0.760 | -0.769 |
| Cholesky(10;4) | -0.827 | -0.953 |
| Crout(9;4) | -0.846 | -0.663 |
| Himeno(8,7,7;4) | -0.607 | -0.475 |

**AMD EPYC 7413:**
| Access Pattern | Pearson ρₚ | Spearman ρₛ |
|----------------|-----------|-------------|
| MMijk(9;4) | -0.648 | -0.489 |
| MMTijk(9,9;4) | -0.863 | -0.823 |
| MMikj(9;4) | -0.800 | -0.838 |
| MMTikj(9,9;4) | -0.291 | -0.405 |
| Jacobi2D(13,13;4) | -0.390 | -0.428 |
| Cholesky(10;4) | -0.725 | -0.892 |
| Crout(9;4) | -0.213 | -0.704 |
| Himeno(8,7,7;4) | -0.561 | -0.496 |

**Key Observations:**
- Moderate to strong negative correlation (negative because lower time = higher fitness)
- Stronger correlation for Cholesky and matrix multiplication kernels
- Weaker correlation for AMD Zen 3 in some cases (MMTikj, Jacobi2D, Crout)
- Rank correlation (Spearman) often stronger than linear (Pearson), indicating monotonic but non-linear relationship
- Sufficient correlation to enable effective genetic algorithm guidance

### Evolutionary Algorithm Performance

**Successful Evolution (4 out of 8 kernels):**

**Fitness Improvements over Canonical Layouts:**

| Kernel | Intel E5-2660 v3 | AMD EPYC 7413 |
|--------|------------------|---------------|
| MMijk(11;4) | +149.8% | +187.5% |
| MMTikj(11,11;4) | +109.6% | +141.1% |
| Cholesky(12;4) | +26.4% | +36.8% |
| Crout(12;4) | +545.9% | +541.1% |

**Failed to Improve (4 out of 8 kernels):**
- MMTijk: No layouts found with higher fitness than canonical
- MMikj: No improvement discovered
- Jacobi2D: Evolution did not exceed canonical performance
- Himeno: No superior layouts found

**Convergence Characteristics:**
- High-fitness individuals discovered within first 5-10 generations
- Rapid initial improvement followed by plateau
- Population diversity maintained throughout 20 generations
- Mean fitness consistently improves over generations for successful cases

**Population Size = 20 appears adequate for:**
- Sufficient diversity for crossover
- Avoiding premature convergence
- Maintaining computational tractability

### Real-World Performance Results

**Speedups on Actual Hardware (Best Evolved vs. Best Canonical):**

**Intel Xeon E5-2660 v3:**
| Access Pattern | Best Canonical | Best Evolved | Speedup |
|----------------|---------------|--------------|---------|
| MMijk(11;4) | 17.84 s | 10.94 s | **63.1%** |
| MMTikj(11,11;4) | 18.13 s | 13.96 s | **29.9%** |
| Cholesky(12;4) | 11.84 s | 11.43 s | **3.6%** |
| Crout(12;4) | 158.54 s | 43.72 s | **262.6%** |

**AMD EPYC 7413:**
| Access Pattern | Best Canonical | Best Evolved | Speedup |
|----------------|---------------|--------------|---------|
| MMijk(11;4) | 37.71 s | 9.58 s | **293.8%** |
| MMTikj(11,11;4) | 32.35 s | 15.21 s | **112.6%** |
| Cholesky(12;4) | 9.72 s | 9.55 s | **1.0%** |
| Crout(12;4) | 232.84 s | 21.03 s | **1007.0%** |

**Key Findings:**
1. **Dramatic improvements for Crout decomposition**: Up to **10× speedup** on AMD Zen 3
2. **Strong gains for matrix multiplication**: 63% (Intel) to 294% (AMD) speedup
3. **Moderate improvements for transposed multiplication**: 30-113% speedup
4. **Minimal benefit for Cholesky**: Only 1-4% improvement
5. **AMD Zen 3 benefits more than Intel Haswell** in most cases
6. **Run-to-run variance low**: Coefficient of variation never exceeded 8%

**Why Zen 3 Benefits More:**
- Larger L2 cache (512 KB vs 256 KB) amplifies locality benefits
- Different cache replacement policies may interact differently with access patterns
- Higher baseline latency for some kernels provides more room for improvement

### Access Pattern Characterization

**Test Kernels Used:**

| Pattern | Description | Memory Size | Loads | Stores |
|---------|-------------|-------------|-------|--------|
| MMijk(m;s) | Matrix multiplication (ijk order) | 3·s·2²ᵐ B | 2·2³ᵐ | 2²ᵐ |
| MMTijk(m,n;s) | Multiply by transposed matrix | s·(2·2ᵐ⁺ⁿ + 2²ⁿ)B | 2·2²ᵐ⁺ⁿ | 2²ᵐ |
| MMikj(m;s) | Matrix mult (ikj order) | 3·s·2²ᵐ B | 3·2³ᵐ | 2³ᵐ |
| MMTikj(m,n;s) | Transposed mult (ikj order) | s·(2·2ᵐ⁺ⁿ + 2²ⁿ)B | 3·2²ᵐ⁺ⁿ | 2²ᵐ⁺ⁿ |
| Jacobi2D(m,n;s) | 4-point stencil | 2·s·2ᵐ⁺ⁿ B | ~4·2ᵐ⁺ⁿ | 2ᵐ⁺ⁿ |
| Cholesky(m;s) | Cholesky-Banachiewicz | 2·s·2²ᵐ B | 2·2²ᵐ | ~½·2²ᵐ |
| Crout(m;s) | Crout decomposition | 2·s·2²ᵐ B | 7/2·2²ᵐ | 2²ᵐ |
| Himeno(m,n,p;s) | 19-point stencil (3D) | 12·s·2ᵐ⁺ⁿ⁺ᵖ B | 24·2ᵐ⁺ⁿ⁺ᵖ | 2ᵐ⁺ⁿ⁺ᵖ |

**Why Different Kernels Respond Differently:**

**High Benefit (MMijk, Crout):**
- Multiple dimensions accessed non-sequentially
- Irregular access patterns benefit from balanced multi-dimensional locality
- Classical layouts perform poorly (factor 10× slower for Crout)

**Moderate Benefit (MMTikj):**
- Mixed sequential and non-sequential access
- Some dimensions benefit more than others from optimized layout

**Low Benefit (Cholesky, Jacobi2D):**
- Access patterns already exhibit reasonable locality in canonical layout
- Algorithm structure limits potential for layout optimization
- Computational intensity may dominate over memory access time

**No Benefit (MMTijk, MMikj, Himeno):**
- Access patterns may be inherently cache-friendly in canonical layout
- OR: Genetic algorithm failed to explore promising regions of design space
- OR: Fitness function doesn't accurately predict benefit for these patterns

### Visualization of Layout Space

**All 20 Possible 8×8 2D Layouts:**

The paper provides visualizations showing how elements are ordered in memory for all 20 distinct layouts of an 8×8 array. Notable examples:
- **[0,0,0,1,1,1]**: Row-major (all rows contiguous)
- **[1,1,1,0,0,0]**: Column-major (all columns contiguous)
- **[0,1,0,1,0,1]**: Standard Morton (balanced diagonal locality)
- **[0,1,1,0,0,1]** through **[1,0,1,1,0,0]**: Various intermediate patterns with different locality characteristics

Each layout produces distinct spatial patterns affecting how neighboring elements in the 2D grid are positioned in 1D memory, directly impacting cache line utilization.

## Relevance to CountingGloBiMap Project

The research on generalized Morton layouts has significant implications for the CountingGloBiMap project's performance optimization:

### Direct Applicability to Spatial Data Structures

**Current Usage in Project:**
CountingGloBiMap already uses space-filling curves implicitly through the hashing trick:
```cpp
// From include/counting_globimap.hpp
// Hashing converts spatial coordinates [x, y, category] to bloom filter positions
hash[i] = (h1 + (i+1) * h2) & mask
```

**Potential Improvements:**

1. **Explicit Morton Ordering for Rasterization:**
The GloBiMap rasterization feature (spatial queries over rectangular regions) could benefit from Morton-ordered storage:
```cpp
// Current: Implicit ordering through hashing
auto &raster = bf.rasterize(100, 100, 50, 50);

// Potential optimization: Store rasterized results in evolved Morton layout
// for better cache locality during spatial range queries
```

2. **Multi-dimensional Layer Access:**
The multi-layer structure (1-bit, 8-bit, 16-bit, 32-bit, 64-bit counters) could use generalized layouts for spatial locality:
```cpp
// Current: Separate vectors for each bit depth
std::vector<uint8_t> f8;
std::vector<uint16_t> f16;
std::vector<uint32_t> f32;

// Potential: Morton-ordered multi-dimensional arrays
// Dimension 0: spatial X
// Dimension 1: spatial Y
// Dimension 2: layer type (8-bit, 16-bit, 32-bit)
```

3. **3D Hilbert Curve Integration:**
The project has recent commits on 3D Hilbert encoding (commit e9db315). This paper's methodology could optimize 3D layouts:
- Project already implements Hilbert3D with 7.8× speedup using LUTs
- Could apply evolutionary approach to find cache-optimal 3D bit-interleaving patterns
- Especially valuable for 3D spatial datasets (x, y, time) or (x, y, category)

### Cache Optimization for Blocked Bloom Filters

**Synergy with Cache-Optimal Implementations:**

The project includes several cache-conscious bloom filter variants from `save-buffer/bloomfilter_benchmarks`:

```cpp
// From include/blocked_bloom_filter.hpp
// 256-bit blocks aligned to cache lines
// First hash selects block, remaining probe within block
```

**Generalized Morton layouts could enhance:**

1. **BlockedBloomFilter**: Organize blocks in Morton order for spatial queries
2. **RegisterBlockedBloomFilter**: Apply to 64-bit register access patterns
3. **SimdBloomFilter**: Layout data for optimal AVX2 gather instruction performance

**Example Application:**
```cpp
// Current SIMD implementation processes 8 elements in parallel
// Could optimize memory layout of those 8 elements using evolved pattern
// to maximize cache hit rate during batch operations
```

### Multi-Category Support Optimization

**Project Feature:**
```cpp
// From CLAUDE.md - Multi-category support
filter.put({100, 200, 1});  // Category 1 at (100, 200)
filter.put({100, 200, 2});  // Category 2 at (100, 200)
```

**Cache Optimization Strategy:**
The 3-dimensional space (x, y, category) creates a layout optimization opportunity:
- Standard approach: Interleave x, y, category bits round-robin
- Optimized approach: Use genetic algorithm to find best interleaving for GDELT/COVID workloads
- Could reduce cache misses when querying multiple categories at same location

### Dataset-Specific Tuning

**GDELT Events Dataset:**
- 1.9M events with QuadClass categories (4 categories)
- Spatial distribution: ~1000 locations with varying densities
- Access pattern: Likely exhibits spatial clustering (hotspots)

**COVID-19 Dataset:**
- 1.8M case events sampled at 1%
- Strong spatial hotspots (USA, India, Brazil)
- Access pattern: Very clustered in certain regions

**Optimization Strategy:**
1. Profile actual access patterns from experiments:
   ```bash
   ./build/globimap_test_multicategory_dataset
   ```
2. Use profiled access patterns as fitness function input
3. Run genetic algorithm to find GDELT-specific or COVID-specific layouts
4. Benchmark evolved layout vs. standard hashing

**Expected Benefits:**
- Datasets with spatial clustering should benefit most (like Crout: 10× speedup)
- Uniform random datasets might see minimal benefit (like Cholesky: 1-4%)
- Could improve `experiments/src/globimap_test_dataset_compare.cpp` results

## Key Differences / Integration Points

### Differences from Current Approach

**Paper's Focus:**
- **Explicit array storage**: Multi-dimensional arrays stored in memory with specific ordering
- **Compile-time layout**: Layout chosen before compilation, baked into indexing code
- **Dense arrays**: All elements present, power-of-2 dimensions required
- **Cache simulation for fitness**: Uses pycachesim to estimate performance
- **Linear algebra kernels**: Matrix multiplication, decomposition, stencils

**CountingGloBiMap Approach:**
- **Hash-based addressing**: No explicit storage array, hash function maps to filter positions
- **Runtime flexibility**: Can change hash functions without recompilation
- **Sparse data**: Bloom filter handles arbitrary sparse spatial distributions
- **Empirical benchmarking**: Measures actual performance on datasets
- **Spatial point queries**: Insert/query individual coordinates

### Integration Strategies

**1. Hybrid Hash + Layout Optimization**

Current hashing could be enhanced with Morton ordering:
```cpp
// Current two-stage hashing
uint64_t base_hash1 = murmur_hash(point);
uint64_t base_hash2 = murmur_hash(point + seed);
uint64_t pos[k] = {(base_hash1 + i * base_hash2) & mask};

// Enhanced with Morton pre-processing
uint64_t morton_encoded = evolved_morton_encode(point);
uint64_t base_hash1 = murmur_hash(morton_encoded);
// ... rest same
```

**Benefits:**
- Points close in spatial dimensions mapped to nearby bloom filter positions
- Better cache locality when inserting/querying spatial clusters
- Maintains all bloom filter properties (false positive guarantees, etc.)

**2. Layer Storage Optimization**

Multi-layer structure could use 2D layouts:
```cpp
// Dimension 0: spatial hash position (size = 2^logsize)
// Dimension 1: layer type (1-bit, 8-bit, 16-bit, 32-bit, 64-bit)

// Current: Separate vectors
Layer {
    std::vector<uint8_t> f8;
    std::vector<uint16_t> f16;
    // ...
};

// Optimized: Single morton-ordered array
Layer {
    std::vector<uint64_t> storage;  // All layers interleaved via Morton
    // Use bit manipulation to extract appropriate counter
};
```

**Trade-off:**
- Better cache locality when cascading between layers
- More complex bit manipulation to access counters
- Only beneficial if cascade operations are common

**3. Benchmark-Driven Layout Selection**

Extend `experiments/src/globimap_test_config.hpp` configuration system:
```cpp
struct FilterConfig {
    uint hash_k;
    std::vector<LayerConfig> layers;

    // NEW: Layout optimization
    std::string layout_pattern;  // e.g., "[0,1,0,1,2,2]" for 3D
    bool use_morton_ordering;
};
```

Generate configurations with different layouts:
```cpp
// Generate all permutations for small problems
auto configs = generate_all_morton_configs(2, 3, 3);  // 2D, 3 bits each

// Use genetic algorithm for large problems
auto evolved = evolve_layout_for_dataset("gdelt_multicategory.h5",
                                          generations=20,
                                          pop_size=20);
```

**4. Python Integration for Evolution**

The project already uses Python (uv) for dataset conversion and analysis. Could add:
```python
# datasets/utils/optimize_layout.py
import pycachesim
import numpy as np
from deap import base, creator, tools  # Genetic algorithm library

def evolve_layout_for_workload(hdf5_dataset, cache_config):
    """
    Run genetic algorithm to find optimal Morton layout
    for a specific dataset's access patterns.
    """
    # 1. Profile access patterns from C++ benchmark
    # 2. Simulate cache performance with different layouts
    # 3. Run genetic algorithm (20 generations, population=20)
    # 4. Return best layout as config string
    pass

# Usage
best_layout = evolve_layout_for_workload(
    "datasets/hdf5/gdelt_events_multicategory.h5",
    cache_config="haswell"  # or "zen3"
)
print(f"Evolved layout: {best_layout}")
# Output: "[0,1,0,2,1,0,2,1,2]" for 3D with (3,3,3) bits
```

Integrate with existing report generation:
```bash
# reports/generate_reports.sh
echo "Optimizing layouts for datasets..."
uv run datasets/utils/optimize_layout.py --dataset gdelt
uv run datasets/utils/optimize_layout.py --dataset covid

# Rebuild with optimized layouts
cmake -DEVOLVED_LAYOUT_GDELT="[0,1,0,2,1,0,2,1,2]" ..
make -j$(nproc)

# Re-run benchmarks
./run_all_experiments.sh
```

## Practical Takeaways

### Implementation Recommendations

1. **Start with profiling**: Before evolving layouts, measure cache miss rates for current implementation using `perf`:
   ```bash
   perf stat -e cache-misses,cache-references ./globimap_test_multicategory_dataset
   ```

2. **Focus on high-value kernels**: Based on paper's results:
   - **High priority**: Kernels with irregular multi-dimensional access (like Crout → 10× speedup)
   - **Medium priority**: Matrix-like operations (like MMijk → 3× speedup)
   - **Low priority**: Already cache-efficient kernels (like Cholesky → <5% improvement)

3. **Use BMI2 instructions for efficiency**: Morton encoding should use PDEP:
   ```cpp
   #include <immintrin.h>

   uint64_t morton_encode_2d(uint32_t x, uint32_t y) {
       // Custom mask based on evolved layout [i0, i1, ..., in-1]
       uint64_t mask_x = 0x5555555555555555ULL;  // Example for [0,1,0,1,...]
       uint64_t mask_y = 0xAAAAAAAAAAAAAAAAULL;  // Example for [0,1,0,1,...]
       return _pdep_u64(x, mask_x) | _pdep_u64(y, mask_y);
   }
   ```

4. **Constrain search space for SIMD compatibility**: If using SimdBloomFilter or PatternedSimdBloomFilter:
   - Ensure least significant 3 bits come from same dimension (for 8-wide AVX2)
   - This guarantees contiguous blocks of 8 elements for vectorization
   - Example constraint: Layout must start with [d, d, d, ...] for some dimension d

5. **Small-scale validation first**: Test on 256×256 or 512×512 arrays before scaling:
   - Faster iteration during development
   - Design space already large (2,704,156 layouts for 4096×4096)
   - Performance characteristics often transfer to larger scales

6. **Dataset-specific optimization**:
   - Run separate evolution for GDELT vs. COVID datasets
   - Access patterns differ significantly (GDELT: 4 categories, COVID: 1% sampling)
   - Store evolved layouts as dataset metadata in HDF5 files

7. **Maintain fallback to canonical layouts**: Not all kernels benefit equally:
   ```cpp
   enum class LayoutStrategy {
       ROW_MAJOR,
       COL_MAJOR,
       MORTON_STANDARD,
       MORTON_EVOLVED
   };

   // Runtime selection based on access pattern characteristics
   auto layout = (access_pattern.is_irregular() ?
                  MORTON_EVOLVED : ROW_MAJOR);
   ```

### Performance Expectations

**Based on paper's results, expect:**

**Optimistic Scenario (irregular spatial access):**
- 2-10× speedup for kernels with poor canonical layout performance
- Examples: Sparse spatial queries, multi-category range queries
- Conditions: Zen 3 architecture, large working set, spatial clustering

**Typical Scenario (mixed access patterns):**
- 30-100% speedup for multi-dimensional operations
- Examples: Multi-category bloom filter queries, rasterization
- Conditions: Haswell or Zen 3, moderate working set

**Pessimistic Scenario (already cache-friendly):**
- 0-10% speedup for simple sequential access
- Examples: Single-category insertion, linear scans
- Conditions: Small working set fitting in L1/L2 cache

**No Benefit Scenarios:**
- Purely compute-bound operations (hash calculation dominates)
- Very small datasets (entire structure fits in L1 cache)
- Random access patterns with no spatial locality

### Development Workflow

**Phase 1: Baseline Measurement**
```bash
# Collect baseline performance
./build/globimap_test_multicategory_dataset > baseline.txt
perf stat -e cache-misses ./build/globimap_test_multicategory_dataset

# Profile access patterns
perf record -e cache-misses -g ./build/globimap_test_multicategory_dataset
perf report
```

**Phase 2: Layout Evolution**
```bash
# Extract access traces for simulation
./build/globimap_test_multicategory_dataset --trace-only > access_trace.txt

# Run Python evolution script
uv run python scripts/evolve_morton_layout.py \
    --trace access_trace.txt \
    --cache-config zen3 \
    --generations 20 \
    --output evolved_layout.json
```

**Phase 3: Implementation & Testing**
```cpp
// implement evolved layout in C++
uint64_t evolved_morton_encode(uint32_t x, uint32_t y, uint32_t cat) {
    // Use layout from evolved_layout.json: e.g., [0,1,0,2,1,2,0,1,2]
    uint64_t mask_x = /* ... computed from layout ... */;
    uint64_t mask_y = /* ... */;
    uint64_t mask_cat = /* ... */;
    return _pdep_u64(x, mask_x) |
           _pdep_u64(y, mask_y) |
           _pdep_u64(cat, mask_cat);
}
```

**Phase 4: Validation**
```bash
# Rebuild with evolved layout
cmake -DUSE_EVOLVED_MORTON=ON ..
make -j$(nproc)

# Run same benchmark
./build/globimap_test_multicategory_dataset > evolved.txt

# Compare results
python scripts/compare_performance.py baseline.txt evolved.txt
```

**Phase 5: Integration**
```bash
# If successful, integrate into bloom filter configs
# Add to FilterConfig in globimap_test_config.hpp
# Update documentation in CLAUDE.md
# Regenerate reports
./reports/generate_reports.sh
```

### Limitations to Consider

1. **Power-of-2 requirement**: Generalized Morton requires array dimensions to be powers of 2
   - Over-allocation needed for odd sizes
   - O(2ⁿ) overhead for n-dimensional arrays
   - Bloom filter sizes already powers of 2, so not an issue for this project

2. **Hardware dependency**: BMI2 PDEP instruction required for efficiency
   - Intel: Haswell (2013+)
   - AMD: Zen 3 (2020+) for full hardware support
   - Graceful degradation: Fall back to lookup tables on older CPUs

3. **Evolution time**: Genetic algorithm with cache simulation takes time
   - 20 generations × 20 individuals × simulation time
   - For 2,704,156 layout space, GA explores ~0.015% of possibilities
   - May miss global optimum, but finds good local optimum

4. **Non-transferable layouts**: Evolved layouts are specific to:
   - Access pattern (GDELT ≠ COVID)
   - Cache hierarchy (Haswell ≠ Zen 3)
   - Array size (2048×2048 ≠ 4096×4096)
   - Need separate evolution for each combination

5. **Simulation accuracy**: Fitness function correlation not perfect (ρₛ = -0.4 to -0.95)
   - May not always predict actual performance
   - Need empirical validation on target hardware
   - Some kernels see no improvement despite high simulated fitness

## Research Applications

### Immediate Extensions to CountingGloBiMap

1. **Spatial Query Optimization**
   - Apply evolved layouts to `GloBiMap::rasterize()` function
   - Optimize rectangular region queries for GDELT event retrieval
   - Target: 50-200% speedup for spatial range queries

2. **Multi-Category Cache Locality**
   - Evolve 3D layouts for (x, y, category) space
   - Optimize for QuadClass GDELT queries (4 categories)
   - Measure improvement in `test_multicategory_dataset` benchmark

3. **Layer Cascade Optimization**
   - Profile cascade frequency in multi-layer CountingGloBiMap
   - If cascades are common, optimize 2D layout (position × layer_type)
   - Target: Reduce cascade overhead by improving layer access locality

4. **Hilbert vs. Morton Comparison**
   - Project has 3D Hilbert implementation (commit e9db315)
   - Compare: Standard Morton vs. Evolved Morton vs. Hilbert
   - Evaluate: Encoding speed vs. cache performance trade-off

### Broader Research Directions

1. **GPU Acceleration with Custom Layouts**
   - Extend evolution to GPU memory access patterns
   - Optimize for coalesced memory access in CUDA
   - Apply to `SimdBloomFilter` and `PatternedSimdBloomFilter`

2. **Multi-Objective Optimization**
   - Paper mentions NSGA-II for future work
   - Objectives: Cache performance + encoding speed + SIMD compatibility
   - Pareto frontier of layout trade-offs

3. **Machine Learning for Fitness Prediction**
   - Train ML model: layout + access pattern → performance
   - Replace cache simulation with fast ML inference
   - Enable real-time layout selection based on workload

4. **Adaptive Layout Switching**
   - Profile access patterns at runtime
   - Switch layouts dynamically based on detected pattern
   - Example: Row-major for sequential, Morton for spatial queries

5. **Integration with Space-Partitioning Structures**
   - Combine with kd-trees, octrees, or R-trees
   - Optimize both partitioning structure AND leaf array layout
   - Particularly relevant for spatial bloom filters

6. **Compression-Aware Layouts**
   - Some layouts may compress better (more runs of identical values)
   - Co-optimize for cache performance + compressed size
   - Relevant for storing large bloom filters on disk

### Publication Opportunities

**Potential Paper Topics Based on This Work:**

1. **"Cache-Optimized Probabilistic Spatial Data Structures via Evolved Morton Layouts"**
   - Apply paper's methodology to bloom filters
   - Show benefits for spatial/temporal datasets
   - Venue: ACM SIGSPATIAL or IEEE ICDE

2. **"Learned Index Layouts for Multi-Category Spatial Filters"**
   - Use ML to predict optimal layout from dataset characteristics
   - Benchmark on GDELT, COVID, and other spatial datasets
   - Venue: VLDB or ACM SIGMOD

3. **"Hardware-Accelerated Space-Filling Curves for Probabilistic Cardinality Estimation"**
   - Deep dive into BMI2/AVX-512 optimization
   - Compare Hilbert, Morton, and evolved curves
   - Venue: IEEE TPDS or ACM TACO

## Implementation Reference

### Immediate Action Items for CountingGloBiMap

**Priority 1: Profiling Infrastructure**
```bash
# Add perf integration to benchmarks
cd experiments/src
# Modify globimap_test_multicategory_dataset.cpp to output cache statistics
```

**Priority 2: Morton Encoding Library**
```cpp
// Create include/morton_layouts.hpp
namespace globimap {
    // Standard Morton (round-robin)
    uint64_t morton_encode_2d(uint32_t x, uint32_t y);
    uint64_t morton_encode_3d(uint32_t x, uint32_t y, uint32_t z);

    // Evolved layouts (from genetic algorithm)
    uint64_t evolved_morton_encode(uint32_t x, uint32_t y, uint32_t cat,
                                   const std::vector<uint8_t>& layout);
}
```

**Priority 3: Python Evolution Framework**
```python
# datasets/utils/evolve_layout.py
# Integrate pycachesim + DEAP for genetic algorithms
# Inputs: HDF5 dataset + cache config
# Outputs: Optimized layout configuration
```

**Priority 4: Benchmark Comparison**
```bash
# Create new experiment: experiments/src/globimap_test_layout_comparison.cpp
# Compare: row-major vs. Morton vs. evolved vs. Hilbert
# Metrics: insert time, query time, cache misses, memory bandwidth
```

**Priority 5: Documentation**
```markdown
# Update docs/ideas/sbbf_follow_ups.md
- Add section on cache-optimized layouts
- Reference this paper
- Describe evolution methodology
- Present benchmark results
```

### Code Examples for Integration

**Example 1: Enhanced Hash Function**
```cpp
// include/morton_hashfn.hpp
#pragma once
#include <immintrin.h>
#include "hashfn.hpp"

namespace globimap {

class MortonHashFunction : public HashFunction {
private:
    std::vector<uint8_t> layout_;  // Interleaving pattern
    uint64_t mask_x_, mask_y_, mask_cat_;

public:
    MortonHashFunction(const std::vector<uint8_t>& layout)
        : layout_(layout) {
        compute_masks();
    }

    void hash_point(const uint64_t* data, size_t len,
                   uint64_t* hash_out) override {
        // 1. Morton encode the coordinates
        uint64_t morton = 0;
        #ifdef __BMI2__
        morton = _pdep_u64(data[0], mask_x_) |
                _pdep_u64(data[1], mask_y_);
        if (len > 2) {
            morton |= _pdep_u64(data[2], mask_cat_);
        }
        #else
        // Fallback to LUT-based encoding
        morton = morton_encode_lut(data, len);
        #endif

        // 2. Apply MurmurHash to Morton-encoded value
        murmur::MurmurHash3_x64_128(&morton, sizeof(morton),
                                    seed_, (void*)hash_out);
    }

private:
    void compute_masks() {
        // Build PDEP masks from layout vector
        mask_x_ = mask_y_ = mask_cat_ = 0;
        for (size_t i = 0; i < layout_.size(); ++i) {
            if (layout_[i] == 0) mask_x_ |= (1ULL << i);
            else if (layout_[i] == 1) mask_y_ |= (1ULL << i);
            else if (layout_[i] == 2) mask_cat_ |= (1ULL << i);
        }
    }
};

} // namespace globimap
```

**Example 2: Configuration Extension**
```cpp
// experiments/src/globimap_test_config.hpp
struct FilterConfig {
    uint hash_k;
    std::vector<LayerConfig> layers;

    // NEW: Morton layout configuration
    enum LayoutType {
        STANDARD_HASH,      // Current implementation
        MORTON_STANDARD,    // Round-robin Morton
        MORTON_EVOLVED      // Evolved layout from GA
    };

    LayoutType layout_type = STANDARD_HASH;
    std::vector<uint8_t> morton_layout;  // e.g., [0,1,0,1,2,2]
};

// Helper to load evolved layout from JSON
FilterConfig load_config_with_layout(const std::string& json_path) {
    // Parse JSON, extract layout pattern
    // Return complete FilterConfig
}
```

**Example 3: Benchmark Integration**
```cpp
// experiments/src/globimap_test_layout_comparison.cpp
#include "counting_globimap.hpp"
#include "morton_hashfn.hpp"

int main() {
    // Load dataset
    auto dataset = load_hdf5("datasets/hdf5/gdelt_events_multicategory.h5");

    // Test configurations
    std::vector<FilterConfig> configs = {
        make_standard_config(),
        make_morton_standard_config(),
        load_evolved_config("configs/gdelt_evolved_haswell.json"),
        load_evolved_config("configs/gdelt_evolved_zen3.json")
    };

    // Benchmark each
    for (auto& cfg : configs) {
        auto filter = create_filter(cfg);

        auto insert_time = benchmark_inserts(filter, dataset);
        auto query_time = benchmark_queries(filter, dataset);
        auto cache_stats = measure_cache_performance(filter, dataset);

        print_results(cfg.name, insert_time, query_time, cache_stats);
    }
}
```

### Integration Timeline Estimate

**Week 1-2: Foundation**
- Set up cache profiling tools (perf integration)
- Implement basic Morton encoding library
- Validate BMI2 instruction usage

**Week 3-4: Evolution Framework**
- Port cache simulation setup from paper
- Implement genetic algorithm (use DEAP library)
- Create Python script to evolve layouts for GDELT/COVID

**Week 5-6: Integration**
- Add MortonHashFunction to project
- Extend FilterConfig to support layout specifications
- Update bloom filter constructors

**Week 7-8: Benchmarking**
- Run evolution for multiple datasets and cache hierarchies
- Implement comparison benchmarks
- Measure speedups on target hardware

**Week 9-10: Optimization & Documentation**
- Tune parameters (population size, mutation rate, generations)
- Profile and optimize hot paths
- Document findings in reports/

**Week 11-12: Validation & Publication**
- Validate results across different machines
- Generate publication-quality figures and tables
- Write technical report or paper draft

**Total Estimated Effort:** 3 person-months for full integration and evaluation

---

This paper provides a rigorous, well-validated methodology for optimizing cache performance through data layout evolution. Its application to CountingGloBiMap could yield significant performance improvements, particularly for spatially-clustered datasets like GDELT and COVID-19. The combination of space-filling curves with probabilistic data structures represents an exciting research direction with practical impact for large-scale spatial analytics.
