# Counting GloBiMaps

A multi-layer counting bloom filter data structure for cardinality estimation of sparse spatial data.

This project is extracted from the GloBiMap research codebase and focuses specifically on the **CountingGloBiMap** implementation - a hierarchical counting bloom filter with cascading overflow layers.

## Overview

CountingGloBiMap extends the basic bloom filter concept with multiple layers of different bit depths (1, 8, 16, 32, 64 bits). When a counter in one layer reaches its maximum value, increments cascade to the next layer. This allows:

- **Compact storage** of varying-magnitude counts
- **Cardinality estimation** via min-count estimator
- **Error detection and magnitude tracking**
- **Efficient spatial hashing** with the hashing trick

## Key Features

- **Multi-layer architecture**: Hierarchical layers with configurable bit depths
- **Hashing trick**: Generate k hash functions from 2 base hashes: `h(i) = (h1 + (i+1)*h2) & mask`
- **Min-count estimator**: `get_min()` returns minimum count across k hash functions
- **Error analysis**: Built-in error detection and magnitude histograms
- **OpenMP parallelization**: Parallel processing throughout

## API Overview

### Core Methods

```cpp
// Configuration
CountingGloBiMap(const FilterConfig &conf, bool collect_input = false)

// Insert operations
void put(const std::vector<uint64_t> &point)
void put_all(const std::vector<uint64_t> &points)

// Query operations
bool get_bool(const std::vector<uint64_t> &point)
uint64_t get_min(const std::vector<uint64_t> &point)  // Min-count estimator
template<typename RT> RT get_mean(const std::vector<uint64_t> &point)

// Analysis
void detect_errors(uint64_t x, uint64_t y, uint64_t width, uint64_t height)
std::string summary()
std::string error_summary()
```

### Configuration

```cpp
struct LayerConfig {
    uint bits;      // Bit depth: 1, 8, 16, 32, or 64
    uint logsize;   // Layer size: 2^logsize
};

struct FilterConfig {
    uint hash_k;                           // Number of hash functions
    std::vector<LayerConfig> layers;       // Layer configurations
};

// Example: 3-layer filter with k=8 hash functions
FilterConfig config{
    8,  // k hash functions
    {
        {8, 24},   // Layer 0: 8-bit counters, 2^24 size
        {16, 20},  // Layer 1: 16-bit counters, 2^20 size
        {32, 16}   // Layer 2: 32-bit counters, 2^16 size
    }
};
```

## Building

### Prerequisites

- CMake >= 3.12
- C++17 compiler
- OpenMP
- HDF5 (for experiments with real datasets)

### Build Steps

```bash
# Clone with submodules
git clone --recursive <repository-url>
cd counting-globimaps

# Or if already cloned, initialize submodules
git submodule update --init --recursive

# Build
mkdir build
cd build
cmake ..
make -j$(nproc)
```

### Available Executables

- `test_datasets` - Test with HDF5 datasets (with error detection)
- `test_datasets_new` - Alternative dataset testing (with performance benchmarks)
- `test_datasets_for_k` - Test varying k (hash functions)
- `test_polygon` - Polygon/shapefile processing
- `test_polygons_mask` - Polygon mask tests
- `globimap_rasterize_polys` - Rasterize polygons utility
- `print_poly_stats` - Polygon statistics
- `test_dataset_full_time` - Full timing benchmarks
- `test_cos` - COS dataset tests

## Usage Example

```cpp
#include "counting_globimap.hpp"

// Configure a 2-layer counting filter
globimap::FilterConfig config{
    10,  // 10 hash functions
    {
        {8, 20},   // 8-bit layer, 2^20 size (1 MB)
        {16, 16}   // 16-bit layer, 2^16 size (128 KB)
    }
};

// Create filter with error tracking
auto filter = globimap::CountingGloBiMap(config, true);

// Insert points
filter.put({100, 200});
filter.put({150, 250});

// Query cardinality estimate
uint64_t count = filter.get_min({100, 200});

// Get summary statistics
std::cout << filter.summary() << std::endl;

// Detect errors in a region
filter.detect_errors(0, 0, 1024, 1024);
std::cout << filter.error_summary() << std::endl;
```

## Architecture

### File Structure

```
counting-globimaps/
├── include/
│   ├── counting_globimap.hpp  # Main CountingGloBiMap implementation
│   ├── hashfn.hpp             # Hash function interface
│   └── murmur.hpp             # MurmurHash implementation
├── experiments/
│   └── src/
│       ├── globimap_test_config.hpp  # Configuration utilities
│       └── *.cpp                     # Experiment executables
└── lib/
    ├── HighFive/              # HDF5 C++ wrapper
    ├── shapelib/              # Shapefile reading
    ├── tqdm.cpp/              # Progress bars
    ├── tqdm-cpp/              # Alternative progress bars
    └── boost-headers-only/    # Boost utilities
```

### Layer Design

Each layer is a template supporting multiple bit depths:

```cpp
template <typename BITS1 = bool, typename BITS8 = uint8_t,
          typename BITS16 = uint16_t, typename BITS32 = uint32_t,
          typename BITS64 = uint64_t>
struct Layer {
    uint bits;          // Active bit depth
    uint64_t size;      // Number of counters
    uint64_t mask;      // Hash mask
    std::vector<BITS1> f1;    // 1-bit storage
    std::vector<BITS8> f8;    // 8-bit storage
    // ... etc
};
```

When incrementing a counter:
1. Check if current layer is at max value
2. If yes, cascade to next layer
3. If no, increment in current layer

### Hashing Trick

Instead of k independent hash functions, compute 2 base hashes (h1, h2) once, then:

```
hash[i] = (h1 + (i+1) * h2) & mask
```

This is cache-efficient and reduces computation while maintaining good distribution.

## Experiments

The `experiments/` directory contains research code for testing different configurations:

- **Dataset experiments**: Test with real geospatial datasets (Twitter, OSM)
- **Polygon experiments**: Rasterize and query polygon data
- **Configuration sweeps**: Test various layer configurations and k values
- **Performance benchmarks**: Measure insert/query throughput

Most experiments expect HDF5 datasets at specific paths (configurable in source).

## Dependencies

All dependencies are vendored as git submodules:

- **HighFive**: HDF5 C++ wrapper
- **shapelib**: Shapefile reading
- **tqdm.cpp** / **tqdm-cpp**: Progress bars
- **boost-headers-only**: Boost utilities

## License

[Specify license - inherits from parent GloBiMap project]

## References

Extracted from GloBiMap research project:
- Werner, M. (2019). GloBiMaps - A Probabilistic Data Structure for In-Memory Processing of Global Raster Datasets. In 27th ACM SIGSPATIAL International Conference on Advances in Geographic Information Systems (SIGSPATIAL '19).
- https://martinwerner.de/pdf/2019globimap.pdf
