# Quick Start Guide

Get up and running with CountingGloBiMap in 5 minutes.

## Step 1: Clone and Build (2 minutes)

```bash
# Clone the repository with submodules
git clone --recursive https://github.com/yourusername/counting-globimaps.git
cd counting-globimaps

# Or if already cloned, initialize submodules
git submodule update --init --recursive

# Install Python dependencies
pip install -r requirements.txt

# Build the Python module
python setup.py build_ext --inplace
```

**Verify installation**:
```bash
python -c "import counting_globimap as cgm; print('Success!')"
```

## Step 2: Run Basic Example (30 seconds)

```bash
cd examples
python example_counting_basic.py
```

You should see output showing:
- Filter creation with 1 MB memory
- Basic put/get operations
- Count estimation accuracy

## Step 3: Try Real-World Data (2 minutes)

### Download a real-world dataset:
```bash
cd ..
./download_datasets.sh covid19  # Or: gdelt, infrastructure
```

### Run analysis:
```bash
cd examples
python example_covid19_analysis.py   # COVID-19 case patterns
# Or:
python example_gdelt_analysis.py     # Global events (conflicts, protests)
```

You should see:
- Global spatial data loaded
- Memory compression (typically 10-50x)
- Hotspot identification
- False positive rate < 1%

## Step 4: Interactive Exploration (optional)

```bash
# Start Jupyter
jupyter notebook notebooks/RealWorld_Dataset_Analysis.ipynb
```

This interactive notebook walks through:
- Synthetic data with hotspots
- COVID-19 spatial analysis
- Infrastructure failure patterns
- Accuracy and memory trade-offs

## What's Next?

### Learn the API

Basic usage pattern:

```python
import counting_globimap as cgm

# 1. Configure filter
config = cgm.make_multi_layer_config(
    k=3,  # hash functions
    layer_specs=[
        (1, 22),   # 1-bit, 2^22 buckets
        (8, 20),   # 8-bit, 2^20 buckets
        (16, 18),  # 16-bit, 2^18 buckets
    ]
)

# 2. Create filter
cgmap = cgm.CountingGloBiMap(config, collect_input=True)

# 3. Insert points
cgmap.put([x, y])

# 4. Query points
count = cgmap.get_min([x, y])
exists = cgmap.get_bool([x, y])

# 5. Analyze
errors = cgmap.detect_errors()
print(f"Memory: {cgmap.byte_size() / 1024:.1f} KB")
print(f"FP rate: {errors['fp_rate']*100:.2f}%")
```

### Try More Examples

```bash
cd examples

# Basic examples (no datasets required)
python example_multi_layer.py           # Multi-layer configuration
python example_error_detection.py       # Error analysis
python example_cardinality.py          # Cardinality estimation
python example_compression_benchmark.py # Memory comparison

# Real-world examples (require datasets)
python example_gdelt_analysis.py           # GDELT global events
python example_infrastructure_failures.py  # NYC 311 data
```

### Work with Your Own Data

1. **Convert your data to pixel coordinates**:
   ```python
   def coords_to_pixels(lat, lon, resolution=0.1):
       x = int((lon + 180) / resolution)
       y = int((90 - lat) / resolution)
       return x, y
   ```

2. **Choose appropriate configuration**:
   - **High sparsity** (< 0.01%): Multi-layer with small layers
   - **Medium sparsity** (0.01-1%): Multi-layer with balanced layers
   - **Low sparsity** (> 1%): Single-layer might be sufficient

3. **Insert your data**:
   ```python
   for lat, lon, count in your_data:
       x, y = coords_to_pixels(lat, lon)
       for _ in range(count):
           cgmap.put([x, y])
   ```

4. **Query and analyze**:
   ```python
   # Query specific location
   estimated_count = cgmap.get_min([x, y])

   # Check memory usage
   print(f"Memory: {cgmap.byte_size() / 1024 / 1024:.2f} MB")

   # Detect errors
   errors = cgmap.detect_errors()
   print(f"False positive rate: {errors['fp_rate']*100:.2f}%")
   ```

### Download More Datasets

```bash
# List available datasets
./download_datasets.sh --list

# Download specific dataset
./download_datasets.sh infrastructure

# Download all datasets
./download_datasets.sh
```

Available datasets:
- **COVID-19** (Johns Hopkins) - Auto-download ✓
- **GDELT** (Global events, 12 months, ~2M events) - Auto-download ✓
- **Infrastructure Failures** (NYC 311) - Auto-download ✓
- **Lightning Strikes** (NOAA) - Manual download instructions

### Explore C++ Implementation

If you want to use the C++ API directly:

```bash
# Build C++ executables
mkdir build
cd build
cmake ..
make -j$(nproc)

# Run C++ experiments
./test_datasets
./test_polygon
```

See `README.md` for complete C++ API documentation.

## Common Issues

### Build fails with "pybind11 not found"

```bash
# Initialize submodules
git submodule update --init --recursive
```

### Module import fails

```bash
# Ensure you're in the project root
cd /path/to/counting-globimaps

# Build module
python setup.py build_ext --inplace

# Verify
python -c "import counting_globimap"
```

### Dataset not found

```bash
# Download required dataset
./download_datasets.sh covid19
```

### OpenMP warnings

These are normal and can be safely ignored:
```
OMP: Warning #181: ...
```

To suppress them:
```bash
export OMP_DISPLAY_ENV=FALSE
```

## Performance Tips

1. **Choose k wisely**:
   - k=3-5: Fast, moderate accuracy
   - k=8-10: Slower, better accuracy
   - k=15+: Diminishing returns

2. **Layer sizing**:
   - First layer (1-bit): Should hold all unique pixels
   - Subsequent layers: Decrease by 2-4 orders of magnitude

3. **Disable input collection** for production:
   ```python
   cgmap = cgm.CountingGloBiMap(config, collect_input=False)
   ```
   This saves memory (~50% reduction)

4. **Use appropriate resolution**:
   - Too fine: Wastes memory
   - Too coarse: Loses spatial detail
   - Good starting point: 0.1° (≈11 km)

## Getting Help

- **Documentation**: See `README.md` for full API reference
- **Examples**: Check `examples/README.md` for detailed example descriptions
- **Notebooks**: Explore `notebooks/` for interactive tutorials
- **Issues**: Report bugs at [GitHub Issues](https://github.com/yourusername/counting-globimaps/issues)

## Benchmarks

Typical performance on modern hardware:

| Operation | Throughput |
|-----------|------------|
| Insert | 3-14 M points/sec |
| Query | 5-20 M queries/sec |
| Memory | 10-100x compression |
| Accuracy | < 5% error typical |

## Next Steps

1. ✅ Built the module
2. ✅ Ran basic examples
3. ✅ Tried real-world data
4. → **Experiment with your own datasets**
5. → **Try different configurations**
6. → **Read the research paper** ([Werner, 2019](https://martinwerner.de/pdf/2019globimap.pdf))

Happy mapping! 🗺️
