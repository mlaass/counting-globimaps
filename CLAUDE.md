# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

CountingGloBiMap is a multi-layer counting bloom filter implementation for cardinality estimation of sparse spatial data. This is a header-only C++ library extracted from the GloBiMap research codebase, featuring hierarchical layers with cascading overflow and OpenMP parallelization.

## Python Environment (uv)

This project uses [uv](https://github.com/astral-sh/uv) for Python package management. Python scripts are used for dataset conversion and analysis.

### Setup Python Environment
```bash
# Install dependencies (creates .venv automatically)
uv sync

# Run Python scripts with uv
uv run python script.py

# Add new Python dependencies
uv add package-name
```

### Convert CSV Datasets to HDF5
```bash
# Convert all datasets (default: GDELT all events, COVID-19 1% sampling)
uv run datasets/utils/csv_to_hdf5.py --dataset all

# Convert specific dataset
uv run datasets/utils/csv_to_hdf5.py --dataset gdelt
uv run datasets/utils/csv_to_hdf5.py --dataset covid

# Adjust sampling rates
uv run datasets/utils/csv_to_hdf5.py --gdelt-sample 100000        # Sample 100K GDELT events
uv run datasets/utils/csv_to_hdf5.py --covid-sample-rate 0.05     # Sample 5% of COVID cases
```

**Datasets:**
- **GDELT**: 1.9M global news events with geographic coordinates (12-month sample)
- **COVID-19**: 1.8M case events (sampled at 1% from 182M total cases on 2021-06-30)
  - Creates realistic spatial distribution with hotspots (USA, India, Brazil, etc.)
  - Each confirmed case becomes one coordinate entry at that location

Converted HDF5 files are saved to `datasets/hdf5/`.

### Analyze Results with Jupyter Notebooks

Use the modern dataset-agnostic notebook:

```bash
cd notebooks
uv run jupyter notebook dataset_comparison_analysis.ipynb
```

**Key features:**
- Auto-discovers all `.h5` datasets
- Loads results from organized subdirectories
- Works with any dataset without code changes
- Provides summary tables and visualizations

**Quick analysis:**
```python
from utils import quick_analysis
datasets, results, summary = quick_analysis("dataset_comparison")
print(summary)
```

### Generate Reports

Generate structured markdown reports with graphs and tables:

```bash
# Generate all reports
./reports/generate_reports.sh
```

**Reports generated:**
- `output/dataset_comparison_report.md` - Performance comparison on GDELT/COVID-19 datasets
- `output/multicategory_report.md` - Category isolation and accuracy analysis
- `output/k_sensitivity_report.md` - K parameter sensitivity (placeholder)
- `output/cosine_report.md` - Cosine distribution benchmark (placeholder)
- `output/implementation_comparison_report.md` - Quick baseline comparison (placeholder)

Each report includes:
- Embedded PNG figures in markdown
- Companion PDF with all figures (`*_figures.pdf`)
- Summary tables and key findings

Reports are saved to `reports/output/`, figures to `reports/figures/`.

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

### Running Comparison Experiments
```bash
# From project root (not build/)
./run_all_experiments.sh
```

This runs all comparison experiments sequentially and saves results to a timestamped directory (`results_YYYY-MM-DD_HH-MM-SS/`).

Note: Dataset comparison experiments expect HDF5 datasets in `./datasets/hdf5/`. Use the CSV-to-HDF5 conversion script to prepare datasets first.

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

- `include/globimap.hpp` - Simple binary bloom filter (header-only)
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

## Bloom Filter Implementations

The project includes 10 bloom filter implementations, each with different trade-offs. All are header-only and located in `include/`:

### Implementation Overview

1. **GloBiMap** (`globimap.hpp`) - Simple binary bloom filter
   - Classic bloom filter with single bit per position
   - Membership testing only (no counting)
   - Includes rasterization for spatial queries
   - Includes error correction for false positive suppression
   - Serialization support (tobuffer/frombuffer)
   - Best for: Simple membership testing, spatial rasterization

2. **CountingGloBiMap** (`counting_globimap.hpp`) - Multi-layer hierarchical filter
   - Original implementation with cascading layers
   - NEW: Optional `minimal_increment` for conservative updates (reduces overcounting)
   - NEW: Optional `cascade_factor` for early cascade (prevents saturation)
   - Best for: Highly skewed data with varying count magnitudes

3. **Variable-Increment CBF** (`variable_increment_bf.hpp`) - Simple single-layer filter
   - Based on Rottenstreich et al. (2014)
   - Uses variable increments [L, 2L-1] for 50% memory savings
   - Simple 4-parameter configuration
   - **⚠️ WARNING**: Provides ~4-5x overcounting for frequency queries (310% error)
   - Best for: **Membership testing only** (`get_bool()`), NOT frequency estimation

4. **Spectral Bloom Filter** (`spectral_bloom_filter.hpp`) - Multiple variants
   - Based on Cohen & Matias (2003)
   - Three variants: MS (Minimum Selection), MI (Minimal Increment), RM (Recurring Minimum)
   - MI variant provides conservative updates for better accuracy
   - RM variant supports deletions
   - Best for: High-frequency item detection with accuracy

5. **d-Left Counting Bloom Filter** (`dleft_counting_bf.hpp`) - Deterministic lookups
   - Based on Bonomi et al. (2006)
   - Uses d-left hashing with fingerprints for cache-friendly design
   - Supports deletions
   - Best for: Cache-sensitive applications, deterministic query time

6. **Count-Min Sketch** (`count_min_sketch.hpp`) - Probabilistic guarantees
   - Based on Cormode & Muthukrishnan (2005)
   - Provides error bounds (epsilon, delta parameters)
   - Optional conservative update
   - Best for: When you need provable error guarantees, minimal memory

### Cache-Optimal Bloom Filters

Additional membership-only bloom filters optimized for cache efficiency. From [save-buffer/bloomfilter_benchmarks](https://github.com/save-buffer/bloomfilter_benchmarks).

7. **BlockedBloomFilter** (`blocked_bloom_filter.hpp`) - Cache-line aligned
   - 256-bit blocks aligned to cache lines
   - First hash selects block, remaining hashes probe within block
   - Reduces cache misses through spatial locality
   - Best for: Memory-constrained systems with cache sensitivity

8. **RegisterBlockedBloomFilter** (`register_blocked_bf.hpp`) - 64-bit atomic masks
   - Uses 64-bit registers as atomic units
   - Single mask operation per lookup
   - Compensation parameter tunes memory/FPR tradeoff
   - Best for: Very fast membership testing

9. **SimdBloomFilter** (`simd_bloom_filter.hpp`) - AVX2 vectorized
   - Processes 8 elements in parallel using AVX-256
   - Uses gather instructions for efficient block lookups
   - Batch operations for maximum throughput
   - Requires AVX2 support (compile with `-march=native`)
   - Best for: High-throughput applications with SIMD support

10. **PatternedSimdBloomFilter** (`simd_bloom_filter.hpp`) - Advanced SIMD
    - Pre-generated mask patterns with rotation
    - Better FPR than standard SimdBloomFilter
    - Also requires AVX2 support
    - Best for: Best balance of throughput and accuracy

### Quick Selection Guide

Choose based on your requirements:

```
Need error bounds?          → Count-Min Sketch
Need deletions?             → d-Left CBF or Spectral BF (RM variant)
Need minimal memory?        → Count-Min Sketch (2.66 KB typical)
Need best accuracy?         → Spectral BF (MI) or CountingGloBiMap (MI)
Need cache efficiency?      → d-Left CBF, BlockedBF, or RegisterBlockedBF
Need SIMD throughput?       → SimdBloomFilter or PatternedSimdBloomFilter
Need membership only?       → GloBiMap, BlockedBF, or RegisterBlockedBF
Need frequency estimation?  → Spectral BF (MI), Count-Min Sketch, or CountingGloBiMap (MI)
Need varying magnitudes?    → CountingGloBiMap (multi-layer)
Need spatial rasterization? → GloBiMap (binary)
```

**⚠️ WARNING**: Do NOT use Variable-Increment CBF for frequency estimation - it provides ~2-5x overcounting due to variable increments [L, 2L-1]. Use `get_bool()` for membership only.

### Building and Running Unit Tests

```bash
# From build/ directory

# Build individual test suites
make test_globimap
make test_variable_increment_bf
make test_spectral_bloom_filter
make test_dleft_counting_bf
make test_count_min_sketch
make test_enhanced_globimap
make test_cache_optimal_bf        # Cache-optimal BF (requires AVX2)

# Run tests
./test_globimap                   # 14 tests - Binary bloom filter
./test_variable_increment_bf      # 15 tests - VI-CBF correctness
./test_spectral_bloom_filter      # 17 tests - All SBF variants
./test_dleft_counting_bf          # 14 tests - d-Left hashing & deletions
./test_count_min_sketch           # 17 tests - Error bounds & accuracy
./test_enhanced_globimap          # 10 tests - Conservative updates
./test_cache_optimal_bf           # 22 tests - Cache-optimal implementations

# Build and run comparison experiments
make compare_all_implementations
./compare_all_implementations     # Side-by-side performance comparison

make bloom_filter_benchmark
./bloom_filter_benchmark          # Cache-optimal BF benchmark with SIMD
```

### Usage Examples

**GloBiMap (Binary Bloom Filter):**
```cpp
#include "globimap.hpp"

GloBiMap<> bf;
bf.configure(8, 20);  // 8 hash functions, 2^20 bits (128 KB)

std::vector<uint64_t> point = {123, 456};
bf.put(point);
bool present = bf.get(point);  // true

// Rasterize a region for spatial queries
auto &raster = bf.rasterize(100, 100, 50, 50);  // 50x50 region starting at (100,100)

// Serialization
std::string buf;
bf.tobuffer(buf);
GloBiMap<> bf2;
bf2.configure(8, 20);
bf2._frombuffer(buf, bf.filter.size());
```

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

## Multi-Category Support

All implementations support **variable-length point vectors** for tracking multiple categories at the same spatial location. This enables applications like:
- Event classification (e.g., GDELT QuadClass: verbal/material cooperation/conflict)
- Time-series binning (track counts per time bucket at each location)
- Multi-attribute filtering (location + category + subcategory)

### Critical Hash Function Bug Fix

**⚠️ Fixed in commit [hash]**: The hash wrapper in `include/hashfn.hpp` previously hardcoded the input length to 16 bytes, causing all implementations to ignore additional vector elements beyond `[x, y]`.

**Before (BROKEN)**:
```cpp
// hashfn.hpp line 33 - WRONG: ignores len parameter
murmur::MurmurHash3_x64_128(data, 16, *v1, (void *)hash);
```

**After (FIXED)**:
```cpp
// hashfn.hpp line 33 - CORRECT: uses actual vector length
murmur::MurmurHash3_x64_128(data, len * sizeof(uint64_t), *v1, (void *)hash);
```

**Impact**: Without this fix, categories were not isolated - all queries returned the total count across all categories.

### Usage

All implementations automatically support variable-length vectors through the same `put()` / `get_min()` interface:

```cpp
#include "counting_globimap.hpp"  // or any other implementation
using namespace globimap;

FilterConfig conf;
conf.hash_k = 8;
conf.layers = {{8, 20}, {16, 18}};
conf.minimal_increment = true;
CountingGloBiMap<> filter(conf);

// Insert events at same location with different categories
filter.put({100, 200, 1});  // Category 1 at (100, 200)
filter.put({100, 200, 1});
filter.put({100, 200, 2});  // Category 2 at (100, 200)
filter.put({100, 200, 2});
filter.put({100, 200, 2});

// Query by category - completely isolated
uint64_t cat1 = filter.get_min({100, 200, 1});  // Returns ~2
uint64_t cat2 = filter.get_min({100, 200, 2});  // Returns ~3
uint64_t cat3 = filter.get_min({100, 200, 3});  // Returns 0 (never inserted)
```

**Backward Compatibility**: All existing 2D code continues to work without modification:
```cpp
filter.put({x, y});           // Still works (2-element vector)
uint64_t count = filter.get_min({x, y});
```

### Validation Results

**Unit Tests** (`tests/test_multicategory.cpp`):
- All 4 implementations achieve **0% error, 100% category isolation**
- Categories at the same location are completely independent
- Run with: `./build/test_multicategory`

**Synthetic Benchmark** (`experiments/src/globimap_test_multicategory.cpp`):
- Tests same location (1000, 2000) with 4 categories: 100, 500, 50, 1000 inserts
- Results: `./results/compare_multicategory.json`

| Implementation | Mean Error % | Max Error % | Isolation % |
|----------------|--------------|-------------|-------------|
| Spectral BF (MI) | 0.00 | 0.00 | 100.00 |
| d-Left CBF | 0.00 | 0.00 | 100.00 |
| Count-Min Sketch | 0.00 | 0.00 | 100.00 |
| CountingGloBiMap (MI) | 0.00 | 0.00 | 100.00 |

### Real-World Performance: GDELT Multi-Category Dataset

**Dataset**: 1.9M GDELT events with QuadClass categories (derived from Goldstein scale)
- Category 1 (Verbal Cooperation): 882K events (45.6%)
- Category 2 (Material Cooperation): 262K events (13.6%)
- Category 3 (Verbal Conflict): 598K events (30.9%)
- Category 4 (Material Conflict): 191K events (9.9%)

**Conversion**:
```bash
# Convert GDELT CSV to multi-category HDF5 [lat, lon, category] format
uv run datasets/utils/convert_gdelt_multicategory.py \
    datasets/gdelt/gdelt_events_sample.csv \
    -o datasets/hdf5/gdelt_events_multicategory.h5
```

**Benchmark Results** (`./build/globimap_test_multicategory_dataset`):

| Implementation | Memory | Insert Time | Query Time | Category Isolation |
|----------------|--------|-------------|------------|-------------------|
| Spectral BF (MI) | 2 MB | 0.21s | 0.50 μs | Perfect (0% error) |
| Count-Min Sketch | 88 KB | 0.33s | 1.09 μs | Perfect (0% error) |
| CountingGloBiMap (MI) | 1.5 MB | 0.82s | 0.61 μs | Perfect (0.11% error) |
| d-Left CBF | 95 KB | 0.32s | 1.37 μs | Good (14-21% error) |

All implementations correctly isolate categories - queries for category N only return counts for category N, not the total across all categories.

### Multi-Category Experiments

```bash
# Build multi-category tests and benchmarks
cd build
make test_multicategory                       # Unit tests
make globimap_test_multicategory             # Synthetic benchmark
make globimap_test_multicategory_dataset     # Real GDELT benchmark

# Run experiments
./test_multicategory                         # Quick validation
./globimap_test_multicategory                # Synthetic: 4 categories at 1 location
./globimap_test_multicategory_dataset        # Real: 1.9M GDELT events, 4 categories
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
- **`globimap_test_multicategory.cpp`** - Multi-category isolation benchmark (synthetic)
- **`globimap_test_multicategory_dataset.cpp`** - Multi-category GDELT dataset benchmark
- `compare_all_implementations.cpp` - Quick baseline comparison (3 memory budgets)
- `globimap_test_dataset_compare.cpp` - Real dataset comparison (GDELT, COVID-19)
- `globimap_test_cos_compare.cpp` - Cosine distribution comparison
- `globimap_test_k_compare.cpp` - K parameter sensitivity analysis

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

## Research Papers

The repository includes draft longpapers documenting the Counting GloBiMap data structure:

### Paper Versions

**`cbf-longpaper-v1/`** - VLDB-formatted version
- Title: "Counting GloBiMaps - A Probabilistic Data Structure for Handling Big Point Datasets"
- Format: `\documentclass[sigconf, nonacm]{acmart}` (VLDB template)
- Main file: `cbf-longpaper-v1.tex`
- Complete draft with all sections

**`cbf-longpaper-v2/`** - ACM-formatted version
- Same content adapted for ACM conference format
- Format: `\documentclass[sigconf]{acmart}` (ACM template)
- Main file: `cbf-longpaper-v2.tex`
- Includes BibLaTeX support files (acmnumeric.bbx, acmauthoryear.cbx, etc.)
- Has some incomplete sections marked with `\todo{}`

### Paper Content Overview

Both papers cover:
1. **Introduction** - Motivation for probabilistic data structures in spatial computing
2. **Related Work** - Bloom filters, Counting Bloom filters, GloBiMaps
3. **Counting GloBiMaps** - Multi-layer architecture, PUT/GET operations
4. **Operations** - Union, Intersection (with merge functions), SUM for spatial statistics
5. **Multiresolution** - Re-randomization strategy between layers
6. **Evaluation** - Experiments on Twitter (213M points) and OSM Asia (1.5B points) datasets
7. **Point-in-Polygon** - Error analysis for polygon counting queries

### Key Concepts from Papers

- **Layered overflow handling**: Small bit-depth first layer absorbs long tail, overflows cascade up
- **Hashing trick**: Single MurmurHash3 call generates k hash functions via `h[i] = h1 + (i+1)*h2`
- **False positive error model**: `err_total = k * (t/n) * (1 - e^(-kn/m))`
- **Multiresolution re-randomization**: Higher layers use finer spatial resolution to restore hash uniformity

### Building Papers

```bash
# From paper directory
cd cbf-longpaper-v1  # or cbf-longpaper-v2
pdflatex cbf-longpaper-v1.tex
bibtex cbf-longpaper-v1
pdflatex cbf-longpaper-v1.tex
pdflatex cbf-longpaper-v1.tex
```

### Figures

Both versions include figures in `figures/`:
- `CGM_Layer_Schema.pdf` - Multi-layer architecture diagram
- `test_datasets_*.pdf` - Experimental results plots
- `polygons_mask_*.pdf` - Point-in-polygon error analysis
- Dataset visualizations (Twitter, OSM, US Census, Global boundaries)
