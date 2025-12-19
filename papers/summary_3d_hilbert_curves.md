# 3D Hilbert Curve Research - Combined Summary

## Papers Covered

### Paper 1: Efficient Point Cloud Analysis Using Hilbert Curve
**Authors:** Wanli Chen, Xinge Zhu, Guojin Chen, Bei Yu
**Venue/Year:** ECCV 2022 (European Conference on Computer Vision)
**Affiliation:** The Chinese University of Hong Kong
**PDF:** 136620717.pdf

### Paper 2: Efficient 3D Hilbert Curve Encoding and Decoding Algorithms
**Authors:** JIA Lianyin, LIANG Binbin, LI Mengjuan, LIU Yong, CHEN Yinong, DING Jiaman
**Venue/Year:** Chinese Journal of Electronics, Vol.31, No.2, March 2022
**Affiliations:** Kunming University of Science and Technology (China), Arizona State University (USA)
**PDF:** Chinese J of Electronics - 2022 - JIA - Efficient 3D Hilbert Curve Encoding and Decoding Algorithms.pdf

### Paper 3: 3D Hilbert Space Filling Curves in 3D City Modeling for Faster Spatial Queries
**Authors:** Uznir Ujang, Francois Anton, Suhaibah Azri, Alias Abdul Rahman, Darka Mioc
**Venue/Year:** International Journal of 3-D Information Modeling, 3(2), April-June 2014
**Affiliations:** Universiti Teknologi Malaysia, Denmark Technical University
**PDF:** ij3dim.2014040101.pdf

## Overview

### What are 3D Hilbert Curves?

3D Hilbert curves are space-filling curves that provide a continuous, one-to-one mapping between multidimensional 3D space and 1D space. Discovered by David Hilbert in 1891, Hilbert curves have exceptional **locality-preserving properties** - nearby points in 3D space remain nearby when mapped to 1D sequence positions.

### Why They Matter for Spatial Data

3D Hilbert curves are critical for:

1. **Spatial Data Management** - Organizing 3D spatial data (point clouds, city models, LiDAR) for efficient storage and retrieval
2. **Dimensionality Reduction** - Mapping 3D voxels to 2D representations while preserving spatial topology
3. **Query Optimization** - Accelerating nearest neighbor searches, range queries, and spatial indexing
4. **Computational Efficiency** - Reducing 3D convolution overhead by enabling 2D processing with preserved locality
5. **Database Indexing** - Clustering multidimensional data for fast proximity searches

The key advantage over alternatives (Z-order curves, reshape functions) is **superior spatial cohesion and continuity** - Hilbert curves minimize "jump connections" where consecutive points are spatially distant.

## Key Contributions

### Paper 1: HilbertNet for Point Cloud Processing

**Main Contribution:** First deep learning framework (HilbertNet) that uses 3D Hilbert curves to flatten 3D voxels into 2D for efficient point cloud analysis.

**Key Innovations:**
- **Voxelization + Hilbert Flattening Module (VHFM):** Divides 3D space into R×R×R voxels, then flattens each Z-axis slice using 2D Hilbert curves
- **Hilbert Interpolation:** Adaptive feature gathering that overcomes sparsity issues
- **Hilbert Pooling:** 3D-aware pooling that converts 2D features back to 3D
- **Hilbert Attention:** Lightweight transformer with intra-slice and inter-slice correlation

**Mathematical Foundation:**
- Proved Hilbert curve has **Space-to-Linear Ratio (SLR) = 6**, versus reshape function's SLR growing with order
- Demonstrated Hilbert has **zero Jump segments** and higher Still segments (66.7% vs reshape's 62.5%)

**Results:**
- **ModelNet40 Classification:** 94.1% accuracy (SOTA)
- **ShapeNetPart Segmentation:** 87.1% mIoU (SOTA)
- **Efficiency:** 3.7x lower FLOPs than 3D convolution

### Paper 2: JFK-3HE and JFK-3HD Algorithms

**Main Contribution:** Efficient encoding (JFK-3HE) and decoding (JFK-3HD) algorithms that skip leading zeros in coordinates/Hilbert codes, optimized for skewed data distributions.

**Key Innovations:**
- **State View Design:** Two compact 4D lookup tables (PHM, PNM) for encoding and two 2D tables (HPM, HNM) for decoding
  - Total space overhead: **192 bytes** for both tables
- **First-1-Check (F1C) Algorithm:** Binary-search-like method to detect first non-zero bit

**Complexity:**
- **Time:** O(d) where d = log₂(max(X, Y, Z)) - actual depth, not maximum order
- **Space:** 192 bytes (constant)

**Results:**
- **Skewed Distribution (θ=8, φ=100%):**
  - Encoding: 27% faster than best competitor
  - Decoding: 50% faster
- **Extreme Skew (θ=20, φ=50%):**
  - Decoding: 65% faster (0.09s vs 0.26s for 1M codes)

### Paper 3: CityGML Spatial Query Optimization

**Main Contribution:** First application of 3D Hilbert curves to CityGML city models, demonstrating query performance improvements for building data retrieval.

**Key Innovations:**
- **3D City Model Indexing:** Extended 2D Hilbert curves to 3D for organizing building objects
- **Arc-Length Addressing:** Each 3D building assigned position on 1D Hilbert curve
- **SFC Cell Clustering:** Spatial objects grouped by Hilbert curve cells for windowing and range queries

**Results (1,000 building dataset):**

| Query Type | CityGML Time | 3D Hilbert Time | Speedup |
|------------|--------------|-----------------|---------|
| Nearest Neighbor | 32ms | 2-3ms | **90-95% faster** |
| Range Query (single cell) | 16.0ms | 9.3ms | **58% faster** |
| Range Query (8 cells) | 16.7ms | 11.3ms | **32% faster** |

## Technical Details

### Hilbert Curve Generation

**3D Hilbert Curve:**
- Extends to 2ⁿ × 2ⁿ × 2ⁿ space (8 subcubes per division)
- **12 basic states** (rotations/reflections) for order-1 curve
- State transitions encoded in lookup tables
- 1,536 possible 3D variants

### Encoding Algorithm (JFK-3HE)

```
Input: 3D coordinate (X, Y, Z), order m
Output: Hilbert code A

1. A = 0
2. p = F1C(max(X, Y, Z))  // Find first 1-bit position
3. k = m - p - 1           // Orders to skip
4. T_{k+1} = (k mod 2) << 3  // Initial state: 0 or 8
5. for i = k+1 to m:
     A = A << 3 | PHM[T_i][X_i][Y_i][Z_i]  // 3-bit Hilbert code
     T_{i+1} = PNM[T_i][X_i][Y_i][Z_i]      // Next state
6. return A
```

### Locality Analysis

**Segment Definitions:**
- **Jump Segment:** Consecutive points Pi, Pi+1 where |Pi+1 - Pi| > 1
- **Still Segment:** Consecutive points where dimension k unchanged

**Results (D=3, N=2⁶=64):**
- Hilbert: 66.7% still, 0% jump
- Reshape: 59% still, 8% jump

## Relevance to Project

### Direct Applications to GloBiMap and Spatial Bloom Filters

**1. Hierarchical Spatial Indexing**
- **Current Challenge:** CountingGloBiMap uses multi-layer cascading but lacks explicit spatial ordering
- **3D Hilbert Solution:** Map (lat, lon, category) coordinates to 1D Hilbert codes before hashing
  - Preserves spatial locality: nearby locations → nearby Hilbert codes → same hash buckets
  - Reduces false positives in range queries by clustering spatially proximate points

**Implementation Example:**
```cpp
// Current: hash(lat, lon, category) with MurmurHash
std::vector<uint64_t> point = {lat, lon, category};
filter.put(point);  // Direct hashing

// Enhanced with 3D Hilbert:
uint64_t hilbert_code = encode_3d_hilbert(lat, lon, category, order);
std::vector<uint64_t> point = {hilbert_code >> 16, hilbert_code & 0xFFFF};
filter.put(point);  // Hash Hilbert code instead
```

**2. Dimensionality Reduction for High-D Bloom Filters**
- **Current Challenge:** Multi-category CountingGloBiMap with (x, y, t, cat1, cat2, ...) vectors increases hash collisions
- **3D Hilbert Solution:** Reduce dimensions before hashing
  - Temporal + Spatial: (x, y, time) → Hilbert code → single dimension
  - Hierarchical Categories: (region_code, sub_region, poi_type) → Morton/Hilbert code

**3. Fast Nearest Neighbor Queries**
- **Current Limitation:** GloBiMap provides membership/cardinality but no spatial proximity
- **3D Hilbert Extension:** Store Hilbert codes in B-tree/sorted array
  - Query k-nearest neighbors via binary search on arc-length
  - Filter candidates using actual Euclidean distance

**4. Optimized Data Serialization**
- **3D Hilbert Enhancement:** Serialize in Hilbert order for better compression
  - Spatial autocorrelation → consecutive counters have similar values
  - Delta encoding (store differences) reduces entropy

**5. Range Query Optimization**
- **3D Hilbert + Cell Partitioning:**
  - Divide space into Hilbert cells
  - Range query Q → identify intersecting cells → probe only those buckets
  - Expected speedup: 58% (Paper 3) to 93%

### Algorithm Candidates for Integration

**JFK-3HE/JFK-3HD (Paper 2):**
- **Where:** Preprocessing step in `datasets/utils/csv_to_hdf5.py`
- **Purpose:** Convert (lat, lon, altitude) to Hilbert codes before HDF5 storage
- **Benefit:** HDF5 chunk reads align with spatial locality

**State-View Tables (Paper 2):**
- **Where:** `include/hilbert_encoding.hpp` (new utility header)
- **Purpose:** 192-byte lookup tables for fast 3D Hilbert encoding/decoding
- **Integration:** Called in `put()` and `get_min()` methods

### Performance Summary

| Application | Method | Dataset | Key Metric | Improvement |
|-------------|--------|---------|------------|-------------|
| Point Cloud Classification | HilbertNet | ModelNet40 | 94.1% accuracy | +0.3% vs SOTA |
| 3D Encoding (skewed) | JFK-3HE | 1M coords | 0.24s vs 0.33s | 27% faster |
| 3D Decoding (skewed) | JFK-3HD | 1M codes | 0.09s vs 0.26s | 65% faster |
| City Nearest Neighbor | 3D Hilbert | 1,000 buildings | 2ms vs 32ms | 93% faster |
| City Range Query | 3D Hilbert | 1,000 buildings | 9.3ms vs 16.0ms | 58% faster |

### Key Findings

1. **Locality Preservation is Critical** - Hilbert curves maintain spatial coherence better than alternatives
2. **Dimensionality Reduction Without Information Loss** - 3D → 2D flattening preserves topology
3. **Skipped Computation for Sparse Data** - JFK algorithms skip k orders where leading bits are 0
4. **Spatial Query Optimization** - Arc-length indexing enables O(log n) nearest neighbor search

### Recommended Next Steps

1. **Benchmark 3D Hilbert Preprocessing:** Encode GDELT (lat, lon) as 2D Hilbert codes and measure query time reduction

2. **Implement Hilbert-Based Spatial Index:** Create `include/hilbert_spatial_index.hpp` with JFK-3HE/JFK-3HD

3. **Extend Multi-Category Support:** Use (Hilbert(x, y), category) to reduce dimension from 3 to 2

4. **Compare Locality Metrics:** Test Hilbert vs Z-order vs random ordering on COVID-19 dataset
