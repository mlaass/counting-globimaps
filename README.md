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

## Alternative Counting Bloom Filter Implementations

In addition to the original **CountingGloBiMap**, this project now includes 4 alternative counting bloom filter implementations, each optimized for different use cases. All implementations are header-only C++ libraries with consistent APIs.

### Available Implementations

| Implementation | Best For | Memory | Speed | Accuracy | Features |
|----------------|----------|--------|-------|----------|----------|
| **CountingGloBiMap** | Varying count magnitudes | Medium | Good | Excellent* | Multi-layer, cascading |
| **Count-Min Sketch** | Minimal memory, error bounds | **Smallest** | **Fastest** | Excellent | Probabilistic guarantees |
| **Spectral BF** | High-frequency detection | Large | **Fastest** | **Perfect*** | 3 variants, optional deletions |
| **d-Left CBF** | Cache efficiency, deletions | Small | Fast | **Excellent** | Deterministic lookups, deletions |
| **Variable-Increment CBF** | **Membership testing ONLY** | Large | Fast | Poor for counts† | Simple, 50% memory vs standard CBF |

\* With `minimal_increment=true` option
† **WARNING**: 310% error for frequency queries - VI-CBF is designed for membership testing (`get_bool()`), NOT frequency estimation (`get_min()`)

### Performance Comparison

Benchmarked on 100K uniform inserts + 10K Zipfian queries (α=1.5):

| Implementation | Memory Usage | Insert Throughput | Query Accuracy |
|----------------|--------------|-------------------|----------------|
| Count-Min Sketch | **2.66 KB** | 40.5 M/sec | 0.04% error |
| d-Left CBF | 40 KB | 24.6 M/sec | **0.00% error** |
| CountingGloBiMap (MI) | 96 KB | 10.8 M/sec | **0.00% error** |
| Variable-Increment CBF | 2 MB | 17.7 M/sec | 310% error† |
| Spectral BF (MI) | 2 MB | **19.5 M/sec** | **0.00% error** |

† **Expected behavior** - VI-CBF is designed for membership testing (`get_bool()`), NOT frequency estimation. Use Spectral BF or Count-Min Sketch for accurate frequency queries.

### Quick Selection Guide

Choose the right implementation for your needs:

```
┌─────────────────────────────────────────┐
│  What's most important?                 │
└─────────────────────────────────────────┘
         │
         ├─ Need error bounds (ε, δ)?      → Count-Min Sketch
         │
         ├─ Need deletion support?         → d-Left CBF or Spectral BF (RM variant)
         │
         ├─ Minimal memory critical?       → Count-Min Sketch (2.66 KB)
         │
         ├─ Best accuracy needed?          → Spectral BF (MI) or CountingGloBiMap (MI)
         │
         ├─ Fastest inserts needed?        → Count-Min Sketch or Spectral BF (20-40M/sec)
         │
         ├─ Cache efficiency critical?     → d-Left CBF
         │
         ├─ Only membership testing?       → Variable-Increment CBF (use get_bool() only)
         │
         ├─ Need frequency estimation?     → Spectral BF (MI), Count-Min, or GloBiMap (MI)
         │
         └─ Varying count magnitudes?      → CountingGloBiMap (multi-layer)
```

**⚠️ Important**: Do NOT use Variable-Increment CBF for frequency estimation - it provides ~4x overcounting.

### C++ Usage Examples

All implementations follow a consistent API pattern:

**Count-Min Sketch** (minimal memory, error bounds):
```cpp
#include "count_min_sketch.hpp"
using namespace globimap;

CMSConfig conf{0.01, 0.01, true, 16};  // ε=0.01, δ=0.01, conservative, 16-bit
CountMinSketch cms(conf);

std::vector<uint64_t> point = {123, 456};
for (int i = 0; i < 100; ++i) cms.put(point);

uint64_t count = cms.get_min(point);  // ~100 with high probability
// Guarantees: count <= true_count + ε*N with probability 1-δ
```

**Spectral Bloom Filter** (perfect accuracy):
```cpp
#include "spectral_bloom_filter.hpp"
using namespace globimap;

SBFConfig conf{8, 20, 16, MINIMAL_INCREMENT};  // k=8, size=2^20, 16-bit, MI variant
SpectralBloomFilter sbf(conf);

std::vector<uint64_t> point = {123, 456};
for (int i = 0; i < 100; ++i) sbf.put(point);
uint64_t count = sbf.get_min(point);  // Very close to 100 (conservative update)
```

**d-Left Counting Bloom Filter** (cache-friendly, deletions):
```cpp
#include "dleft_counting_bf.hpp"
using namespace globimap;

DLeftCBFConfig conf{2048, 4, 4, 3, 8};  // buckets, slots, d-subtables, fp_bits, counter_bits
DLeftCountingBloomFilter cbf(conf);

std::vector<uint64_t> point = {123, 456};
cbf.put(point);
uint64_t count = cbf.get_min(point);  // 1
cbf.remove(point);                     // Deletion supported!
count = cbf.get_min(point);            // 0
```

**Variable-Increment CBF** (⚠️ **membership testing ONLY**):
```cpp
#include "variable_increment_bf.hpp"
using namespace globimap;

VICBFConfig conf{8, 20, 16, 4};  // k=8, size=2^20, 16-bit counters, L=4
VariableIncrementBloomFilter cbf(conf);

std::vector<uint64_t> point = {123, 456};
cbf.put(point);

// ✓ CORRECT: Use for membership testing
bool present = cbf.get_bool(point);  // true

// ✗ INCORRECT: Do NOT use for frequency estimation (310% error!)
uint64_t count = cbf.get_min(point); // ~4-7 (actual=1, increment ∈ [4,7])
// For accurate frequency queries, use Spectral BF or Count-Min Sketch instead
```

**Enhanced CountingGloBiMap** (conservative updates):
```cpp
#include "counting_globimap.hpp"
using namespace globimap;

FilterConfig conf;
conf.hash_k = 8;
conf.layers = {{8, 16}, {16, 14}};    // 2^16 8-bit + 2^14 16-bit counters
conf.minimal_increment = true;         // NEW: Conservative update (reduces overcounting)
conf.cascade_factor = 0.75;            // NEW: Cascade at 75% of max (prevents saturation)

CountingGloBiMap<> gbm(conf);
for (int i = 0; i < 100; ++i) gbm.put({123, 456});
uint64_t count = gbm.get_min({123, 456});  // Very close to 100
```

### Running Unit Tests

Each implementation has comprehensive unit tests:

```bash
cd build

# Build all tests
make test_count_min_sketch
make test_spectral_bloom_filter
make test_dleft_counting_bf
make test_variable_increment_bf
make test_enhanced_globimap

# Run tests
./test_count_min_sketch           # 17 tests - error bounds, conservative update
./test_spectral_bloom_filter      # 17 tests - MS/MI/RM variants
./test_dleft_counting_bf          # 14 tests - d-left hashing, deletions
./test_variable_increment_bf      # 15 tests - variable increments, overflow
./test_enhanced_globimap          # 10 tests - minimal_increment, cascade_factor

# Run comparison benchmark
make compare_all_implementations
./compare_all_implementations     # Side-by-side comparison of all implementations
```

### Implementation Details

**Count-Min Sketch** ([Cormode & Muthukrishnan, 2005](https://doi.org/10.1016/j.jalgor.2003.12.001))
- Matrix structure: depth × width counters
- Error bound: `estimate ≤ true + ε·N` with probability `1-δ`
- Optional conservative update (only increment minimum counters)
- Optimal for minimal memory with provable guarantees

**Spectral Bloom Filter** ([Cohen & Matias, 2003](https://doi.org/10.1145/945394.945396))
- Three variants: MS (Minimum Selection), MI (Minimal Increment), RM (Recurring Minimum)
- MI variant uses conservative update for near-perfect accuracy
- RM variant maintains secondary filter for deletion support
- Optimal for high-frequency item detection

**d-Left Counting Bloom Filter** ([Bonomi et al., 2006](https://doi.org/10.1016/j.comnet.2005.07.016))
- d-left hashing with fingerprints for space efficiency
- Deterministic query time (no hash collision chains)
- Cache-friendly bucket design
- Supports deletions via fingerprint matching

**Variable-Increment CBF** ([Rottenstreich et al., 2014](https://doi.org/10.1109/TNET.2013.2280836))
- Single-layer design with variable increments [L, 2L-1]
- 50% memory reduction vs standard CBF (from paper)
- Simple 4-parameter configuration
- Best for membership testing, not frequency estimation

**CountingGloBiMap** ([Werner, 2019](https://martinwerner.de/pdf/2019globimap.pdf))
- Multi-layer hierarchical design with cascading overflow
- NEW: `minimal_increment` option for conservative updates
- NEW: `cascade_factor` option for early cascade
- Best for data with varying count magnitudes

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

### Basic Examples

- **example_counting_basic.py** - Basic usage and operations
- **example_multi_layer.py** - Multi-layer configuration
- **example_error_detection.py** - Error detection and analysis
- **example_cardinality.py** - Cardinality estimation

### Real-World Dataset Examples

- **example_covid19_analysis.py** - Analyze COVID-19 case data from Johns Hopkins CSSE
- **example_gdelt_analysis.py** - Analyze GDELT global events (conflicts, protests, political events)
- **example_infrastructure_failures.py** - Analyze NYC 311 infrastructure failure patterns
- **example_compression_benchmark.py** - Memory compression benchmark with varying sparsity

Run examples:
```bash
cd examples
python example_counting_basic.py
python example_covid19_analysis.py
```

## Datasets

CountingGloBiMap is designed for sparse spatial datasets with hotspot patterns. Use the provided script to download sample datasets:

```bash
# Download all available datasets
./download_datasets.sh

# Or download specific datasets
./download_datasets.sh covid19
./download_datasets.sh infrastructure
```

Available datasets:
- **COVID-19 Case Data** (Johns Hopkins CSSE) - Auto-download
- **GDELT Global Events** (Conflicts, protests, political events worldwide) - Auto-download
- **Infrastructure Failures** (NYC 311 Service Requests) - Auto-download
- **Lightning Strike Data** (NOAA) - Manual download instructions provided
- **Wildlife Poaching** (CITES Trade Database) - Manual download instructions provided
- **Human Trafficking** (CTDC Database) - Manual download instructions with ethical guidelines

See `datasets/README.txt` (created by download script) for details on each dataset.

## Jupyter Notebooks

The `notebooks/` directory contains Jupyter notebooks for analysis:

### Analysis Notebooks (from experiments)

- Dataset experiments: `test_datasets_*.ipynb`
- Polygon experiments: `test_polygon*.ipynb`
- Resolution analysis: `resolutions*.ipynb`
- Abundance analysis: `AbundanceR.ipynb`

### Interactive Tutorials

- **RealWorld_Dataset_Analysis.ipynb** - Complete walkthrough of COVID-19, infrastructure, and synthetic data analysis

Run notebooks:
```bash
jupyter notebook notebooks/RealWorld_Dataset_Analysis.ipynb
```

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
