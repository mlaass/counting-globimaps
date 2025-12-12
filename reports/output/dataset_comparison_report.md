# Dataset Comparison Report

*Generated: 2025-12-12 01:31:48*

## Executive Summary

This report analyzes 4 counting bloom filter implementations 
across 2 real-world datasets.


**Key Findings:**

- **Most memory efficient**: Count-Min Sketch (88.49 KB average)

- **Fastest queries**: Spectral BF (MI) (317.29 μs average)

- **Fastest inserts**: Spectral BF (MI) (6.614 s average)


## Datasets Analyzed

- **Covid19 Cases**: 8192x8192 grid
- **Gdelt Events**: 8192x8192 grid


## Memory Usage

![Memory usage comparison across implementations](figures/dataset_comparison_memory.png)

*Memory usage comparison across implementations*


## Insert Performance

![Insert time comparison (lower is better)](figures/dataset_comparison_insert_time.png)

*Insert time comparison (lower is better)*


## Query Performance

![Query time with min/max error bars (lower is better)](figures/dataset_comparison_query_time.png)

*Query time with min/max error bars (lower is better)*


## Memory vs Speed Trade-off

![Memory vs query time (bottom-left is optimal)](figures/dataset_comparison_efficiency.png)

*Memory vs query time (bottom-left is optimal)*


## Detailed Results

|                                            | Memory    | Insert Time   | Query Mean   |
|:-------------------------------------------|:----------|:--------------|:-------------|
| ('Covid19 Cases', 'Spectral BF (MI)')      | 128.00 KB | 13.037 s      | 323.68 μs    |
| ('Covid19 Cases', 'd-Left CBF')            | 95.00 KB  | 17.028 s      | 877.20 μs    |
| ('Covid19 Cases', 'Count-Min Sketch')      | 88.49 KB  | 17.776 s      | 311.21 μs    |
| ('Covid19 Cases', 'CountingGloBiMap (MI)') | 96.00 KB  | 31.463 s      | 548.53 μs    |
| ('Gdelt Events', 'Spectral BF (MI)')       | 128.00 KB | 190.00 ms     | 310.90 μs    |
| ('Gdelt Events', 'd-Left CBF')             | 95.00 KB  | 207.00 ms     | 675.08 μs    |
| ('Gdelt Events', 'Count-Min Sketch')       | 88.49 KB  | 366.00 ms     | 443.67 μs    |
| ('Gdelt Events', 'CountingGloBiMap (MI)')  | 96.00 KB  | 653.00 ms     | 1.02 ms      |



## Recommendations

Based on the analysis:

- **For memory-constrained applications**: Count-Min Sketch offers the smallest footprint

- **For query-heavy workloads**: Spectral BF (MI) provides fastest queries

- **For balanced performance**: d-Left CBF offers good memory/speed trade-off

- **For accuracy with cascading**: CountingGloBiMap (MI) handles varying count magnitudes


---

*Report generated: 2025-12-12 01:31:49*
