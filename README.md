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
- **Python bindings**: Full Python API via pybind11

## Quick Start (Python)

### Installation

```bash
# Install dependencies
pip install -r requirements.txt

# Build and install the Python module
python setup.py build_ext --inplace

# Or install in development mode
pip install -e .
```

### Basic Usage

```python
import counting_globimap as cg

# Create a simple filter configuration
config = cg.make_single_layer_config(
    k=8,           # Number of hash functions
    bits=8,        # 8-bit counters
    logsize=20     # 2^20 size (1 MB)
)

# Create the counting bloom filter
cbf = cg.CountingGloBiMap(config, collect_input=False)

# Insert points
cbf.put([100, 200])
cbf.put([150, 250])
cbf.put([100, 200])  # Insert again

# Query points
exists = cbf.get_bool([100, 200])  # True
count = cbf.get_min([100, 200])    # ~2 (estimate)

# Get summary statistics
print(cbf.summary())
```

### Multi-Layer Configuration

```python
# Create a multi-layer filter for high-cardinality data
config = cg.make_multi_layer_config(
    k=10,
    layer_specs=[
        (8, 24),   # Layer 0: 8-bit counters, 2^24 size
        (16, 20),  # Layer 1: 16-bit counters, 2^20 size
        (32, 16),  # Layer 2: 32-bit counters, 2^16 size
    ]
)

cbf = cg.CountingGloBiMap(config)
print(f"Memory: {cbf.byte_size() / 1024 / 1024:.2f} MB")
```

### Error Detection

```python
# Enable input collection for error detection
config = cg.make_single_layer_config(k=8, bits=8, logsize=20)
cbf = cg.CountingGloBiMap(config, collect_input=True)

# Insert data
for i in range(1000):
    cbf.put([i, i])

# Detect errors in a region
cbf.detect_errors(0, 0, 1024, 1024)
print(f"Error rate: {cbf.error_rate * 100:.4f}%")

# Get detailed error statistics
import json
errors = json.loads(cbf.error_summary())
print(f"Errors detected: {errors['errors']}")
```

## Python API Reference

### Classes

#### `LayerConfig`
- `bits` (int): Bit depth (1, 8, 16, 32, or 64)
- `logsize` (int): Log2 of layer size

#### `FilterConfig`
- `hash_k` (int): Number of hash functions
- `layers` (list[LayerConfig]): Layer configurations

#### `CountingGloBiMap`

**Constructor:**
- `CountingGloBiMap(config: FilterConfig, collect_input: bool = False)`

**Methods:**
- `put(point: list[int])` - Insert a point
- `put_all(points: list[int])` - Insert multiple points (flat list)
- `get_bool(point: list[int]) -> bool` - Check if point exists
- `get_min(point: list[int]) -> int` - Min-count estimate
- `get_mean(point: list[int]) -> float` - Mean-count estimate
- `detect_errors(x, y, width, height)` - Detect errors in region
- `error_magnitudes() -> list[int]` - Get error magnitudes
- `byte_size() -> int` - Total memory usage in bytes
- `summary() -> str` - JSON summary of filter state
- `error_summary() -> str` - JSON summary of errors

**Properties:**
- `hashcount` (int): Number of hash functions
- `collect_input` (bool): Whether input collection is enabled
- `error_rate` (float): Detected error rate
- `config` (FilterConfig): Filter configuration

### Helper Functions

- `make_single_layer_config(k, bits, logsize) -> FilterConfig`
- `make_multi_layer_config(k, layer_specs) -> FilterConfig`

## Examples

See the `examples/` directory for complete examples:

- **example_counting_basic.py** - Basic usage and operations
- **example_multi_layer.py** - Multi-layer configuration
- **example_error_detection.py** - Error detection and analysis
- **example_cardinality.py** - Cardinality estimation

Run examples:
```bash
cd examples
python example_counting_basic.py
```

## Jupyter Notebooks

The `notebooks/` directory contains 10 Jupyter notebooks for analyzing experiment results:

- Dataset experiments: `test_datasets_*.ipynb`
- Polygon experiments: `test_polygon*.ipynb`
- Resolution analysis: `resolutions*.ipynb`
- Abundance analysis: `AbundanceR.ipynb`

See `notebooks/README.md` for details.

## Testing

Run Python tests:
```bash
# Run all tests
pytest tests/

# Run with coverage
pytest --cov=counting_globimap tests/

# Run specific test
python tests/test_counting_globimap.py
```

## C++ API Overview

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
├── counting_globimap/
│   └── pybind.cpp             # Python bindings
├── examples/
│   ├── example_counting_basic.py
│   ├── example_multi_layer.py
│   ├── example_error_detection.py
│   └── example_cardinality.py
├── notebooks/
│   ├── test_datasets_*.ipynb  # Dataset analysis notebooks
│   ├── test_polygon*.ipynb    # Polygon experiment notebooks
│   ├── resolutions*.ipynb     # Resolution analysis
│   └── AbundanceR.ipynb       # Abundance analysis
├── tests/
│   └── test_counting_globimap.py  # Python unit tests
├── experiments/
│   └── src/
│       ├── globimap_test_config.hpp  # Configuration utilities
│       └── *.cpp                     # C++ experiment executables
├── lib/
│   ├── pybind11/              # Python bindings library
│   ├── HighFive/              # HDF5 C++ wrapper
│   ├── shapelib/              # Shapefile reading
│   ├── tqdm.cpp/              # Progress bars
│   ├── tqdm-cpp/              # Alternative progress bars
│   └── boost-headers-only/    # Boost utilities
├── setup.py                   # Python package setup
├── requirements.txt           # Python dependencies
└── CMakeLists.txt             # Build configuration
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

### C++ Dependencies

All C++ dependencies are vendored as git submodules:

- **pybind11**: Python/C++ bindings library
- **HighFive**: HDF5 C++ wrapper
- **shapelib**: Shapefile reading
- **tqdm.cpp** / **tqdm-cpp**: Progress bars
- **boost-headers-only**: Boost utilities
- **OpenMP**: Parallel processing (system library)

### Python Dependencies

See `requirements.txt` for complete list. Core dependencies:

- **numpy**: Array operations
- **h5py**: HDF5 file reading
- **pandas**: Data analysis
- **matplotlib**: Visualization
- **jupyter**: Notebook environment
- **pytest**: Testing framework

## License

[Specify license - inherits from parent GloBiMap project]

## References

Extracted from GloBiMap research project:
- Werner, M. (2019). GloBiMaps - A Probabilistic Data Structure for In-Memory Processing of Global Raster Datasets. In 27th ACM SIGSPATIAL International Conference on Advances in Geographic Information Systems (SIGSPATIAL '19).
- https://martinwerner.de/pdf/2019globimap.pdf
