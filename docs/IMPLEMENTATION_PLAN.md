# Implementation Plan: Alternative Counting Bloom Filter Strategies

**Date:** 2025-11-14
**Purpose:** Extend the counting-globimaps library with alternative counting bloom filter implementations to provide easier configuration options and different space/accuracy trade-offs while preserving the existing multilayer cascading implementation.

## Current State

The existing `CountingGloBiMap` implementation uses a multi-layer cascading architecture where counters overflow from lower bit-depth layers (1, 8, 16, 32, 64 bits) to higher capacity layers. While powerful and space-efficient for certain workloads, the configuration is complex, requiring users to specify the number of layers, bit depths, and sizes for each layer.

## Proposed Approach

Implement 3-4 new counting bloom filter classes as alternatives, each with different configuration complexity and use cases. All implementations will share a common interface where possible and coexist with the current `CountingGloBiMap`.

---

## Strategy 1: Variable-Increment Counting Bloom Filter (VI-CBF)

### Scientific Basis

**Primary Citation:**
> Ori Rottenstreich, Yossi Kanizo, and Isaac Keslassy. 2014. The variable-increment counting bloom filter. IEEE/ACM Trans. Netw. 22, 4 (August 2014), 1092–1105. https://doi.org/10.1109/TNET.2013.2272604

**Key Paper Claims:**
- 50% memory reduction compared to standard counting bloom filters
- Better false positive rate with same memory budget
- Theoretical analysis of overflow probability
- Variable increments drawn from set D_L = {L, L+1, ..., 2L-1} where L is power of 2

**Available at:** https://webee.technion.ac.il/~isaac/p/infocom12_variable.pdf

### Implementation Design

**Class Name:** `VariableIncrementBloomFilter`

**Configuration:**
```cpp
struct VICBFConfig {
    uint hash_k;              // Number of hash functions
    uint logsize;             // Filter size: 2^logsize counters
    uint counter_bits;        // Bits per counter (8, 16, 32, or 64)
    uint increment_L;         // Increment base (must be power of 2, typically 2-8)
};
```

**Core Algorithm:**
1. On insert: hash point to get base hash h1, h2
2. For each of k positions:
   - Compute position: `pos = (h1 + i*h2) & mask`
   - Compute increment: `inc = hash_increment(point, i, increment_L)` → value in [L, 2L-1]
   - Add inc to counter at position (check for overflow)
3. On query: return minimum counter value across k positions

**API:**
```cpp
class VariableIncrementBloomFilter {
public:
    VariableIncrementBloomFilter(const VICBFConfig &conf);
    void put(const std::vector<uint64_t> &point);
    uint64_t get_min(const std::vector<uint64_t> &point);
    bool get_bool(const std::vector<uint64_t> &point);
    std::string summary();
};
```

**Advantages:**
- Much simpler configuration: 4 parameters vs. multilayer's n*2+1 parameters
- Proven 50% memory reduction in literature
- Single layer = predictable memory usage
- No cascading logic needed

**Disadvantages:**
- Cannot support deletions (Rottenstreich et al., 2012, Section III.B)
- Counters grow faster (by L to 2L-1 instead of 1)
- Query interpretation differs from standard CBF
- Saturation at counter max still possible

**Recommended Default:** `increment_L = 4` (based on paper experiments)

---

## Strategy 2: Spectral Bloom Filter with Minimal Increment

### Scientific Basis

**Primary Citation:**
> Saar Cohen and Yossi Matias. 2003. Spectral bloom filters. In Proceedings of the 2003 ACM SIGMOD international conference on Management of data (SIGMOD '03). Association for Computing Machinery, New York, NY, USA, 241–252. https://doi.org/10.1145/872757.872787

**Key Paper Claims:**
- Extends bloom filters to multisets (frequency estimation)
- Three variants: Minimum Selection (MS), Minimal Increment (MI), Recurring Minimum (RM)
- MI provides "conservative update" - only increments minimum counter(s)
- RM variant supports deletions with secondary min-values filter
- Theoretical bounds on frequency estimation error

**Available at:** https://theory.stanford.edu/~matias/papers/sbf-sigmod-03.pdf

**Supporting Reference:**
>Cristian Estan and George Varghese. 2002. New directions in traffic measurement and accounting. In Proceedings of the 2002 conference on Applications, technologies, architectures, and protocols for computer communications (SIGCOMM '02). Association for Computing Machinery, New York, NY, USA, 323–336. https://doi.org/10.1145/633025.633056
>
> Introduced the "conservative update" principle for sketch data structures.

### Implementation Design

**Class Name:** `SpectralBloomFilter`

**Configuration:**
```cpp
enum SBFVariant {
    MINIMUM_SELECTION,    // Standard: query returns min across k positions
    MINIMAL_INCREMENT,    // Conservative: only increment min counter(s)
    RECURRING_MINIMUM     // Supports deletions with secondary filter
};

struct SBFConfig {
    uint hash_k;              // Number of hash functions
    uint logsize;             // Filter size: 2^logsize counters
    uint counter_bits;        // Bits per counter (8, 16, 32, or 64)
    SBFVariant variant;       // Which algorithm to use
};
```

**Core Algorithms:**

**Minimum Selection (MS) - from Cohen & Matias (2003, Section 2.1):**
```
Insert(x):
    for i = 1 to k:
        C[h_i(x)] += 1

Query(x):
    return min{C[h_i(x)] : i = 1..k}
```

**Minimal Increment (MI) - from Cohen & Matias (2003, Section 2.2):**
```
Insert(x):
    m = min{C[h_i(x)] : i = 1..k}
    for i = 1 to k:
        if C[h_i(x)] == m:
            C[h_i(x)] += 1

Query(x):
    return min{C[h_i(x)] : i = 1..k}
```

**Recurring Minimum (RM) - from Cohen & Matias (2003, Section 3):**
```
Uses two structures:
- Primary counter array C
- Secondary min-values array M (smaller)

Insert(x):
    m = min{C[h_i(x)] : i = 1..k}
    for i = 1 to k:
        if C[h_i(x)] == m:
            C[h_i(x)] += 1
            M[h'_i(x)] = m  // record minimum before increment

Delete(x):
    for i = 1 to k:
        if M[h'_i(x)] < C[h_i(x)]:
            C[h_i(x)] -= 1
```

**API:**
```cpp
class SpectralBloomFilter {
public:
    SpectralBloomFilter(const SBFConfig &conf);
    void put(const std::vector<uint64_t> &point);
    uint64_t get_min(const std::vector<uint64_t> &point);    // Frequency estimate
    uint64_t get_mean(const std::vector<uint64_t> &point);   // Alternative estimator
    bool get_bool(const std::vector<uint64_t> &point);       // Membership

    // RM variant only:
    void remove(const std::vector<uint64_t> &point);  // Only if variant == RECURRING_MINIMUM

    std::string summary();
    std::string error_analysis();
};
```

**Advantages:**
- Better frequency estimation accuracy than standard CBF (Cohen & Matias, 2003, Section 4)
- MI reduces overcounting for high-frequency items
- Simple configuration: 4 parameters
- RM variant supports deletions
- Natural interpretation: counter values ≈ actual frequencies

**Disadvantages:**
- MI requires query before insert (higher computational cost)
- RM doubles memory usage for deletion support
- Still vulnerable to counter saturation
- No built-in overflow handling

**Recommended Default:** `variant = MINIMAL_INCREMENT` for best accuracy without deletions

---

## Strategy 3: d-Left Counting Bloom Filter

### Scientific Basis

**Primary Citation:**
> Bonomi, F., Mitzenmacher, M., Panigrahy, R., Singh, S., Varghese, G. (2006). An Improved Construction for Counting Bloom Filters. In: Azar, Y., Erlebach, T. (eds) Algorithms – ESA 2006. ESA 2006. Lecture Notes in Computer Science, vol 4168. Springer, Berlin, Heidelberg. https://doi.org/10.1007/11841036_61

**Extended Version:**
> Bonomi, F., Mitzenmacher, M., Panigrahy, R., Singh, S., & Varghese, G. (2006). Bloom Filters via d-Left Hashing and Dynamic Bit Reassignment. *Harvard University Computer Science Technical Report TR-07-06*.


**Available at:** https://www.eecs.harvard.edu/~michaelm/postscripts/esa2006b.pdf
**Available at:** https://www.eecs.harvard.edu/~michaelm/postscripts/aller2006.pdf

**Key Paper Claims:**
- 50%+ memory savings compared to standard counting bloom filters
- Uses d-left hashing with buckets and fingerprints
- Better cache locality than standard bloom filters
- Supports insertions and deletions
- Deterministic worst-case lookup time

**Foundational Work on d-Left Hashing:**
> Berthold Vöcking. 2003. How asymmetry helps load balancing. J. ACM 50, 4 (July 2003), 568–589. https://doi.org/10.1145/792538.792546

### Implementation Design

**Class Name:** `DLeftCountingBloomFilter`

**Configuration:**
```cpp
struct DLeftCBFConfig {
    uint num_buckets;         // Total number of buckets (must be divisible by d)
    uint d;                   // Number of hash table segments (typically 3-4)
    uint slots_per_bucket;    // Slots in each bucket (typically 3-4)
    uint counter_bits;        // Bits per counter (typically 2-4)
    uint fingerprint_bits;    // Bits for fingerprint (typically 4-8)
};
```

**Data Structure (from Bonomi et al., 2006, Section 3):**
```
Array divided into d segments of (num_buckets/d) buckets each
Each bucket contains:
  - slots_per_bucket entries
  - Each entry: fingerprint (fingerprint_bits) + counter (counter_bits)
```

**Core Algorithm (from Bonomi et al., 2006, Algorithm 1):**
```
Insert(x):
    fingerprint = hash_fp(x)
    for i = 1 to d:
        bucket_i = h_i(x) in segment i

    // Find leftmost bucket with empty slot or matching fingerprint
    for i = 1 to d:
        for each slot in bucket_i:
            if slot.empty or slot.fingerprint == fingerprint:
                if slot.counter < MAX:
                    slot.fingerprint = fingerprint
                    slot.counter += 1
                    return

    // All buckets full, use leftmost
    increment counter in bucket_1's first slot

Query(x):
    fingerprint = hash_fp(x)
    for i = 1 to d:
        bucket_i = h_i(x) in segment i
        for each slot in bucket_i:
            if slot.fingerprint == fingerprint:
                return slot.counter
    return 0  // Not found
```

**API:**
```cpp
class DLeftCountingBloomFilter {
public:
    DLeftCountingBloomFilter(const DLeftCBFConfig &conf);
    void put(const std::vector<uint64_t> &point);
    uint64_t get_count(const std::vector<uint64_t> &point);
    bool get_bool(const std::vector<uint64_t> &point);
    void remove(const std::vector<uint64_t> &point);  // Deletion support

    std::string summary();
    double load_factor();  // Proportion of slots occupied
};
```

**Advantages:**
- 50%+ memory reduction vs. standard CBF (Bonomi et al., 2006, Section 5)
- Better cache locality due to bucket structure
- Supports deletions natively
- Deterministic lookup (check exactly d buckets)
- Good for high insertion rates

**Disadvantages:**
- Complex implementation (bucket management, fingerprints)
- Many configuration parameters (d, slots, counter size, fingerprint size)
- Fingerprint collisions can cause false positives
- Small counters (2-4 bits typical) limit max count
- Load factor degrades with duplicates

**Recommended Defaults (from Bonomi et al., 2006, Section 5.2):**
- `d = 4` (4 hash table segments)
- `slots_per_bucket = 4`
- `counter_bits = 3` (max count = 7)
- `fingerprint_bits = 8`

---

## Strategy 4: Count-Min Sketch with Conservative Update (Optional)

### Scientific Basis

**Primary Citation:**
> Graham Cormode and S. Muthukrishnan. 2005. An improved data stream summary: the count-min sketch and its applications. J. Algorithms 55, 1 (April 2005), 58–75. https://doi.org/10.1016/j.jalgor.2003.12.001

**Conservative Update:**
> Cristian Estan and George Varghese. 2002. New directions in traffic measurement and accounting. In Proceedings of the 2002 conference on Applications, technologies, architectures, and protocols for computer communications (SIGCOMM '02). Association for Computing Machinery, New York, NY, USA, 323–336. https://doi.org/10.1145/633025.633056

**Available at:** https://moodle2.units.it/pluginfile.php/717089/mod_resource/content/0/4.Count_min_sketchl.pdf

**Key Paper Claims:**
- Sublinear space: O(1/ε × log(1/δ)) where ε = error, δ = confidence
- Frequency estimation with guarantees: f̂(i) ≤ f(i) + ε||f||₁ with probability 1-δ
- Conservative update reduces overestimation
- Heavy hitter detection
- Not a bloom filter but complementary use case

### Implementation Design

**Class Name:** `CountMinSketch`

**Configuration:**
```cpp
struct CMSConfig {
    double epsilon;           // Error parameter (typically 0.001 - 0.1)
    double delta;             // Confidence parameter (typically 0.01 - 0.1)
    bool conservative;        // Use conservative update
};

// Internally computes:
// width = ceil(e / epsilon)
// depth = ceil(ln(1 / delta))
```

**Core Algorithm (from Cormode & Muthukrishnan, 2005, Section 2):**

**Standard Update:**
```
Insert(x):
    for i = 1 to depth:
        C[i][h_i(x)] += 1

Query(x):
    return min{C[i][h_i(x)] : i = 1..depth}
```

**Conservative Update (from Estan & Varghese, 2002):**
```
Insert(x):
    current_estimate = min{C[i][h_i(x)] : i = 1..depth}
    for i = 1 to depth:
        if C[i][h_i(x)] == current_estimate:
            C[i][h_i(x)] += 1

Query(x):
    return min{C[i][h_i(x)] : i = 1..depth}
```

**API:**
```cpp
class CountMinSketch {
public:
    CountMinSketch(const CMSConfig &conf);
    void put(const std::vector<uint64_t> &point);
    uint64_t get_frequency(const std::vector<uint64_t> &point);

    // Heavy hitter queries
    std::vector<std::pair<uint64_t, uint64_t>> get_heavy_hitters(double threshold);

    std::string summary();
    double epsilon_actual();  // Actual error parameter
    double delta_actual();    // Actual confidence parameter
};
```

**Advantages:**
- Extremely simple configuration: just ε and δ
- Theoretical guarantees on accuracy (Cormode & Muthukrishnan, 2005, Theorem 1)
- Excellent for heavy hitter detection
- Conservative update improves accuracy
- Sublinear space independent of data size

**Disadvantages:**
- Not a bloom filter replacement
- No membership testing (always returns frequency ≥ 0)
- Higher memory than bloom filters for membership testing
- Different use case (frequency estimation vs. set membership)

**Note:** This is included as it's commonly used alongside bloom filters and shares implementation patterns, but serves a different primary purpose.

---

## Hybrid Strategy: Adaptive Cascade CountingGloBiMap (Enhancement)

### Scientific Basis

**Primary Citation:**
> D. Ficara, S. Giordano, G. Procissi and F. Vitucci, "MultiLayer Compressed Counting Bloom Filters," IEEE INFOCOM 2008 - The 27th Conference on Computer Communications, Phoenix, AZ, USA, 2008, pp. 311-315, doi: 10.1109/INFOCOM.2008.71.

**Available at:** https://www.researchgate.net/profile/Domenico-Ficara/publication/4334204_MultiLayer_Compressed_Counting_Bloom_Filters/links/02bfe5108d632afef0000000/MultiLayer-Compressed-Counting-Bloom-Filters.pdf

**Key Paper Claims:**
- Hierarchical architecture with tighter bounds on overflow
- Huffman-based compression of multilayer structure
- 50% memory reduction vs. standard CBF
- Formal analysis of overflow probability per layer

**Supporting Theory:**
> Broder, Andrei & Mitzenmacher, Michael. (2003). Survey: Network Applications of Bloom Filters: A Survey.. Internet Mathematics. 1. 10.1080/15427951.2004.10129096.
>
> Survey of bloom filter applications including multilayer designs.

### Enhancement Design

Instead of a completely new implementation, enhance the existing `CountingGloBiMap` with optional adaptive features:

**Enhanced Configuration:**
```cpp
struct FilterConfig {
    uint hash_k;
    std::vector<LayerConfig> layers;

    // NEW: Optional adaptive features
    bool use_minimal_increment = false;       // From Cohen & Matias (2003)
    double cascade_factor = 1.0;              // Fraction of max before cascade (0.0-1.0)
    bool use_compression = false;             // From Ficara et al. (2008)
};
```

**Implementation:**
1. **Minimal Increment:** Apply conservative update within existing cascade logic
2. **Cascade Factor:** Cascade at (max_value × cascade_factor) instead of max_value
3. **Compression:** Apply run-length encoding to sparse layers (future work)

**Advantages:**
- Backward compatible with existing code
- Incremental improvements
- Can be enabled/disabled for comparison

**Disadvantages:**
- Still requires multilayer configuration
- Only modest simplification

---

## Recommended Implementation Priority

### Phase 1: Core Alternative Implementations (High Priority)
1. **Variable-Increment CBF** - Simplest, proven 50% memory reduction
2. **Spectral Bloom Filter** - Multiple variants, good documentation

### Phase 2: Advanced Implementations (Medium Priority)
3. **d-Left CBF** - More complex but substantial benefits
4. **Adaptive Cascade Enhancement** - Improve existing implementation

### Phase 3: Optional Extensions (Low Priority)
5. **Count-Min Sketch** - Different use case but commonly requested
6. **Compression variants** - Based on Ficara et al. (2008)

---

## Shared Infrastructure

All implementations should share:

1. **Common hash functions** - Reuse existing MurmurHash from `murmur.hpp`
2. **Common interface** - Where possible, provide `put()`, `get_min()`, `get_bool()` methods
3. **Configuration serialization** - JSON support in `globimap_test_config.hpp`
4. **Error analysis** - Shared methods for `summary()`, `error_summary()`

**Proposed header structure:**
```
include/
├── counting_globimap.hpp              # Existing multilayer implementation
├── variable_increment_bf.hpp          # New: VI-CBF
├── spectral_bloom_filter.hpp          # New: SBF with MI/MS/RM variants
├── dleft_counting_bf.hpp              # New: d-Left CBF
├── count_min_sketch.hpp               # New: CMS (optional)
├── hashfn.hpp                         # Existing: hash interface
├── murmur.hpp                         # Existing: MurmurHash
└── common/
    ├── bf_interface.hpp               # New: common interface
    └── bf_config.hpp                  # New: shared config utilities
```

---

## Experimental Validation Plan

For each new implementation, create experiments to measure:

1. **Memory efficiency** - Bytes per element stored
2. **False positive rate** - vs. theoretical predictions
3. **Query accuracy** - For cardinality/frequency estimation
4. **Insert throughput** - Operations per second
5. **Query throughput** - Operations per second
6. **Scalability** - Performance with varying dataset sizes

**Comparison experiments:**
```
experiments/src/
├── compare_all_implementations.cpp    # Side-by-side comparison
├── test_vi_cbf.cpp                    # Variable increment tests
├── test_spectral_bf.cpp               # Spectral BF tests
├── test_dleft_cbf.cpp                 # d-Left CBF tests
└── benchmark_suite.cpp                # Comprehensive benchmarks
```

**Test datasets:**
- Synthetic uniform distribution
- Synthetic Zipfian distribution (heavy hitters)
- Real geospatial datasets (existing Twitter, OSM datasets)
- Polygon datasets (existing shapefile tests)

---

## Documentation Updates Required

1. **README.md:**
   - Add section comparing all implementations
   - Decision tree: which implementation to use for which use case
   - Performance comparison tables

2. **CLAUDE.md:**
   - Update architecture section with new implementations
   - Add build commands for new experiments
   - Describe configuration approaches

3. **New: IMPLEMENTATION_GUIDE.md:**
   - Detailed guide for each implementation
   - Configuration best practices
   - Trade-off analysis
   - Code examples

4. **API Documentation:**
   - Doxygen comments for all new classes
   - Example usage in header comments

---

## References

### Primary Papers

1. Rottenstreich, O., Kanizo, Y., & Keslassy, I. (2012). The Variable-Increment Counting Bloom Filter. *IEEE INFOCOM 2012*, 1880-1888. https://doi.org/10.1109/INFCOM.2012.6195573

2. Cohen, S., & Matias, Y. (2003). Spectral Bloom Filters. *Proceedings of the 2003 ACM SIGMOD International Conference on Management of Data*, 241-252. https://doi.org/10.1145/872757.872787

3. Bonomi, F., Mitzenmacher, M., Panigrahy, R., Singh, S., & Varghese, G. (2006). An Improved Construction for Counting Bloom Filters. *Algorithms – ESA 2006*, 4168, 684-695. https://doi.org/10.1007/11841036_61

4. Cormode, G., & Muthukrishnan, S. (2005). An Improved Data Stream Summary: The Count-Min Sketch and its Applications. *Journal of Algorithms*, 55(1), 58-75. https://doi.org/10.1016/j.jalgor.2003.12.001

5. Ficara, D., Giordano, S., Procissi, G., Vitucci, F., Antichi, G., & Di Pietro, A. (2008). Multilayer Compressed Counting Bloom Filters. *IEEE INFOCOM 2008*, 311-315. https://doi.org/10.1109/INFOCOM.2008.62

### Supporting References

6. Estan, C., & Varghese, G. (2002). New Directions in Traffic Measurement and Accounting. *ACM SIGCOMM 2002*, 323-336. https://doi.org/10.1145/633025.633056

7. Vöcking, B. (2003). How Asymmetry Helps Load Balancing. *Journal of the ACM*, 50(4), 568-589. https://doi.org/10.1145/792538.792546

8. Broder, A., & Mitzenmacher, M. (2004). Network Applications of Bloom Filters: A Survey. *Internet Mathematics*, 1(4), 485-509. https://doi.org/10.1080/15427951.2004.10129096

### Technical Reports and Extended Versions

9. Bonomi, F., Mitzenmacher, M., Panigrahy, R., Singh, S., & Varghese, G. (2006). Bloom Filters via d-Left Hashing and Dynamic Bit Reassignment. *Harvard University Computer Science Technical Report TR-07-06*. https://www.eecs.harvard.edu/~michaelm/postscripts/aller2006.pdf

10. Cormode, G., & Muthukrishnan, S. (2004). An Improved Data Stream Summary: The Count-Min Sketch and its Applications. *DIMACS Technical Report 2004-15*. https://people.eecs.berkeley.edu/~satishr/cs270/sp11/rough-notes/CountMin.pdf

### Online Resources

11. Rottenstreich, O., Kanizo, Y., & Keslassy, I. (2012). Variable-Increment Counting Bloom Filter. Technical report. https://webee.technion.ac.il/~isaac/p/infocom12_variable.pdf

12. Cohen, S., & Matias, Y. (2003). Spectral Bloom Filters. Technical report. http://www.cs.tau.ac.il/~matias/publications/sbf_sigmod_2003.pdf

13. Vallentin, M. (2011). A Garden Variety of Bloom Filters. Blog post. http://matthias.vallentin.net/blog/2011/06/a-garden-variety-of-bloom-filters/

14. Nochlin, P. (2024). Spectral Bloom Filters - Tutorial. Blog post. https://pncnmnp.github.io/blogs/spectral-bloom-filters.html

---

## Timeline Estimate

- **Variable-Increment CBF:** 1-2 weeks (implementation + tests)
- **Spectral Bloom Filter:** 1-2 weeks (3 variants)
- **d-Left CBF:** 2-3 weeks (complex bucket management)
- **Adaptive Cascade Enhancement:** 1 week
- **Count-Min Sketch:** 1 week (optional)
- **Documentation & Experiments:** 1-2 weeks
- **Total:** 7-11 weeks for complete implementation

---

## Success Criteria

1. All new implementations compile and pass unit tests
2. Each implementation matches or exceeds published performance claims
3. Configuration is measurably simpler (fewer required parameters)
4. Comprehensive comparison experiments completed
5. Documentation provides clear guidance on implementation selection
6. Backward compatibility maintained with existing `CountingGloBiMap`
