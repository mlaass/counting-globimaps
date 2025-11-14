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
