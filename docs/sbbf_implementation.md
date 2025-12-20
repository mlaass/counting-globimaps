# SBBF Documentation: Components, Tests, and Benchmarks

## Overview

The Spatial-Blocked Bloom Filter (SBBF) replaces traditional hash-based block indexing with Space-Filling Curves (SFC) to preserve spatial locality. This enables cache-efficient neighborhood queries for 2D/3D spatial data.

**Core Insight:** Low SFC bits → block index (sequential access), High SFC bits → intra-block signature

---

## 1. Space-Filling Curve Implementations

**File:** `include/space_filling_curves.hpp` (1130 lines)

### Morton Curves (Z-order)

| Curve | Performance | Method | Max Bits |
|-------|-------------|--------|----------|
| Morton2D | ~2 ns | BMI2 PDEP/PEXT or bit-magic | 32 per coord |
| Morton3D | ~2 ns | BMI2 PDEP/PEXT or bit-magic | 21 per coord |

- Bit-interleaving: x at 0,2,4... / y at 1,3,5... (2D)
- Good locality but has "jump" discontinuities at quadrant boundaries

### Hilbert Curves (Better locality)

| Curve | Performance | Method | Speedup vs Reference |
|-------|-------------|--------|---------------------|
| Hilbert2D LUT | ~3 ns | 2KB LUT, 4-bit chunks | 24x |
| Hilbert3D LUT | ~6 ns | 96-byte Morton→Hilbert transform | 14x |
| Hilbert2D ref | ~75 ns | Bit-by-bit with rotation | baseline |
| Hilbert3D ref | ~89 ns | Bit-by-bit with rotation | baseline |

**Hilbert2D LUT:** 4 states × 256 entries = 1024 × 2 bytes = 2KB
**Hilbert3D LUT:** 12 states × 8 octants = 96 bytes (Morton→Hilbert transform)

### Batch Neighborhood Encoding

Optimized for querying 3×3 (2D) or 3×3×3 (3D) neighborhoods:

| Function | Speedup | Fast Path Coverage |
|----------|---------|-------------------|
| `encode_neighborhood_2d()` | 4x | 77% (same 16×16 block) |
| `encode_neighborhood_3d()` | 2x | 12.5% (same 4×4×4 block) |

---

## 2. Intra-Block Hashing Strategies

**The key design question: How do we set k bits within a 64-bit block?**

```cpp
enum class IntraBlockStrategy {
    DOUBLE_HASH,     // Default - lightweight double hashing
    PATTERN_LOOKUP,  // Pre-computed k-bit patterns from LUT
    MULTIPLEXED      // OR multiple patterns together
};
```

### Strategy Details

| Strategy | Memory | Method | Use Case |
|----------|--------|--------|----------|
| **DOUBLE_HASH** | 0 | `bit[i] = (h1 + i*h2) % 64` | Default, good balance |
| **PATTERN_LOOKUP** | 2-8 KB | `pattern_table[seed % size]` | Better distribution |
| **MULTIPLEXED** | 0 | OR x patterns of k/x bits | Tunable tradeoff |

### DOUBLE_HASH (Default)

```cpp
// From high bits of SFC code:
h1 = seed % 64           // Primary position
h2 = (seed / 64) % 64    // Step size (forced != 0)

mask = 0
for i in 1..k:
    mask |= (1 << ((h1 + i*h2) % 64))
```

### PATTERN_LOOKUP

- Pre-compute `pattern_table_size` (default 1024) patterns at init
- Each pattern has exactly `hash_k` bits set
- Seed indexes: `pattern_table[seed % table_size]`
- Better bit distribution than double hashing

### MULTIPLEXED

- Divide k bits across `multiplex_count` independent patterns
- Each uses double hashing with different seed bits
- Balances computation and distribution

---

## 3. SBBF Configuration

```cpp
struct SBBFConfig {
    SFCType sfc_type = MORTON_2D;           // MORTON_2D/3D, HILBERT_2D/3D
    unsigned sfc_bits = 16;                  // Bits per coordinate
    unsigned log_num_blocks = 10;            // 2^10 = 1024 blocks
    unsigned bits_per_block = 64;            // 64, 256, or 512
    unsigned hash_k = 4;                     // Bits set per element
    IntraBlockStrategy intra_strategy = DOUBLE_HASH;
    size_t pattern_table_size = 1024;        // For PATTERN_LOOKUP
    unsigned multiplex_count = 2;            // For MULTIPLEXED
};
```

**Memory:** `2^log_num_blocks * bits_per_block / 8` bytes

---

## 4. Tests

### SBBF Unit Tests (`tests/test_sbbf.cpp`) - 25 tests

| Category | Tests | Coverage |
|----------|-------|----------|
| Configuration | 4 | Validation, memory calculation |
| Basic ops | 5 | put/get for Morton/Hilbert 2D/3D |
| Correctness | 2 | No false negatives (1000+ points) |
| FPR | 1 | FPR < 20% for test config |
| Strategies | 3 | DOUBLE_HASH, PATTERN_LOOKUP, MULTIPLEXED |
| Neighborhood | 4 | 2D/3D neighborhood queries, early-exit |
| Utility | 5 | Memory, summary, clear, block fill |

### Hilbert LUT Tests (`tests/test_hilbert_lut.cpp`) - 22 tests

| Category | Tests | Coverage |
|----------|-------|----------|
| 2D LUT correctness | 5 | Reference match, exhaustive 8-bit |
| 2D roundtrip | 2 | Encode/decode, ordering preserved |
| 2D neighborhood | 5 | Same-block, boundary, random, performance |
| 2D performance | 1 | LUT vs reference (24x speedup) |
| 3D LUT correctness | 3 | Bijection, roundtrip, locality |
| 3D neighborhood | 3 | Correctness, exhaustive, performance |
| 3D performance | 1 | LUT vs reference (14x speedup) |

### SFC Tests (`tests/test_space_filling_curves.cpp`)

- Morton2D/3D encode/decode roundtrip
- Hilbert2D/3D encode/decode roundtrip
- Edge cases (zero, max coordinates)
- Locality verification

---

## 5. Benchmarks

### Main Benchmark (`experiments/src/sbbf_benchmark.cpp`)

**Scenarios:**
- 2D Uniform (100K points)
- 2D Clustered (100K points, 100 clusters)
- 3D Uniform (100K points)

**Metrics via nanobench + hardware counters:**
- Insert/Query/Neighbor latency (ns)
- Instructions per operation
- CPU cycles per operation
- False positive rate
- Memory usage

**Run:**
```bash
./sbbf_benchmark --scenario 3d --epochs 5
```

### Recent Results (1MB memory budget, 100K elements)

| Implementation | Query | Neighbor (per) | FPR |
|----------------|-------|----------------|-----|
| SBBF-Morton3D | 5.6 ns | 3.3 ns | 0.07% |
| SBBF-Hilbert3D | 14.5 ns | 5.5 ns | 0.06% |
| BlockedBF | 23.3 ns | 22.2 ns | 1.44% |
| RegisterBF | 20.6 ns | 18.1 ns | 1.82% |

**SBBF advantage:** 4x faster neighbor queries, 20x better FPR

---

## 6. Existing Documentation

- `docs/hilbert_lut_optimization.md` - LUT implementation details
- `docs/ideas/sbbf_proposal.md` - Original design proposal with diagrams
- `docs/ideas/sbbf_corrections.md` - Design corrections

---

## 7. Intra-Block Strategy Benchmark Results

Compare DOUBLE_HASH vs PATTERN_LOOKUP vs MULTIPLEXED for query latency,
instruction count, and false positive rate.

**Run:** `./sbbf_benchmark --scenario strategy`

**Test configuration:**
- Memory: 1MB (log_num_blocks=17, bits_per_block=64)
- SFC: Hilbert3D (10-bit coordinates)
- Elements: 100K random 3D points
- Query set: 100K disjoint random points

### Results (1MB, 100K elements, Hilbert3D)

| Strategy | k | Param | Query | ins | cyc | Neighbor | ins | cyc | FPR |
|----------|---|-------|-------|-----|-----|----------|-----|-----|-----|
| DOUBLE_HASH | 2 | - | 14.7 ns | 163 | 53 | 144 ns | 2507 | 515 | 0.085% |
| **DOUBLE_HASH** | **4** | **-** | **15.7 ns** | **175** | **56** | **150 ns** | **2818** | **540** | **0.058%** |
| DOUBLE_HASH | 6 | - | 16.7 ns | 187 | 60 | 160 ns | 3128 | 576 | 0.063% |
| DOUBLE_HASH | 8 | - | 16.4 ns | 199 | 59 | 171 ns | 3440 | 616 | 0.143% |
| PATTERN_LOOKUP | 4 | 256 | 14.7 ns | 148 | 52 | 137 ns | 2118 | 492 | 0.323% |
| PATTERN_LOOKUP | 4 | 512 | 16.7 ns | 148 | 58 | 145 ns | 2118 | 520 | 0.163% |
| PATTERN_LOOKUP | 4 | 1024 | 15.4 ns | 148 | 55 | 143 ns | 2118 | 516 | 0.083% |
| **PATTERN_LOOKUP** | **4** | **2048** | **17.6 ns** | **148** | **63** | **140 ns** | **2119** | **504** | **0.048%** |
| MULTIPLEXED | 4 | x1 | 18.1 ns | 193 | 65 | 226 ns | 3260 | 813 | 0.325% |
| MULTIPLEXED | 4 | x2 | 17.8 ns | 207 | 63 | 177 ns | 3622 | 637 | 0.087% |
| MULTIPLEXED | 4 | x4 | 18.1 ns | 235 | 65 | 199 ns | 4348 | 717 | 0.393% |

### Key Findings

1. **PATTERN_LOOKUP uses 15-25% fewer instructions**
   - 148 ins/query vs 175 for DOUBLE_HASH k=4
   - 2118 ins/neighbor vs 2818 (25% reduction)
   - Single table lookup instead of double-hashing loop

2. **Table size affects FPR significantly**
   - 256 patterns: 0.323% FPR (too few, collisions)
   - 2048 patterns: 0.048% FPR (7x better)

3. **DOUBLE_HASH k=4 is best without extra memory**
   - 0.058% FPR, good instruction efficiency
   - Higher k values have worse FPR (double-hashing collisions)

4. **MULTIPLEXED has no advantage**
   - More instructions, worse FPR than alternatives
   - Not recommended

### Recommendations

| Use Case | Strategy | Config |
|----------|----------|--------|
| **Best FPR** | PATTERN_LOOKUP | table_size=2048 |
| **No memory overhead** | DOUBLE_HASH | k=4 |
| **Lowest latency** | PATTERN_LOOKUP | table_size=1024 |
