# CountingGloBiMap Redesign Plan

## Problem Summary

The current CountingGloBiMap (`include/counting_globimap.hpp`) has fundamental implementation issues:

1. **Memory Waste** (lines 72-81): Layer struct declares all 5 vectors (f1, f8, f16, f32, f64) but only uses one. ~85% wasted memory.
2. **Runtime Type Dispatch** (lines 105-148): Switch on `bits` field for every get/set.
3. **Double Hash Computation**: `minimal_increment` mode computes hash positions twice.
4. **Poor Cache Locality**: 5 separate vectors scattered in memory.
5. **Wasted Saturated Counters**: When layer 0 saturates at 255, those bits are wasted - just sitting at max while layer 1 tracks overflow separately.
6. **Uniform Layer Sizes**: All layers same size, but biased data means upper layers could be much smaller.

**Benchmark reality**: CountingGloBiMap is 2-3x slower than Spectral BF (MI) and Count-Min Sketch while achieving similar accuracy.

## Solution: Template-Specialized Multi-Layer Design

Create `CountingGloBiMapV2` with compile-time layer types, eliminating runtime dispatch and memory waste.

### Key Innovation 1: Overflow Bit + Continuation Bits

**Current (wasteful)**: When 8-bit counter saturates at 255, it stays at 255 forever while layer 1 tracks overflow separately.

**New approach**: Reserve high bit as overflow flag, remaining bits continue counting:

```
Counter layout:
  [overflow_flag (1 bit)] [value_bits (N-1 bits)]

Recommended default: 12-bit + 20-bit layers (not 8-bit)
  Layer 0: 12 bits → 11 value bits → max 2047 before cascade
  Layer 1: 20 bits → 19 value bits → max 524,287 before cascade

Example: Count = 5000 with 12+20 bit config
  Layer 0: overflow=1, low_bits = 5000 & 0x7FF = 904
  Layer 1: 5000 >> 11 = 2

Reconstruction: (layer1 << 11) | (layer0 & 0x7FF) = 4096 + 904 = 5000
```

**Benefits**:
- 12-bit base handles 99%+ of spatial cells without cascading
- 20-bit second layer handles extreme hotspots (up to 500K+ counts)
- Full precision maintained across layers
- Much less cascade overhead than 8-bit base

### Key Innovation 1b: Flexible Counter Storage

Support both **bit-packed** (memory-optimal) and **byte-aligned** (speed-optimal) storage:

```cpp
// Option A: Byte-aligned (simpler, faster)
// Uses 16-bit storage for 12-bit counters, 32-bit for 20-bit
template<typename StorageType, uint ValueBits>
struct ByteAlignedLayer {
    std::vector<StorageType> counters;  // uint16_t or uint32_t
    static constexpr StorageType overflow_bit = StorageType(1) << ValueBits;
    static constexpr StorageType value_mask = overflow_bit - 1;
};

// Option B: Bit-packed (memory-optimal)
// True 12-bit or 20-bit storage with bit manipulation
template<uint TotalBits>
struct BitPackedLayer {
    std::vector<uint64_t> packed;  // 64-bit words for packing
    // Access requires bit-shift/mask operations
};

// Configuration selects which to use
struct LayerConfig {
    uint value_bits;      // 11, 19, etc.
    bool bit_packed;      // true = memory-optimal, false = speed-optimal
};
```

**Trade-offs**:
| Mode | Memory | Speed | Complexity |
|------|--------|-------|------------|
| Byte-aligned 12+20 | 2 + 4 = 6 bytes/position | Fast | Low |
| Bit-packed 12+20 | 1.5 + 2.5 = 4 bytes/position | ~20% slower | High |

**SIMD opportunity**: Check overflow bits of 16 counters (16-bit each) in one AVX2 instruction:
```cpp
// Check if any of 16 uint16_t counters have overflow bit set (bit 11 for 12-bit counters)
__m256i counters = _mm256_loadu_si256(data);
__m256i overflow_mask = _mm256_set1_epi16(0x0800);  // Bit 11
__m256i overflows = _mm256_and_si256(counters, overflow_mask);
int has_overflow = !_mm256_testz_si256(overflows, overflows);
```

### Key Innovation 2: Asymmetric Layer Sizes for Biased Data

Spatial data follows power-law distribution (many cold spots, few hot spots). Upper layers can be **much smaller**:

```
Traditional V1 (uniform 8-bit layers):
  Layer 0 (8-bit):  2^20 = 1M   (1 MB)
  Layer 1 (16-bit): 2^20 = 1M   (2 MB)
  Layer 2 (32-bit): 2^20 = 1M   (4 MB)
  Total: 7 MB

New V2 with 12+20 bit, asymmetric (byte-aligned):
  Layer 0 (12-bit in 16-bit): 2^20 = 1M   (2 MB)    - 99%+ stays here (max 2047)
  Layer 1 (20-bit in 32-bit): 2^14 = 16K  (64 KB)   - hot spots only
  Total: ~2.06 MB

New V2 with 12+20 bit, asymmetric (bit-packed):
  Layer 0 (12-bit packed):    2^20 = 1M   (1.5 MB)
  Layer 1 (20-bit packed):    2^14 = 16K  (40 KB)
  Total: ~1.54 MB
```

**Why this works**: With biased data, hash collisions in smaller upper layers are acceptable because:
1. Few items overflow to upper layers
2. Items that do overflow are likely already "hot" (high-frequency)
3. Collision in upper layer adds to already-high count (bounded error)

### Core Design

```cpp
// Byte-aligned layer with configurable value bits
// StorageType: uint16_t, uint32_t, uint64_t
// ValueBits: actual bits for value (e.g., 11 for 12-bit counter)
template<typename StorageType, uint ValueBits>
struct TypedLayer {
    std::vector<StorageType> counters;
    uint64_t mask;
    uint logsize;

    static constexpr uint value_bits = ValueBits;
    static constexpr StorageType overflow_bit = StorageType(1) << ValueBits;
    static constexpr StorageType value_mask = overflow_bit - 1;
    static constexpr StorageType max_value = value_mask;

    StorageType get_value(uint64_t pos) const { return counters[pos] & value_mask; }
    bool has_overflow(uint64_t pos) const { return counters[pos] & overflow_bit; }

    // Returns true if cascade needed
    bool increment(uint64_t pos) {
        StorageType val = counters[pos] & value_mask;
        if (val < max_value) {
            counters[pos] = (counters[pos] & overflow_bit) | (val + 1);
            return false;
        }
        counters[pos] = overflow_bit;  // Wrap to 0, set overflow
        return true;
    }

    size_t byte_size() const { return counters.size() * sizeof(StorageType); }
};

// Common layer types
using Layer12 = TypedLayer<uint16_t, 11>;  // 12-bit: 11 value + 1 overflow
using Layer20 = TypedLayer<uint32_t, 19>;  // 20-bit: 19 value + 1 overflow
using Layer32 = TypedLayer<uint32_t, 31>;  // 32-bit: 31 value + 1 overflow

// Variadic template for type-safe layer stack
template<typename... Layers>
class CountingGloBiMapV2 {
    std::tuple<Layers...> layers_;
    uint hash_k_;
    bool minimal_increment_;

public:
    // Configuration: array of logsizes, one per layer
    void configure(uint hash_k, std::array<uint, sizeof...(Layers)> logsizes, bool mi = false);
};

// Default configuration: 12+20 bit, asymmetric
using DefaultCGM = CountingGloBiMapV2<Layer12, Layer20>;
```

### Key Improvements

| Issue | Current | V2 |
|-------|---------|-----|
| Memory | 5 vectors/layer | 1 vector/layer |
| Dispatch | Runtime switch | Compile-time |
| MI mode hashing | 2x | 1x (cached) |
| Cache | Poor | Good (contiguous) |
| Saturated bits | Wasted at max | Continue as low bits |
| Layer sizes | Uniform | Asymmetric (smaller upper) |
| Default counter | 8-bit (max 255) | 12-bit (max 2047) |
| Cascade frequency | Every 255 | Every 2047 (12-bit) |
| Bit-width | Fixed 8/16/32/64 | Configurable (12, 20, etc.) |
| Storage mode | Byte-aligned only | Byte-aligned OR bit-packed |

## Implementation Steps

### Step 1: Create TypedLayer and V2 Core (Byte-Aligned)

**File**: `include/counting_globimap_v2.hpp` (NEW)

- `TypedLayer<StorageType, ValueBits>` template with overflow bit protocol
- Pre-defined types: `Layer12`, `Layer20`, `Layer32`
- `CountingGloBiMapV2<Layers...>` variadic template
- put() with hash position caching for MI mode
- get_min() with bit-shift reconstruction across layers

**get_min() with overflow bit reconstruction**:
```cpp
uint64_t get_min(const std::vector<uint64_t>& point) {
    uint64_t h1, h2;
    hash(point.data(), point.size(), &h1, &h2);

    uint64_t min_count = UINT64_MAX;
    for (uint i = 0; i < hash_k_; ++i) {
        uint64_t total = 0;
        uint shift = 0;

        // Traverse layers, accumulating shifted values
        // Layer 0: 7 bits at position 0
        // Layer 1: 15 bits at position 7
        // Layer 2: 31 bits at position 22
        std::apply([&](auto&... layers) {
            ((void)[&] {
                auto& l = layers;
                uint64_t pos = (h1 + (i + 1) * h2) & l.mask;
                uint64_t val = l.get_value(pos);  // Masked value bits
                total += val << shift;
                shift += l.value_bits;  // 7, 15, 31, ...
                return l.has_overflow(pos);  // Continue if overflow set
            }() && ...);
        }, layers_);

        if (total == 0) return 0;
        min_count = std::min(min_count, total);
    }
    return min_count;
}
```

### Step 2: Pre-defined Configurations

**File**: `include/counting_globimap_v2.hpp` (continued)

```cpp
// Common layer configurations (byte-aligned)
using Layer12 = TypedLayer<uint16_t, 11>;  // 12-bit in 16-bit storage
using Layer20 = TypedLayer<uint32_t, 19>;  // 20-bit in 32-bit storage
using Layer32 = TypedLayer<uint32_t, 31>;  // 32-bit (full 32-bit storage)

// Recommended default: 12+20 bit, 2 layers
using DefaultCGM = CountingGloBiMapV2<Layer12, Layer20>;

// Alternative configurations
using CGM_12 = CountingGloBiMapV2<Layer12>;              // Single layer, small counts
using CGM_12_20_32 = CountingGloBiMapV2<Layer12, Layer20, Layer32>;  // 3 layers, huge counts
```

### Step 3: Bit-Packed Layer (Optional, Memory-Optimal)

**File**: `include/counting_globimap_v2.hpp` (continued)

```cpp
// Bit-packed layer for memory-optimal storage
template<uint TotalBits>
struct BitPackedLayer {
    std::vector<uint64_t> packed;  // 64-bit words
    uint64_t mask;

    static constexpr uint value_bits = TotalBits - 1;
    static constexpr uint elements_per_word = 64 / TotalBits;

    // Bit manipulation for access
    uint64_t get_value(uint64_t pos) const;
    bool has_overflow(uint64_t pos) const;
    bool increment(uint64_t pos);
};

using PackedLayer12 = BitPackedLayer<12>;  // 5 counters per 64-bit word
using PackedLayer20 = BitPackedLayer<20>;  // 3 counters per 64-bit word
```

### Step 4: Unit Tests

**File**: `tests/test_counting_globimap_v2.cpp` (NEW)

- Correctness: Verify overflow bit + continuation logic
- Reconstruction: Test bit-shift reconstruction across layers
- Memory: Verify byte-aligned and bit-packed sizes
- Comparison: Compare V2 vs V1 on identical inputs
- Performance: Benchmark insert/query throughput

### Step 5: Benchmark Integration

**File**: `experiments/src/globimap_test_dataset_compare.cpp` (MODIFY)

- Add CountingGloBiMapV2 (byte-aligned and bit-packed) to comparison suite
- Run on GDELT and COVID-19 datasets
- Compare against Spectral BF (MI), CMS, d-Left CBF

### Step 6: Documentation

**File**: `CLAUDE.md` (MODIFY)

- Document V2 API and usage
- Explain overflow bit + continuation design
- Migration guide from V1

## Critical Files

| File | Action |
|------|--------|
| `include/counting_globimap.hpp` | Reference (keep V1 for compatibility) |
| `include/counting_globimap_v2.hpp` | **CREATE** - New implementation |
| `include/spectral_bloom_filter.hpp` | Reference for MI algorithm |
| `include/common/bf_interface.hpp` | MODIFY - Add V2 interface |
| `tests/test_counting_globimap_v2.cpp` | **CREATE** - Unit tests |
| `experiments/src/globimap_test_dataset_compare.cpp` | MODIFY - Add benchmark |

## Expected Outcomes

- **Insert throughput**: 2-3x faster (eliminate switch, cache hash positions, less cascading)
- **Query latency**: 15-20% faster (eliminate switch, simpler reconstruction)
- **Cascade reduction**: 8x less cascading (2047 vs 255 threshold with 12-bit base)
- **Memory (byte-aligned)**: 2.06 MB for 1M positions (12+20 bit, asymmetric)
- **Memory (bit-packed)**: 1.54 MB for 1M positions (33% savings)
- **Accuracy**: Better than V1 (full precision via continuation bits)

## Success Criteria

1. V2 insert throughput within 20% of Spectral BF (MI) on GDELT dataset
2. V2 query latency within 20% of Spectral BF (MI)
3. V2 accuracy matches or exceeds V1
4. V2 memory usage documented and predictable
