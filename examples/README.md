# CountingGloBiMap Examples

This directory contains example scripts demonstrating how to use CountingGloBiMap for various use cases.

## Basic Examples

These examples demonstrate core functionality with synthetic data:

### example_counting_basic.py

**Purpose**: Introduction to basic CountingGloBiMap operations

**What it demonstrates**:
- Creating a single-layer filter configuration
- Inserting points with `put()`
- Querying points with `get_bool()` and `get_min()`
- Checking memory usage with `byte_size()`
- Getting summary statistics

**Run it**:
```bash
python example_counting_basic.py
```

**Expected output**: Demonstrates basic put/get operations and shows filter memory usage.

---

### example_multi_layer.py

**Purpose**: Multi-layer configuration for varying count magnitudes

**What it demonstrates**:
- Creating multi-layer filters with cascading overflow
- Inserting points with different frequencies (1x, 10x, 100x, 1000x)
- Comparing estimated vs actual counts
- Understanding when to use multi-layer vs single-layer

**Run it**:
```bash
python example_multi_layer.py
```

**Expected output**: Shows how multi-layer filters handle varying frequencies with better accuracy than single-layer.

---

### example_error_detection.py

**Purpose**: Error detection and false positive analysis

**What it demonstrates**:
- Enabling input collection with `collect_input=True`
- Using `detect_errors()` to find false positives
- Analyzing false positive rates
- Understanding the accuracy/memory trade-off

**Run it**:
```bash
python example_error_detection.py
```

**Expected output**: Error detection results showing false positive rates and error magnitudes.

---

### example_cardinality.py

**Purpose**: Cardinality estimation for spatial data

**What it demonstrates**:
- Estimating unique pixel counts in regions
- Handling Zipfian distribution (hotspot pattern)
- Regional cardinality analysis
- Comparing filter estimates vs ground truth

**Run it**:
```bash
python example_cardinality.py
```

**Expected output**: Cardinality estimates for different regions with accuracy metrics.

---

## Real-World Dataset Examples

These examples work with actual sparse spatial datasets to demonstrate practical applications.

### example_gdelt_analysis.py

**Purpose**: Analyze global events from news media worldwide

**Dataset**: GDELT 2.0 Events Database (Global Database of Events, Language, and Tone)

**Download dataset first**:
```bash
cd ..
./download_datasets.sh gdelt
```

**What it demonstrates**:
- Loading GDELT tab-separated event data
- Filtering by event type (conflict vs cooperation)
- Analyzing event hotspots globally
- Comparing conflict and cooperation patterns
- Impact analysis using Goldstein scale
- Very sparse data (< 0.01% of globe with events)

**Run it**:
```bash
python example_gdelt_analysis.py
```

**Expected output**:
- Loads 2-5M global events from news media (365 days)
- Shows event type distribution (verbal/material conflict/cooperation)
- Identifies top 20 event hotspots worldwide
- Regional analysis for political centers and conflict zones
- Conflict vs cooperation comparison
- Memory compression (typically 20-50x)

**Example output**:
```
GDELT GLOBAL EVENTS ANALYSIS
======================================================================
Spatial resolution: 0.1° (≈11.1 km)

Loading GDELT events...
Loaded 287,453 events

EVENT TYPE DISTRIBUTION
======================================================================
  Verbal Cooperation         89,234 ( 31.0%)
  Material Cooperation       45,127 ( 15.7%)
  Verbal Conflict            98,456 ( 34.3%)
  Material Conflict          54,636 ( 19.0%)

TOP 20 EVENT HOTSPOTS
======================================================================
Rank     Lat       Lon   Actual  Estimated  Avg Impact
----------------------------------------------------------------------
1      38.91   -77.04    2,847      2,845       -1.23
       (Washington DC - Political events)
2      50.45    30.52    1,934      1,932       -4.56
       (Kyiv - Conflict zone)
3      31.50    34.47    1,523      1,521       -6.78
       (Gaza - Material conflict)
...

Memory Efficiency:
  Uncompressed (dict): 4.38 MB
  CountingGloBiMap: 3.75 MB
  Compression ratio: 1.2x (with error detection on)

CONFLICT VS COOPERATION SPATIAL ANALYSIS
======================================================================
Region               Conflict   Cooperation      Ratio
----------------------------------------------------------------------
Washington DC           1,234         1,987       0.62
Moscow                    876           542       1.62
Kyiv                    1,934           234       8.27
Gaza                    1,523            89      17.11
Brussels                  654         1,123       0.58
```

---

### example_covid19_analysis.py

**Purpose**: Analyze COVID-19 case spatial patterns

**Dataset**: Johns Hopkins CSSE COVID-19 Daily Reports

**Download dataset first**:
```bash
cd ..
./download_datasets.sh covid19
```

**What it demonstrates**:
- Loading real geospatial CSV data
- Converting lat/lon to pixel coordinates
- Using log-scale for highly variable counts
- Memory efficiency for global-scale data
- Spatial hotspot analysis

**Run it**:
```bash
python example_covid19_analysis.py
```

**Expected output**:
- Loads COVID-19 case data from Johns Hopkins repository
- Shows memory compression ratio (typically 10-50x)
- Estimates case density in major cities (NYC, London, Milan, Wuhan, Mumbai)
- False positive rate analysis
- Optional visualization (requires matplotlib)

**Example output**:
```
Analyzing COVID-19 data from: 03-09-2023.csv
Spatial resolution: 0.1° (≈11.1 km)

Loading data...
Loaded 3,142 locations with confirmed cases
Filter memory usage: 3.75 MB
Total confirmed cases: 676,609,955

Estimated case density in major cities:
City                      Lat      Lon  Estimated
------------------------------------------------------------
New York City          40.7128  -74.0060          8
Los Angeles            34.0522 -118.2437          6
London                 51.5074   -0.1278          7
Milan                  45.4642    9.1900          9
Wuhan                  30.5928  114.3055         12
Mumbai                 19.0760   72.8777          8

Memory Efficiency:
  Uncompressed (naive): 50.27 MB
  CountingGloBiMap: 3.75 MB
  Compression ratio: 13.4x
```

---

### example_infrastructure_failures.py

**Purpose**: Analyze infrastructure failure hotspots

**Dataset**: NYC 311 Service Requests (infrastructure-related)

**Download dataset first**:
```bash
cd ..
./download_datasets.sh infrastructure
```

**What it demonstrates**:
- Filtering specific complaint types
- High-resolution spatial analysis (~111m pixels)
- Hotspot identification
- Per-category analysis

**Run it**:
```bash
python example_infrastructure_failures.py
```

**Expected output**:
- Loads NYC 311 infrastructure failure reports
- Filters for relevant complaint types (water, sewer, street lights, etc.)
- Shows top 10 failure hotspots with coordinates
- Breakdown by complaint type
- Memory comparison vs naive storage

**Example output**:
```
NYC INFRASTRUCTURE FAILURE ANALYSIS
======================================================================
Spatial resolution: 0.001° (≈111m)

Loading 311 service requests...
Loaded 247,891 infrastructure failure reports

COMPLAINT TYPE BREAKDOWN
======================================================================
Street Light Condition                    89,234 ( 36.0%)
Street Condition                          64,127 ( 25.9%)
Water System                              42,819 ( 17.3%)
Sewer                                     28,456 ( 11.5%)
...

Top 10 failure hotspots:
Rank      X      Y   Actual  Estimated    Error
----------------------------------------------------------------------
1      73219  40731      342        338        4
       Location: (40.7310, -73.2190)
2      73985  40668      298        295        3
       Location: (40.6680, -73.9850)
...

Memory Efficiency:
  Uncompressed data: 3870.2 KB
  CountingGloBiMap: 960.0 KB
  Compression ratio: 4.0x
```

---

### example_compression_benchmark.py

**Purpose**: Benchmark memory compression for varying sparsity levels

**Dataset**: Synthetic data with configurable sparsity

**What it demonstrates**:
- Generating sparse data with hotspots
- Comparing multiple approaches:
  - Naive dictionary storage
  - Single-layer CountingGloBiMap
  - Multi-layer CountingGloBiMap
- Measuring memory usage, build time, and accuracy
- Testing with different sparsity levels

**Run it**:
```bash
python example_compression_benchmark.py
```

**Expected output**:
- Runs 3 benchmarks with varying parameters:
  1. Small world, high density (100K points in 1000×1000)
  2. Medium world, medium density (500K points in 10000×10000)
  3. Large world, low density (1M points in 100000×100000)
- For each benchmark:
  - Memory usage comparison
  - Build time comparison
  - Accuracy test on 100 random queries

**Example output**:
```
BENCHMARK: 1,000,000 points in 100,000x100,000 world
Hotspot ratio: 80% clustered, 20% random
======================================================================
Total points: 1,000,000
Unique pixels: 87,342
Sparsity: 0.000874%

Running compression methods...

RESULTS
======================================================================
Method                         Memory    Ratio  Build Time
--------------------------------------------------------------------------------
Naive Dictionary                 8.34 MB   1.0x      127.3 ms
Single Layer (8-bit, k=3)        1.05 MB   7.9x       89.2 ms
Multi-layer (k=3)                0.42 MB  19.9x      102.1 ms

ACCURACY TEST (100 random queries)
======================================================================
Method                         Mean Error  Max Error      RMSE
--------------------------------------------------------------------------------
Single Layer (8-bit, k=3)            0.23          2      0.45
Multi-layer (k=3)                    0.18          1      0.31
```

---

## Running All Examples

You can run all examples in sequence:

```bash
# Basic examples (no datasets needed)
python example_counting_basic.py
python example_multi_layer.py
python example_error_detection.py
python example_cardinality.py
python example_compression_benchmark.py

# Real-world examples (require datasets)
cd ..
./download_datasets.sh covid19 infrastructure
cd examples

python example_covid19_analysis.py
python example_infrastructure_failures.py
```

## Requirements

All examples require the `counting_globimap` Python module:

```bash
# From the project root
python setup.py build_ext --inplace
```

Real-world examples also require:
- **numpy** (coordinate conversion, statistics)
- **matplotlib** (optional, for visualizations)

Install requirements:
```bash
pip install -r ../requirements.txt
```

## Troubleshooting

**Module not found error**:
```
ImportError: No module named 'counting_globimap'
```

Solution: Build the module first:
```bash
cd ..
python setup.py build_ext --inplace
```

**Dataset not found error**:
```
COVID-19 dataset not found at: ../datasets/covid19
```

Solution: Download datasets:
```bash
cd ..
./download_datasets.sh covid19
```

**Visualization error**:
```
ImportError: No module named 'matplotlib'
```

Solution: Install matplotlib or skip visualizations:
```bash
pip install matplotlib
```

## Next Steps

- Modify examples to work with your own datasets
- Experiment with different filter configurations
- Try the interactive Jupyter notebook: `../notebooks/RealWorld_Dataset_Analysis.ipynb`
- Read the full API documentation in `../README.md`
