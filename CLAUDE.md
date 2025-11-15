# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

CountingGloBiMap is a multi-layer counting bloom filter implementation for cardinality estimation of sparse spatial data. This is a header-only C++ library extracted from the GloBiMap research codebase, featuring hierarchical layers with cascading overflow and OpenMP parallelization.

## Build Commands

### Initial Setup
```bash
# Initialize submodules (required on first clone)
git submodule update --init --recursive

# Create build directory
mkdir -p build && cd build

# Configure with CMake
cmake ..

# Build all executables
make -j$(nproc)
```

### Build Types
- **Release build** (default): `-DCMAKE_BUILD_TYPE=Release` - Uses `-Ofast` optimization
- **Debug build**: `-DCMAKE_BUILD_TYPE=Debug` - Uses `-g` flag

### Building Single Executables
```bash
# From build/ directory
make test_datasets
make test_datasets_new
make test_datasets_for_k
make test_polygon
make globimap_rasterize_polys
```

### Running Tests
```bash
# From build/ directory
./test_datasets
./test_datasets_new
./test_datasets_for_k
```

Note: Most experiment executables expect HDF5 datasets at hardcoded paths. Check the source files in `experiments/src/` to see expected dataset locations.

## Code Architecture

### Core Data Structure: CountingGloBiMap

The main implementation is header-only in `include/counting_globimap.hpp`. Key concepts:

**Multi-Layer Design**: Each layer has a different bit depth (1, 8, 16, 32, or 64 bits). When a counter reaches its maximum value, increments cascade to the next layer with higher bit depth. This provides compact storage for counts of varying magnitudes.

**Hashing Trick**: Instead of k independent hash functions, the implementation computes 2 base hashes (h1, h2) using MurmurHash, then generates k hash positions via:
```
hash[i] = (h1 + (i+1) * h2) & mask
```
This reduces computation and improves cache efficiency while maintaining good distribution.

**Layer Structure**: The template `Layer<BITS1, BITS8, BITS16, BITS32, BITS64>` supports all bit depths in a single type. Each layer maintains:
- `bits`: Active bit depth for this layer
- `size`: Number of counters (always 2^logsize)
- `mask`: Hash mask for this layer
- `f1`, `f8`, `f16`, `f32`, `f64`: Storage vectors for each bit depth

**Min-Count Estimator**: The `get_min()` method returns the minimum count across all k hash functions for a point, providing cardinality estimation with bounded error.

### Key Files

- `include/counting_globimap.hpp` - Main CountingGloBiMap implementation (header-only)
- `include/murmur.hpp` - MurmurHash3 implementation for hashing
- `include/hashfn.hpp` - Hash function interface
- `experiments/src/globimap_test_config.hpp` - Configuration generation utilities
- `experiments/src/*.cpp` - Experiment executables (dataset tests, polygon processing, benchmarks)

### Configuration System

Filters are configured via:
```cpp
struct LayerConfig {
    uint bits;      // 1, 8, 16, 32, or 64
    uint logsize;   // Layer size is 2^logsize
};

struct FilterConfig {
    uint hash_k;                           // Number of hash functions
    std::vector<LayerConfig> layers;       // Layer stack
};
```

Layers are ordered from finest (typically 1-bit or 8-bit) to coarsest (typically 32-bit or 64-bit).

## Alternative Counting Bloom Filter Implementations

The project includes 5 counting bloom filter implementations, each with different trade-offs. All are header-only and located in `include/`:

### Implementation Overview

1. **CountingGloBiMap** (`counting_globimap.hpp`) - Multi-layer hierarchical filter
   - Original implementation with cascading layers
   - NEW: Optional `minimal_increment` for conservative updates (reduces overcounting)
   - NEW: Optional `cascade_factor` for early cascade (prevents saturation)
   - Best for: Highly skewed data with varying count magnitudes

2. **Variable-Increment CBF** (`variable_increment_bf.hpp`) - Simple single-layer filter
   - Based on Rottenstreich et al. (2014)
   - Uses variable increments [L, 2L-1] for 50% memory savings
   - Simple 4-parameter configuration
   - **⚠️ WARNING**: Provides ~4-5x overcounting for frequency queries (310% error)
   - Best for: **Membership testing only** (`get_bool()`), NOT frequency estimation

3. **Spectral Bloom Filter** (`spectral_bloom_filter.hpp`) - Multiple variants
   - Based on Cohen & Matias (2003)
   - Three variants: MS (Minimum Selection), MI (Minimal Increment), RM (Recurring Minimum)
   - MI variant provides conservative updates for better accuracy
   - RM variant supports deletions
   - Best for: High-frequency item detection with accuracy

4. **d-Left Counting Bloom Filter** (`dleft_counting_bf.hpp`) - Deterministic lookups
   - Based on Bonomi et al. (2006)
   - Uses d-left hashing with fingerprints for cache-friendly design
   - Supports deletions
   - Best for: Cache-sensitive applications, deterministic query time

5. **Count-Min Sketch** (`count_min_sketch.hpp`) - Probabilistic guarantees
   - Based on Cormode & Muthukrishnan (2005)
   - Provides error bounds (epsilon, delta parameters)
   - Optional conservative update
   - Best for: When you need provable error guarantees, minimal memory

### Quick Selection Guide

Choose based on your requirements:

```
Need error bounds?          → Count-Min Sketch
Need deletions?             → d-Left CBF or Spectral BF (RM variant)
Need minimal memory?        → Count-Min Sketch (2.66 KB typical)
Need best accuracy?         → Spectral BF (MI) or GloBiMap (MI)
Need cache efficiency?      → d-Left CBF
Need membership only?       → Variable-Increment CBF (use get_bool(), NOT get_min())
Need frequency estimation?  → Spectral BF (MI), Count-Min Sketch, or GloBiMap (MI)
Need varying magnitudes?    → CountingGloBiMap (multi-layer)
```

**⚠️ WARNING**: Do NOT use Variable-Increment CBF for frequency estimation - it provides ~2-5x overcounting due to variable increments [L, 2L-1]. Use `get_bool()` for membership only.

### Building and Running Unit Tests

```bash
# From build/ directory

# Build individual test suites
make test_variable_increment_bf
make test_spectral_bloom_filter
make test_dleft_counting_bf
make test_count_min_sketch
make test_enhanced_globimap

# Run tests
./test_variable_increment_bf      # 15 tests - VI-CBF correctness
./test_spectral_bloom_filter      # 17 tests - All SBF variants
./test_dleft_counting_bf          # 14 tests - d-Left hashing & deletions
./test_count_min_sketch           # 17 tests - Error bounds & accuracy
./test_enhanced_globimap          # 10 tests - Conservative updates

# Build and run comparison experiment
make compare_all_implementations
./compare_all_implementations     # Side-by-side performance comparison
```

### Usage Examples

**Variable-Increment CBF:**
```cpp
#include "variable_increment_bf.hpp"
using namespace globimap;

VICBFConfig conf{8, 20, 16, 4};  // k=8, size=2^20, 16-bit counters, L=4
VariableIncrementBloomFilter cbf(conf);

std::vector<uint64_t> point = {123, 456};
cbf.put(point);

// ✓ CORRECT USAGE: Membership testing
bool present = cbf.get_bool(point);  // true

// ✗ INCORRECT USAGE: Frequency estimation (provides ~4x overcounting)
uint64_t count = cbf.get_min(point); // ~4-7 (actual=1, but increment ∈ [4,7])
// Do NOT use get_min() for frequency - use Spectral BF or Count-Min Sketch instead
```

**Spectral Bloom Filter (MI variant):**
```cpp
#include "spectral_bloom_filter.hpp"
using namespace globimap;

SBFConfig conf{8, 20, 16, MINIMAL_INCREMENT};  // Conservative update
SpectralBloomFilter sbf(conf);

std::vector<uint64_t> point = {123, 456};
for (int i = 0; i < 100; ++i) sbf.put(point);
uint64_t count = sbf.get_min(point);  // Very close to 100
```

**d-Left Counting Bloom Filter:**
```cpp
#include "dleft_counting_bf.hpp"
using namespace globimap;

DLeftCBFConfig conf{2048, 4, 4, 3, 8};  // buckets, slots, d-subtables, fp_bits, counter_bits
DLeftCountingBloomFilter cbf(conf);

std::vector<uint64_t> point = {123, 456};
cbf.put(point);
uint64_t count = cbf.get_min(point);  // 1
cbf.remove(point);                     // Deletion supported
count = cbf.get_min(point);            // 0
```

**Count-Min Sketch:**
```cpp
#include "count_min_sketch.hpp"
using namespace globimap;

CMSConfig conf{0.01, 0.01, true, 16};  // ε=0.01, δ=0.01, conservative, 16-bit
CountMinSketch cms(conf);

std::vector<uint64_t> point = {123, 456};
for (int i = 0; i < 100; ++i) cms.put(point);
uint64_t count = cms.get_min(point);  // ~100 with high probability

// Error guarantees: count <= true + ε*N with probability 1-δ
double epsilon = cms.epsilon_actual();
double delta = cms.delta_actual();
```

**Enhanced CountingGloBiMap:**
```cpp
#include "counting_globimap.hpp"
using namespace globimap;

FilterConfig conf;
conf.hash_k = 8;
conf.layers = {{8, 16}, {16, 14}};    // 2^16 8-bit + 2^14 16-bit counters
conf.minimal_increment = true;         // Conservative update
conf.cascade_factor = 0.75;            // Cascade at 75% of max

CountingGloBiMap<> gbm(conf);

std::vector<uint64_t> point = {123, 456};
for (int i = 0; i < 100; ++i) gbm.put(point);
uint64_t count = gbm.get_min(point);  // Very close to 100
```


### Experiments Structure

All experiment code is in `experiments/src/`:
- `globimap_test_dataset.cpp` - Test with HDF5 datasets, includes error detection
- `globimap_test_dataset_new.cpp` - Alternative dataset testing with performance benchmarks
- `globimap_test_datasets_for_k.cpp` - Sweep over different k values
- `globimap_test_polygon.cpp` - Polygon/shapefile processing
- `globimap_test_polygons_mask.cpp` - Polygon mask tests
- `globimap_rasterize_polys.cpp` - Rasterize polygons utility
- `globimap_print_poly_stats.cpp` - Print polygon statistics
- `globimap_test_dataset_full_time.cpp` - Full timing benchmarks
- `globimap_test_cos.cpp` - COS dataset tests

The `globimap_test_config.hpp` header provides utilities for generating and saving filter configurations as JSON.

## Dependencies

All dependencies are vendored as git submodules in `lib/`:
- **HighFive** - HDF5 C++ wrapper for dataset I/O
- **shapelib** - Shapefile reading for polygon experiments
- **tqdm.cpp** / **tqdm-cpp** - Progress bars for long-running experiments
- **boost-headers-only** - Boost utilities (header-only)

Dependencies are automatically built via CMake subdirectories.

## OpenMP Parallelization

The codebase uses OpenMP throughout. Parallelization is controlled by the `MAKE_PARALLEL` macro in `counting_globimap.hpp`:
- When defined: Enables `#pragma omp` directives
- When undefined: Macros expand to no-ops

The CMake flags include `-fopenmp` by default.

## HDF5 Configuration

The project expects HDF5 in the Debian/Ubuntu serial layout:
- Include: `/usr/include/hdf5/serial`
- Libs: `/usr/lib/x86_64-linux-gnu/hdf5/serial`

If HDF5 is in a different location, update paths in `CMakeLists.txt` lines 9, 33-35.

## Compiler Settings

- **Standard**: C++17 (set via `-std=c++17`, though CMakeLists.txt declares C++20)
- **Flags**: `-fopenmp -march=native -fPIC`
- **Optimization**: `-Ofast` for Release, `-g` for Debug

The `-march=native` flag optimizes for the build machine's CPU architecture.
