# Fast LUT-Based Hilbert 2D Encoding

## Overview

This document describes the LUT-based optimization for Hilbert curve encoding in `include/space_filling_curves.hpp`. The optimization achieves **~25x speedup** over the reference bit-by-bit implementation.

## Performance Results

| Implementation | Latency | Throughput | Speedup |
|----------------|---------|------------|---------|
| Hilbert2D LUT | 3.0 ns/op | 333 M ops/s | 25x |
| Hilbert2D Reference | 75 ns/op | 13 M ops/s | 1x |
| Morton2D (BMI2) | 0.7 ns/op | 1.5 B ops/s | baseline |

The LUT-based Hilbert encoder is only 4.4x slower than Morton2D, which uses hardware PDEP/PEXT instructions.

## Problem: Bit-by-Bit Algorithm

The standard Hilbert encoding algorithm processes one bit at a time:

```cpp
for (s = (1 << (Bits - 1)); s > 0; s >>= 1) {
    rx = (x & s) > 0 ? 1 : 0;
    ry = (y & s) > 0 ? 1 : 0;
    code += s * s * ((3 * rx) ^ ry);
    rotate(s, x, y, rx, ry);  // Conditional branches
}
```

For 16-bit coordinates, this requires 16 loop iterations with conditional branches in `rotate()`, leading to poor branch prediction and ~75ns latency.

## Solution: 4-Bit Chunked LUT

Process 4 bits at a time using a precomputed lookup table, reducing iterations from 16 to 4.

### State Machine Model

The 2D Hilbert curve has 4 orientations (states):

- **State 0**: Original H curve
- **State 1**: 90° clockwise rotation
- **State 2**: 180° rotation
- **State 3**: 270° clockwise rotation

Each state defines a different traversal order for the 4 quadrants:

```
State 0 (H):  (0,0) → (0,1) → (1,1) → (1,0)  child states: [1, 0, 0, 3]
State 1 (A):  (0,0) → (1,0) → (1,1) → (0,1)  child states: [0, 1, 1, 2]
State 2 (H'): (1,1) → (1,0) → (0,0) → (0,1)  child states: [3, 2, 2, 1]
State 3 (A'): (1,1) → (0,1) → (0,0) → (1,0)  child states: [2, 3, 3, 0]
```

### LUT Structure

- **Index**: `(state << 8) | (x4 << 4) | y4` = 1024 entries
- **Entry**: 16-bit value
  - Bits 0-7: 8-bit Hilbert code chunk for this 4×4 block
  - Bits 8-9: Next state for subsequent chunks

Total size: 2KB (fits in L1 cache)

### Algorithm

```cpp
encode(x, y):
    code = 0
    state = 0

    for each 4-bit chunk (MSB to LSB):
        x4 = (x >> shift) & 0xF
        y4 = (y >> shift) & 0xF

        idx = (state << 8) | (x4 << 4) | y4
        entry = lut[idx]

        code = (code << 8) | (entry & 0xFF)
        state = (entry >> 8) & 0x3

    return code
```

### Runtime LUT Generation

The LUT is generated at program startup via static initialization (not hardcoded):

```cpp
struct LUT {
    alignas(64) uint16_t data[1024];

    LUT() {
        // Generate by simulating bit-by-bit algorithm for all state/chunk combinations
        for (state : 0..3)
            for (x4 : 0..15)
                for (y4 : 0..15)
                    data[idx] = compute_entry(state, x4, y4);
    }
};

static const LUT& get_lut() {
    static const LUT lut;  // Initialized once on first use
    return lut;
}
```

This approach:
- Generates the LUT once at program startup
- Uses lazy initialization (generated on first `encode()` call)
- Keeps the source code clean without hardcoded tables
- Allows easy verification against reference implementation

## Bit Width Support

| Bits | Chunks | LUT Lookups | Notes |
|------|--------|-------------|-------|
| 4 | 1 | 1 | Minimum for LUT |
| 8 | 2 | 2 | 1.2 ns/op |
| 12 | 3 | 3 | 2.2 ns/op |
| 16 | 4 | 4 | 3.0 ns/op |
| 32 | 8 | 8 | Supported |

For `Bits < 4`, falls back to reference implementation.

## References

1. **fast-hilbert** (Rust): https://github.com/becheran/fast-hilbert
   - 512-byte LUT achieves 4ns/encode
   - Inspiration for chunked LUT approach

2. **hilbert_gen**: https://github.com/wzli/hilbert_gen
   - Portable C implementation with bit-wise operations

3. **qHilbert**: https://github.com/Wunkolo/qHilbert
   - SIMD parallel-prefix for index→(x,y) decoding
   - Note: Solves inverse problem (not used here)

4. **Alves et al. (2022)**: "Cache-oblivious Hilbert Curve-based Blocking Scheme"
   - ACM Transactions on Mathematical Software
   - SIMD Hilbert curve generator using XOR axiom swapping
   - Achieves 3.68×10⁹ points/second

## Files

- `include/space_filling_curves.hpp`: Implementation (Hilbert2D struct)
- `tests/test_hilbert_lut.cpp`: Correctness verification tests
- `experiments/src/hilbert_benchmark.cpp`: Performance benchmark

## Usage

```cpp
#include "space_filling_curves.hpp"
using namespace sfc;

// Encode 16-bit coordinates to Hilbert index
uint64_t code = Hilbert2D<16>::encode(x, y);

// Reference implementation (for verification)
uint64_t ref = Hilbert2D<16>::encode_reference(x, y);

// Decode back to coordinates
uint32_t dx, dy;
Hilbert2D<16>::decode(code, dx, dy);
```
