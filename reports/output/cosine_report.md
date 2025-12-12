# Cosine Distribution Analysis Report

**Status**: No results available yet

## Description

This report analyzes implementation performance on synthetic data
with a deterministic cosine distribution pattern.

The cosine distribution creates a known spatial pattern that allows precise
validation of cardinality estimation accuracy since ground truth is known exactly.

**Test Configuration:**
- 65,536 inserts (256x256 grid)
- Cosine-weighted distribution creating hotspots
- Exact ground truth for error calculation

## How to Generate Results

```bash
# From project root
./run_all_experiments.sh

# Or run specific experiment from build/
./build/globimap_test_cos_compare
```

Results will be saved to `results/cosine/`.

---

*Report generated: 2025-12-12 01:31:52*
