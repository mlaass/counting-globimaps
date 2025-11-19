# CBF Dataset Encoder

Command-line tool to encode datasets into binary `.cbf` files for the WASM frontend.

## Overview

The encoder loads spatial datasets (HDF5 or CSV format) and creates counting bloom filter representations that can be loaded in the browser via WebAssembly.

## Features

- **Multiple filter types** - Count-Min Sketch, Spectral BF, CountingGloBiMap
- **HDF5 and CSV input** - Load existing datasets
- **Multi-category support** - Encode datasets with category labels
- **Configurable parameters** - Tune filter size, hash functions, error bounds
- **Binary serialization** - Compact `.cbf` format for web delivery

## Building

The encoder is built as part of the main project:

```bash
# From project root
mkdir -p build && cd build
cmake ..
make encode_dataset
```

Output: `./encode_dataset` executable

## Usage

### Basic Usage

```bash
./encode_dataset INPUT.h5 OUTPUT.cbf [OPTIONS]
```

### Examples

**Encode GDELT dataset with Count-Min Sketch:**
```bash
./encode_dataset \
  ../datasets/hdf5/gdelt_events_multicategory.h5 \
  ../frontend/public/datasets/gdelt.cbf \
  --type cms \
  --epsilon 0.01 \
  --delta 0.01 \
  --conservative \
  --verbose
```

**Encode COVID-19 dataset with custom parameters:**
```bash
./encode_dataset \
  ../datasets/hdf5/covid19_cases.h5 \
  ../frontend/public/datasets/covid.cbf \
  --type cms \
  --epsilon 0.005 \
  --delta 0.001 \
  --counter-bits 16 \
  --verbose
```

**Encode with Spectral Bloom Filter:**
```bash
./encode_dataset \
  data.h5 \
  output.cbf \
  --type spectral \
  --hash-k 12 \
  --logsize 22 \
  --verbose
```

## Options

### Filter Type

- `--type TYPE` - Filter implementation: `cms`, `spectral`, `globimap`
  - Default: `cms`

### Count-Min Sketch Parameters

- `--epsilon FLOAT` - Error bound (ε), typical range: 0.001 to 0.1
  - Default: `0.01`
  - Lower = more accuracy, larger memory
- `--delta FLOAT` - Failure probability (δ), typical range: 0.001 to 0.1
  - Default: `0.01`
  - Lower = higher confidence, more hash functions
- `--conservative` - Enable conservative update (recommended)
  - Default: enabled
- `--counter-bits N` - Counter bit width: `8`, `16`, `32`, or `64`
  - Default: `16`
  - Higher = support larger counts, more memory

### Spectral Bloom Filter Parameters

- `--hash-k N` - Number of hash functions (typical: 4-12)
  - Default: `8`
- `--logsize N` - Log₂ of filter size (typical: 16-24)
  - Default: `20` (1M counters)
  - Size = 2^logsize counters

### Coordinate Transformation

- `--width N` - Grid width in cells
  - Default: `3600` (0.1° resolution)
- `--height N` - Grid height in cells
  - Default: `1800` (0.1° resolution)

### Other

- `--verbose` - Print progress information
- `--help` - Show help message

## Input Dataset Format

### HDF5 Format

Expected structure:
- Dataset name: `data`, `coordinates`, or `points`
- Shape: `(N, 2)` for 2D or `(N, 3)` for 3D with categories
- Columns:
  - `[longitude, latitude]` for 2D
  - `[longitude, latitude, category]` for 3D

Example:
```
data: (1934567, 3)
  Row 0: [-122.4194, 37.7749, 1]  # San Francisco, category 1
  Row 1: [-73.9352, 40.7306, 2]   # New York, category 2
  ...
```

### CSV Format (Future)

Planned support for CSV with header:
```csv
longitude,latitude,category
-122.4194,37.7749,1
-73.9352,40.7306,2
...
```

## Output Format

Binary `.cbf` file structure:

```
[Config Header]
  - width (4 bytes)
  - depth (4 bytes)
  - counter_bits (1 byte)
  - conservative (1 byte)
  - epsilon (8 bytes)
  - delta (8 bytes)
  - total_count (8 bytes)

[Counter Data]
  - Serialized counter matrix
  - Size depends on filter type and parameters
```

## Parameter Selection Guide

### Small Datasets (<100K points)

```bash
--epsilon 0.005 --delta 0.01 --counter-bits 16
# Memory: ~500 KB, Accuracy: Very high
```

### Medium Datasets (100K-1M points)

```bash
--epsilon 0.01 --delta 0.01 --counter-bits 16
# Memory: ~200 KB, Accuracy: High (default)
```

### Large Datasets (1M-10M points)

```bash
--epsilon 0.02 --delta 0.05 --counter-bits 16
# Memory: ~50 KB, Accuracy: Good
```

### Minimal Memory (IoT/Mobile)

```bash
--epsilon 0.05 --delta 0.1 --counter-bits 8
# Memory: ~3 KB, Accuracy: Moderate
```

## Memory Usage Estimation

For Count-Min Sketch:
```
width = ceil(e / epsilon)  ≈ 2.718 / epsilon
depth = ceil(ln(1 / delta)) ≈ -ln(delta)
memory = width × depth × (counter_bits / 8)
```

Examples:
- ε=0.01, δ=0.01, 16-bit: 272 × 5 × 2 = **2.7 KB**
- ε=0.001, δ=0.001, 16-bit: 2719 × 7 × 2 = **37 KB**

## Workflow Example

Full pipeline from raw data to deployed frontend:

```bash
# 1. Convert CSV to HDF5 (if needed)
uv run datasets/utils/csv_to_hdf5.py \
  --dataset gdelt

# 2. Encode to .cbf
./build/encode_dataset \
  datasets/hdf5/gdelt_events_multicategory.h5 \
  frontend/public/datasets/gdelt.cbf \
  --type cms \
  --verbose

# 3. Launch frontend
cd frontend
npm start
```

Then load `gdelt.cbf` in the web application.

## Troubleshooting

### "Failed to open HDF5 file"

- Check file path is correct
- Ensure HDF5 libraries are installed
- Verify file is valid HDF5 format

### "Dataset not found"

- Check dataset name in HDF5 file
- Tool looks for: `data`, `coordinates`, or `points`
- Use h5dump or h5ls to inspect file

### Memory Issues

- Reduce epsilon (increases memory requirement)
- Reduce logsize for Spectral BF
- Use 8-bit counters instead of 16-bit

### Accuracy Too Low

- Decrease epsilon (tighter error bound)
- Decrease delta (higher confidence)
- Enable conservative update
- Use higher counter bits (16 or 32)

## Next Steps

After encoding:
1. Copy `.cbf` file to `frontend/public/datasets/`
2. Build WASM module (see `wasm/README.md`)
3. Launch frontend application
4. Load dataset in browser

## Related Documentation

- [Main README](../README.md)
- [WASM Module README](../wasm/README.md)
- [Frontend README](../frontend/README.md)
- [CLAUDE.md](../CLAUDE.md)
