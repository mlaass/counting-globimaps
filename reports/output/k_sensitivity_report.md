# K Parameter Sensitivity Analysis Report

**Status**: No results available yet

## Description

This report analyzes how the number of hash functions (k) affects:
- **Accuracy**: False positive rate and estimation error
- **Performance**: Insert and query time
- **Memory efficiency**: Trade-offs at different k values

The analysis sweeps k from 1 to 32 across multiple implementations to identify
optimal values for different use cases.

## How to Generate Results

```bash
# From project root
./run_all_experiments.sh

# Or run specific experiment from build/
./build/globimap_test_k_compare

# Alternative: test with specific datasets
./build/globimap_test_datasets_for_k
```

Results will be saved to `results/k_sensitivity/`.

---

*Report generated: 2025-12-12 01:31:52*
