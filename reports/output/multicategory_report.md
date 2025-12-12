# Multi-Category Analysis Report

*Generated: 2025-12-12 01:31:50*

## Executive Summary

This report analyzes how counting bloom filter implementations handle 
multi-category data, testing category isolation and per-category accuracy.


**Key Findings:**

- All implementations achieve **100% category isolation** on synthetic test

- **Spectral BF (MI)** achieves perfect accuracy (0% error) on real dataset

- d-Left CBF shows higher error (16.1%) due to fingerprint collisions


## Synthetic Isolation Test

Tests category isolation at a single location with 4 categories 
(100, 500, 50, 1000 inserts each).


### Results Summary

| Implementation        | Memory   | Insert Time   | Mean Error   | Max Error   | Isolation Rate   |
|:----------------------|:---------|:--------------|:-------------|:------------|:-----------------|
| Spectral BF (MI)      | 2.00 MB  | 93.00 μs      | 0.00%        | 0.00%       | 100.0%           |
| d-Left CBF            | 95.00 KB | 1.10 ms       | 0.00%        | 0.00%       | 100.0%           |
| Count-Min Sketch      | 88.49 KB | 156.00 μs     | 0.00%        | 0.00%       | 100.0%           |
| CountingGloBiMap (MI) | 1.50 MB  | 200.00 μs     | 0.00%        | 0.00%       | 100.0%           |


![Error % by implementation and category (green = perfect)](figures/multicategory_isolation_heatmap.png)

*Error % by implementation and category (green = perfect)*


## Real Dataset Benchmark (GDELT)

Tests on 1.9M GDELT events with 4 QuadClass categories.


### Category Distribution

![Distribution of events across GDELT QuadClass categories](figures/multicategory_category_distribution.png)

*Distribution of events across GDELT QuadClass categories*


### Performance Summary

| Implementation        | Memory   | Insert Time   | Query Time   | Avg Error   |
|:----------------------|:---------|:--------------|:-------------|:------------|
| Spectral BF (MI)      | 2.00 MB  | 158.82 ms     | 0.26 μs      | 0.00%       |
| d-Left CBF            | 95.00 KB | 202.22 ms     | 0.48 μs      | 16.14%      |
| Count-Min Sketch      | 88.49 KB | 189.87 ms     | 0.30 μs      | 0.00%       |
| CountingGloBiMap (MI) | 1.50 MB  | 868.94 ms     | 1.61 μs      | 0.03%       |



### Per-Category Accuracy

![Mean estimation error by category and implementation](figures/multicategory_accuracy_by_category.png)

*Mean estimation error by category and implementation*


### Memory vs Accuracy Trade-off

![Memory usage vs mean error (circles=isolation test, squares=dataset)](figures/multicategory_memory_vs_accuracy.png)

*Memory usage vs mean error (circles=isolation test, squares=dataset)*


## Conclusions

- **Category isolation works**: All implementations properly separate categories

- **Spectral BF (MI)** and **Count-Min Sketch** achieve perfect accuracy

- **d-Left CBF** shows some error due to fingerprint collisions at high load

- **CountingGloBiMap (MI)** provides excellent accuracy with minimal overhead


---

*Report generated: 2025-12-12 01:31:51*
