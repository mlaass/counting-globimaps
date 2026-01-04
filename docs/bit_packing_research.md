# CascadeCBF Bit-Packing Research

## Problem Statement

The current CascadeCBF implementation wastes significant memory due to using power-of-2 aligned storage types for non-power-of-2 bit widths:

| Layer Type | Bits Needed | Storage Type | Actual Bits | Waste |
|------------|-------------|--------------|-------------|-------|
| Layer12    | 12 (11+1)   | uint16_t     | 16          | 25%   |
| Layer20    | 20 (19+1)   | uint32_t     | 32          | 37.5% |
| Layer24    | 24 (23+1)   | uint32_t     | 32          | 25%   |

For a typical configuration with 2^20 Layer12 counters, this wastes 0.5 MB (2 MB used vs 1.5 MB optimal).

---

## Research Findings

### 1. Bit-Packing Performance (Daniel Lemire's Research)

**Source**: https://lemire.me/blog/2012/03/06/how-fast-is-bit-packing/

Key findings:
- **Unpacking can be faster than aligned access** because you load fewer words from memory
- **Packing is slower**, especially for non-power-of-2 widths (12, 17, 20...)
- **Sweet spots at 8 and 16 bits** - these pack/unpack significantly faster
- Odd bit widths (17-bit) incur ~2x overhead vs power-of-2 widths

**Implication**: For read-heavy workloads, bit-packing could actually improve performance. For write-heavy workloads with non-power-of-2 widths, expect slowdown.

### 2. SIMD Bit-Packing Libraries

**Sources**:
- simdcomp: https://github.com/lemire/simdcomp
- LittleIntPacker: https://github.com/lemire/LittleIntPacker
- Velox: https://github.com/facebookincubator/velox/discussions/2353

These libraries achieve near-zero overhead bit-packing by:
- Processing 128 integers at a time (SSE2) or 256 (AVX2)
- Using SIMD shuffle/permute instructions
- BMI2's `_pdep_u64` for bit scattering

**Velox benchmarks** (Intel CoffeeLake, 8M values):
| Output Type | Bit Width | Velox | Arrow | DuckDB |
|-------------|-----------|-------|-------|--------|
| uint8       | 1-8       | 0.3-0.5ms | 2.7-3.4ms | 10-19ms |
| uint32      | 1-16      | 1.6-2.6ms | 2.6-4.1ms | 10-77ms |

**Limitation for CascadeCBF**: These approaches work best for sequential batch processing of 128+ integers. CascadeCBF uses **random access** (k hash positions scattered across the array), so SIMD batch benefits don't apply directly.

### 3. Succinct Data Structures (Rank9)

**Source**: https://link.springer.com/chapter/10.1007/978-3-540-68552-4_12

Rank9 structure uses:
- Broadword programming (SWAR - "SIMD in a Register")
- Interleaved index storage to reduce cache misses
- 25% additional space for fast ranking

**Relevance**: These techniques could help if we need auxiliary structures for bit-packed arrays, but the 25% overhead negates memory savings.

### 4. Vertical Bit-Packing

**Source**: ResearchGate - Vertical Bit-Packing paper

Alternative to horizontal bit-packing where bits are stored column-wise rather than row-wise. Enables SIMD operations on packed data directly without unpacking.

**Relevance**: Could be useful if we need to perform aggregate operations on the filter, but adds significant complexity.

---

## Approach Analysis for CascadeCBF

### Approach 1: Byte-Aligned Only (8, 16, 32, 64-bit layers)

**Concept**: Only use storage types that perfectly match bit depths.

| Layer | Value Bits | Overflow | Storage | Max Value |
|-------|------------|----------|---------|-----------|
| Layer7  | 7        | 1        | uint8_t  | 127      |
| Layer15 | 15       | 1        | uint16_t | 32,767   |
| Layer31 | 31       | 1        | uint32_t | 2.1B     |
| Layer63 | 63       | 1        | uint64_t | 9.2E18   |

**Pros**:
- Zero implementation complexity
- Zero memory waste
- Maximum performance (aligned access)

**Cons**:
- Less flexibility in bit depth choices
- Layer7 overflows at 127 (more frequent cascades)

**Memory comparison** (2^20 counters, 2-layer config):
- Current CCBF_12_20: 2^20 * 2B + 2^14 * 4B = 2.1 MB
- Proposed CCBF_8_16: 2^20 * 1B + 2^14 * 2B = 1.03 MB (51% savings!)

### Approach 2: Scalar Bit-Packing

**Concept**: Store counters at exact bit widths, crossing word boundaries.

```cpp
// Example: 12-bit counter access
uint64_t get_12bit(const uint64_t* data, size_t index) {
    size_t bit_offset = index * 12;
    size_t word_idx = bit_offset / 64;
    size_t bit_idx = bit_offset % 64;

    uint64_t val = data[word_idx] >> bit_idx;
    if (bit_idx > 52) {  // Spans two words
        val |= data[word_idx + 1] << (64 - bit_idx);
    }
    return val & 0xFFF;  // 12-bit mask
}
```

**Pros**:
- Maximum memory efficiency (exact bit widths)
- 25-37% memory savings

**Cons**:
- 2-4x slower per access (bit manipulation + potential cache line crossing)
- Complexity in increment with overflow handling
- Poor SIMD applicability due to random access pattern

### Approach 3: Word-Aligned Packing

**Concept**: Pack multiple counters into 64-bit words without crossing boundaries.

Examples:
- 5 x 12-bit = 60 bits in 64-bit word (6% waste)
- 3 x 20-bit = 60 bits in 64-bit word (6% waste)
- 4 x 16-bit = 64 bits exactly (0% waste)

**Pros**:
- No cross-word access
- Moderate complexity
- Reasonable memory savings

**Cons**:
- Index calculation more complex (division/modulo by 5 or 3)
- Still slower than aligned access
- Wastes some bits per word

### Approach 4: Separate Overflow Bitmap

**Concept**: Store values and overflow flags separately.

```cpp
struct PackedLayer {
    std::vector<uint16_t> values;   // 11-bit values, packed
    std::vector<uint64_t> overflow_bits;  // 1 bit per counter
};
```

**Pros**:
- Overflow bitmap is very compact (1 bit per counter)
- Values can use exact widths

**Cons**:
- Two memory accesses per operation
- Cache efficiency reduced
- Complexity in synchronization

---

## Recommendation

**For maximum simplicity + significant savings**: Use **Approach 1 (Byte-Aligned Only)**

The key insight is that using Layer8 (7+1 bits in uint8_t) instead of Layer12 (11+1 bits in uint16_t) gives:
- 50% memory reduction at the first layer
- Zero implementation complexity
- Maximum performance

The trade-off is more frequent overflows to layer 2, but:
- Upper layers are typically much smaller (asymmetric sizing)
- Overflow cascade is already handled efficiently

**Proposed Configuration**:
```cpp
using CCBF_8_16 = CascadeCBF<Layer8, Layer16>;     // 7+15 = 22 value bits
using CCBF_8_16_32 = CascadeCBF<Layer8, Layer16, Layer32>;  // 7+15+31 = 53 value bits
```

**Alternative for maximum memory efficiency**: Implement **Approach 2 (Scalar Bit-Packing)** if the 2-4x performance hit is acceptable. This is best suited for memory-constrained environments where filters are built once and queried many times.

---

## Implementation Plan

### Phase 1: Add byte-aligned layer types
1. Verify Layer8 works correctly (already defined in cascade_cbf.hpp)
2. Add new type aliases: `CCBF_8_16`, `CCBF_8_16_32`
3. Update tests to cover new configurations
4. Benchmark memory savings and cascade frequency

### Phase 2 (Optional): Scalar bit-packing
1. Create `PackedLayer<BITS>` template with arbitrary bit width
2. Implement bit-spanning get/set operations
3. Handle increment with overflow across word boundaries
4. Extensive testing for edge cases (boundary positions)
5. Benchmark vs aligned implementation

### Files to Modify
- `include/cascade_cbf.hpp` - Add new layer types and configurations
- `tests/test_cascade_cbf.cpp` - Add tests for new configurations
- `docs/counting_globimap_v2_architecture.md` - Document new options

---

## References

1. Lemire, D. "How fast is bit packing?" https://lemire.me/blog/2012/03/06/how-fast-is-bit-packing/
2. Facebook Velox bit unpacking: https://github.com/facebookincubator/velox/discussions/2353
3. simdcomp library: https://github.com/lemire/simdcomp
4. Vigna, S. "Broadword Implementation of Rank/Select Queries" https://link.springer.com/chapter/10.1007/978-3-540-68552-4_12
5. countBF paper: https://ieeexplore.ieee.org/document/9615556/
