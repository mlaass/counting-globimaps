# An Improved Construction for Counting Bloom Filters

**Authors:** Flavio Bonomi (Cisco Systems), Michael Mitzenmacher (Harvard University), Rina Panigrahy (Stanford University), Sushil Singh (Cisco Systems), George Varghese (Cisco Systems)

**Venue/Year:** ESA 2006 (European Symposium on Algorithms), LNCS 4168, pp. 684-695

**PDF:** esa2006b.pdf

## Abstract/Overview

This paper introduces the d-left Counting Bloom Filter (dlCBF), a space-efficient alternative to standard counting Bloom filters (CBF). A counting Bloom filter extends the traditional Bloom filter to support dynamic sets with both insertions and deletions by using counters instead of single bits. The dlCBF achieves the same functionality as standard CBFs but uses approximately half the space (or better) while maintaining comparable simplicity and allowing the same false positive guarantees.

## Key Contributions

- **Space Efficiency**: dlCBF uses ~50% or less space compared to standard CBFs for the same false positive rate
- **Simple Construction**: Maintains the simplicity of original Bloom filter designs, making it practical for hardware implementations
- **Novel Two-Phase Hashing**: Solves the deletion ambiguity problem in d-left hashing through a clever two-phase hash construction
- **Theoretical Analysis**: Provides fluid limit analysis for performance characterization under insertions and deletions
- **Experimental Validation**: Comprehensive simulations demonstrating 2.5x+ space savings with matching performance

## Algorithm/Data Structure Details

### Core Structure

The dlCBF uses a **d-left hash table** divided into d subtables (typically d=4):
- Each subtable has B buckets
- Each bucket contains multiple cells (typically 8 cells)
- Each cell stores: **fingerprint remainder + small counter** (e.g., 14 bits + 2 bits = 16 bits/cell)

### Two-Phase Hashing Scheme

**Phase 1 - True Fingerprint:**
```
H: U → [B] × [R]
```
Computes the "true fingerprint" fx = (bucket_index, remainder) for element x

**Phase 2 - Permutations for d Choices:**
```
P₁(fx) = (b₁, r₁)
P₂(fx) = (b₂, r₂)
...
Pₐ(fx) = (bₐ, rₐ)
```
Uses d pseudo-random permutations to generate bucket/remainder pairs for each subtable

**Why Two Phases?**
- Naive d-left hashing creates deletion ambiguity: the same remainder might appear in multiple buckets from different elements
- Two-phase approach ensures only ONE remainder per element exists in the table at any time
- Permutations are invertible, enabling element movement if needed

### Insertion Algorithm

1. Compute true fingerprint: fx = H(x)
2. Compute d choices: Pi(fx) for i = 1..d
3. Check if any remainder ri already exists in bucket bi
   - If YES: increment counter (hash collision)
   - If NO: place remainder in the least loaded bucket (leftmost on ties)

### Deletion Algorithm

1. Compute true fingerprint: fx = H(x)
2. Compute d choices: Pi(fx) for i = 1..d
3. Find the unique bucket containing remainder
4. Decrement counter or remove cell

### Query Algorithm

Element y is considered present if any of its d buckets contains matching remainder ri

### Practical Implementation

**Recommended permutations:** Simple linear functions
```
Pi(H(x)) = a · H(x) mod 2^q
```
where a is chosen uniformly at random from odd numbers in [2^q]

**Fingerprint extraction:**
- High-order bits → bucket index
- Low-order bits → remainder (stored in cell)

## Key Findings/Results

### Theoretical Bounds

**False Positive Rate:**
```
FPR ≤ 24 · 2^(-r)
```
where r is the number of remainder bits

**Space Usage:**
```
4m(r + 2)/3 bits total
= (4 log₂(1/f) + 20 + 4 log₂ 3)/3 bits per element
```
where f is target false positive rate

**Comparison to Standard CBF:**
- Standard CBF: c counters × 4 bits × m elements = 4cm bits
- dlCBF: ~50% of standard CBF space for same FPR
- At r=14 and c=16/3, dlCBF achieves **100x better** FPR at same size

### Experimental Results

**Configuration tested:**
- 4 subtables × 2048 buckets × 8 cells = 2^16 capacity
- Load: 49,152 elements (avg 6 per bucket)
- 14-bit fingerprints + 2-bit counters = 16 bits/cell
- Total size: 2^20 bits (1 MB)

**Performance:**
- False positive rate: 0.001463 (matches theory: 24·2^(-14) ≈ 0.001465)
- Bucket overflow: 0 in 10,000 trials over 2^20 insert/delete operations
- Max counter value: 4 (fits in 2 bits with sentinel)
- **2.5x smaller** than standard CBF (220 KB vs 2.65 MB) for same FPR

**Fluid Limit Accuracy:**
- Differential equations extremely accurate for predicting bucket load distribution
- Works even with simple random linear permutations (not fully independent hashes)

### Load Distribution (6 elements/bucket avg, d=4)

| Load ≥ k | Simulation | Fluid Limit |
|----------|------------|-------------|
| ≥ 1      | 1.0000     | 1.0000      |
| ≥ 5      | 0.9502     | 0.9505      |
| ≥ 6      | 0.7655     | 0.7669      |
| ≥ 7      | 0.2868     | 0.2894      |
| ≥ 8      | 0.0022     | 0.0023      |
| ≥ 9      | 0.0000     | 1.681e-27   |

## Relevance to Project

### Direct Application to CountingGloBiMap

The d-left CBF is **already implemented** in this project as `include/dleft_counting_bf.hpp` and is one of 10 bloom filter variants being compared.

**Key advantages for spatial cardinality estimation:**

1. **Space Efficiency**: Critical for large-scale spatial datasets (GDELT 1.9M events, COVID-19 1.8M cases)
2. **Deterministic Lookups**: d buckets to check (d=4 typical) vs k hash positions in standard CBF
3. **Cache-Friendly**: Fingerprints clustered in buckets improve cache locality
4. **Deletion Support**: Essential for dynamic spatial datasets where events expire
5. **Simple Implementation**: Maintains hardware-friendliness like original Bloom filters

### Multi-Category Support

The dlCBF naturally extends to multi-category filtering through variable-length point vectors:
```cpp
filter.put({lat, lon, category});
uint64_t count = filter.get_min({lat, lon, category});
```

**GDELT Multi-Category Results** (4 categories: Verbal/Material Cooperation/Conflict):
- Memory: 95 KB (smallest of all implementations)
- Insert time: 0.32s for 1.9M events
- Query time: 1.37 μs
- Category isolation: Good (14-21% error due to false positives, not cross-contamination)

### Comparison to Other Implementations

| Implementation | Memory | Accuracy | Use Case |
|----------------|--------|----------|----------|
| **d-Left CBF** | 95 KB | Good | Cache-sensitive, deletions needed |
| Spectral BF (MI) | 2 MB | Perfect | Highest accuracy |
| Count-Min Sketch | 88 KB | Perfect | Error bounds needed |
| CountingGloBiMap | 1.5 MB | Perfect | Varying magnitudes |

### Research Applications

1. **Benchmark baseline**: dlCBF provides strong comparison point for hierarchical structures
2. **Hardware deployment**: Simplicity makes it ideal for router/FPGA implementations
3. **Hybrid approaches**: Could combine dlCBF with hierarchical layers for best of both worlds
4. **Movement optimization**: Section 3.3 discusses moving elements to prevent overflow (unexplored in current implementation)

### Future Work Opportunities

- Implement element movement for emergency overflow handling
- Dynamic fingerprint sizing based on load
- Combine d-left hashing with cascade layers for extreme skew
- Hardware acceleration (FPGA/ASIC) given simple design
