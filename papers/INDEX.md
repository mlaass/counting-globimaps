# Paper Summaries Index

This directory contains research papers related to probabilistic data structures, counting bloom filters, and spatial indexing. Each paper has been summarized with key findings and relevance to the CountingGloBiMap project.

## Counting Bloom Filters & Variants

| Paper | Authors | Year | Summary |
|-------|---------|------|---------|
| **Count-Min Sketch** | Cormode & Muthukrishnan | 2005 | [summary_count_min_sketch.md](summary_count_min_sketch.md) |
| **Spectral Bloom Filters** | Cohen & Matias | 2003 | [summary_spectral_bloom_filter.md](summary_spectral_bloom_filter.md) |
| **Variable-Increment CBF** | Rottenstreich et al. | 2012 | [summary_variable_increment_cbf.md](summary_variable_increment_cbf.md) |
| **MultiLayer Compressed CBF** | Ficara et al. | 2008 | [summary_multilayer_cbf.md](summary_multilayer_cbf.md) |

## d-Left Hashing

| Paper | Authors | Year | Summary |
|-------|---------|------|---------|
| **d-Left Counting Bloom Filter** | Bonomi et al. | 2006 | [summary_dleft_hashing.md](summary_dleft_hashing.md) |
| **Dynamic Bit Reassignment** | Bonomi et al. | 2006 | [summary_aller2006_dynamic_dleft.md](summary_aller2006_dynamic_dleft.md) |

## Hash Function Optimization

| Paper | Authors | Year | Summary |
|-------|---------|------|---------|
| **Less Hashing, Same Performance** | Kirsch & Mitzenmacher | 2008 | [summary_rsa2008_less_hashing.md](summary_rsa2008_less_hashing.md) |

## Cache & Performance Optimization

| Paper | Authors | Year | Summary |
|-------|---------|------|---------|
| **Cache-Efficient Bloom Filters** | Putze, Sanders, Singler | 2009 | [summary_cache_efficient_bf.md](summary_cache_efficient_bf.md) |
| **Performance-Optimal Filtering** | Lang et al. | 2019 | [summary_performance_optimal_filtering.md](summary_performance_optimal_filtering.md) |

## Spatial Indexing

| Paper | Authors | Year | Summary |
|-------|---------|------|---------|
| **3D Hilbert Curves** (3 papers) | Various | 2014-2022 | [summary_3d_hilbert_curves.md](summary_3d_hilbert_curves.md) |
| **3D Hilbert Encoding/Decoding** | Walker | 2018 | [3d_hilbert/summary_2308.05673v2.md](3d_hilbert/summary_2308.05673v2.md) |
| **Dynamic Volume Lines** | Wang et al. | 2018 | [3d_hilbert/summary_tvcg_2018.md](3d_hilbert/summary_tvcg_2018.md) |
| **3D Hilbert for LIDAR** | Wang & Shan | 2005 | [3d_hilbert/summary_wangj.md](3d_hilbert/summary_wangj.md) |
| **Data-Driven Space-Filling Curves** | Tao et al. | 2020 | [3d_hilbert/summary_tvcg_2020.md](3d_hilbert/summary_tvcg_2020.md) |
| **SIMD Hilbert Curve Transposition** | Alves, Russo, Francisco | 2022 | [sfc/summary_simd_hilbert_transposition.md](sfc/summary_simd_hilbert_transposition.md) |
| **Cache-Friendly Morton Layouts** | Makarychev et al. | 2024 | [sfc/summary_icpe24.md](sfc/summary_icpe24.md) |

## SIMD Optimization

| Paper | Authors | Year | Summary |
|-------|---------|------|---------|
| **Ultra-Fast Bloom Filters (UFBF)** | Huang et al. | 2019 | [summary_simd_iwqos.md](summary_simd_iwqos.md) |

---

## Quick Reference by Topic

### Frequency Estimation
- [Count-Min Sketch](summary_count_min_sketch.md) - Provable error bounds (ε, δ)
- [Spectral Bloom Filter (MI)](summary_spectral_bloom_filter.md) - Conservative updates, best accuracy
- [MultiLayer CBF](summary_multilayer_cbf.md) - Hierarchical cascade design

### Memory Efficiency
- [Variable-Increment CBF](summary_variable_increment_cbf.md) - 22-34% memory savings
- [d-Left CBF](summary_dleft_hashing.md) - 50% space reduction
- [Dynamic d-Left](summary_aller2006_dynamic_dleft.md) - 3x better FPR via dynamic bit reassignment

### Performance Optimization
- [Less Hashing](summary_rsa2008_less_hashing.md) - Double hashing: O(k) → O(1)
- [Cache-Efficient BF](summary_cache_efficient_bf.md) - Blocked/SIMD variants, 4x speedup
- [Performance-Optimal](summary_performance_optimal_filtering.md) - Bloom vs Cuckoo tradeoffs

### Spatial Applications
- [3D Hilbert Curves](summary_3d_hilbert_curves.md) - Locality-preserving spatial indexing
- [3D Hilbert Encoding](3d_hilbert/summary_2308.05673v2.md) - Transformation tables, pseudocode
- [Dynamic Volume Lines](3d_hilbert/summary_tvcg_2018.md) - Nonlinear scaling for 3D volume comparison
- [3D Hilbert LIDAR](3d_hilbert/summary_wangj.md) - Point cloud indexing with R-tree
- [Data-Driven SFCs](3d_hilbert/summary_tvcg_2020.md) - Adaptive curves via evolutionary algorithms
- [SIMD Hilbert Transposition](sfc/summary_simd_hilbert_transposition.md) - Cache-oblivious SIMD curve generation, 6.90× faster
- [Cache-Friendly Morton](sfc/summary_icpe24.md) - Evolutionary algorithms for optimal layouts, 10× speedup

### SIMD Performance
- [Ultra-Fast Bloom Filters](summary_simd_iwqos.md) - AVX2/PDEP optimization, 2-4× faster

---

## Relevance to Project Implementations

| Implementation | Key Papers |
|----------------|------------|
| `counting_globimap.hpp` | [MultiLayer CBF](summary_multilayer_cbf.md), [Less Hashing](summary_rsa2008_less_hashing.md) |
| `spectral_bloom_filter.hpp` | [Spectral BF](summary_spectral_bloom_filter.md) |
| `count_min_sketch.hpp` | [Count-Min Sketch](summary_count_min_sketch.md), [Less Hashing](summary_rsa2008_less_hashing.md) |
| `variable_increment_bf.hpp` | [Variable-Increment CBF](summary_variable_increment_cbf.md) |
| `dleft_counting_bf.hpp` | [d-Left CBF](summary_dleft_hashing.md), [Dynamic d-Left](summary_aller2006_dynamic_dleft.md) |
| `blocked_bloom_filter.hpp` | [Cache-Efficient BF](summary_cache_efficient_bf.md) |
| `simd_bloom_filter.hpp` | [Cache-Efficient BF](summary_cache_efficient_bf.md), [Performance-Optimal](summary_performance_optimal_filtering.md), [SIMD Hilbert](sfc/summary_simd_hilbert_transposition.md), [UFBF](summary_simd_iwqos.md) |

---

## Original PDFs

### Main Folder
- `4.Count_min_sketchl.pdf` - Count-Min Sketch
- `sbf-sigmod-03.pdf` - Spectral Bloom Filters
- `infocom12_variable.pdf` - Variable-Increment CBF
- `MultiLayer Compressed Counting Bloom Filters.pdf` - ML-HCBF/ML-CCBF
- `esa2006b.pdf` - d-Left Counting Bloom Filter
- `aller2006.pdf` - Dynamic Bit Reassignment
- `rsa2008.pdf` - Less Hashing, Same Performance
- `1498698.1594230.pdf` - Cache-Efficient Bloom Filters
- `p502-lang.pdf` - Performance-Optimal Filtering

### 3d_hilbert/
- `136620717.pdf` - HilbertNet (ECCV 2022)
- `Chinese J of Electronics - 2022 - JIA - Efficient 3D Hilbert Curve...pdf` - JFK-3HE/JFK-3HD
- `ij3dim.2014040101.pdf` - 3D Hilbert for CityGML
- `2308.05673v2.pdf` - 3D Hilbert Encoding/Decoding (Walker 2018)
- `TVCG.2018.2864510.pdf` - Dynamic Volume Lines
- `WangJ.pdf` - 3D Hilbert for LIDAR Point Clouds
- `tvcg.2020.3030473.pdf` - Data-Driven Space-Filling Curves

### Main Folder (New)
- `SIMD-IWQoS.pdf` - Ultra-Fast Bloom Filters using SIMD Techniques

### sfc/
- `3555353.pdf` - Cache-oblivious Hilbert Curve Transposition (ACM TOMS 2022)
- `ICPE24.pdf` - Cache-Friendly Generalized Morton Layouts (ICPE 2024)
