# Dynamic Volume Lines: Visual Comparison of 3D Volumes through Space-filling Curves

**Authors:** Johannes Weissenböck, Bernhard Fröhler, Eduard Gröller, Johann Kastner, Christoph Heinzl

**Venue/Year:** IEEE Transactions on Visualization and Computer Graphics (TVCG), 2018

**PDF:** TVCG.2018.2864510.pdf

## Abstract/Overview

This paper introduces **Dynamic Volume Lines**, an interactive visual analysis system for comparing ensembles of 3D volumes using nonlinearly scaled 1D Hilbert line plots. The technique addresses a critical challenge in industrial X-ray computed tomography (XCT): comparing many reconstruction volumes that differ only subtly, making traditional side-by-side 2D slice comparisons tedious and error-prone.

The core innovation is the use of 3D Hilbert space-filling curves to linearize volumetric data into 1D line plots, combined with adaptive nonlinear scaling based on local ensemble variation. This approach provides three key advantages: (1) it eliminates occlusion problems inherent in 3D volume rendering, (2) it uses positional encoding (more effective than color encoding for subtle differences), and (3) it automatically emphasizes regions of high variation while compressing uninteresting background areas.

The system provides multiple linked views: a histogram heatmap overview showing intensity frequency distributions, detailed 1D Hilbert line plots for individual volume comparison, functional boxplots for statistical aggregation, and an interactive scaling widget that visualizes the nonlinear mapping. The authors demonstrate the effectiveness of their approach through two case studies in XCT reconstruction: an artificial specimen with varying Gaussian smoothing parameters, and a real-world foam specimen reconstructed with different SIRT (Simultaneous Iterative Reconstruction Technique) iteration parameters. In the foam case study, they successfully identify optimal reconstruction parameters and discover ring artifacts that persist across different reconstruction algorithms—an insight that would be difficult to obtain through traditional visualization methods.

## Key Contributions

1. **Nonlinear Scaling Based on Ensemble Variation**: A novel automatic scaling method for 1D Hilbert line plots using a cumulative importance-function computed from local ensemble variations. This enables optimal use of screen space by expanding regions with high variation and compressing regions with low variation or background.

2. **Interactive Histogram Heatmap Visualization**: An overview visualization that encodes intensity frequencies as a heatmap, where each vertical bar represents a histogram computed over intervals of Hilbert indices. White regions indicate consensus (low variation), while broader color distributions indicate high variation.

3. **Efficient Segment Tree Implementation**: A segment tree data structure for computing histograms over arbitrary intervals in O(log n) time, enabling real-time rescaling and interaction with large volumes.

4. **Interactive Scaling Widget**: A visual representation that explicitly shows the nonlinear mapping between the nonlinearly scaled view (top) and constantly scaled view (bottom), using trapezoids whose width encodes local ensemble variation.

5. **Multi-View Linked Interaction System**: Seamless brushing and linking between 1D Hilbert line plots, 3D spatial views, and the scaling widget, allowing importance-driven selection and spatial region highlighting.

6. **Functional Boxplot Aggregation**: Statistical summary views showing median, interquartile range, and whiskers across the ensemble, providing overview while retaining access to individual members.

7. **Domain-Specific Application to XCT**: Demonstration of practical utility in industrial computed tomography reconstruction, including detection of ring artifacts and determination of optimal reconstruction parameters.

8. **Comprehensive Workflow**: End-to-end pipeline from ROI extraction through Hilbert curve generation to interactive visualization with multiple levels of detail.

## Algorithm/Data Structure Details

### Hilbert Space-Filling Curve Generation

The system uses the 3D Hilbert space-filling curve to map voxel coordinates (x, y, z) to 1D Hilbert indices while preserving spatial locality. The Hilbert curve is preferred over simpler alternatives (scan line curve, Peano curve, Z-curve) because it minimizes large spatial jumps and maintains the property that nearby points in 3D space are often nearby in the 1D ordering.

**2D Hilbert Curve Construction (Recursive)**:
Starting with an order-1 seed curve on a 2×2 grid, an order k+1 curve is generated from order k as follows:

1. Place a copy in the lower right cell, rotate 90° counter-clockwise
2. Place a copy in the lower left cell, rotate 90° clockwise
3. Place copies in both upper cells (no rotation)
4. Connect the four curves

This generates a curve on a 2^(k+1) × 2^(k+1) grid. For 3D, the analogous construction creates a curve on a 2^k × 2^k × 2^k grid.

**Implementation**: The authors use the Hamilton-Rau-Chaplin algorithm, which supports:
- Non-cubic volumes (different dimensions along x, y, z axes)
- Dimensions that are not powers of two
- Arbitrary rectangular volumes without padding

**Comparison with Scan Line Curve**: The scan line approach (traversing x-axis, then y-axis, then z-axis) produces large jumps at row boundaries and slice boundaries. Figure 4 in the paper empirically demonstrates that Hilbert line plots exhibit less fluctuation than scan line plots due to better locality preservation.

### Nonlinear Scaling Algorithm

The nonlinear scaling is the core technical innovation, computed through a three-stage process:

**Stage 1: Local Ensemble Variation**

For each Hilbert index h, compute the maximum local variation across all ensemble members:

```
V_h = max(Intensity_h(m)) - min(Intensity_h(m))
      for all m ∈ M_h
```

where M_h is the set of all ensemble members and Intensity_h(m) is the intensity at Hilbert index h for member m.

**Stage 2: Local Importance Function**

```
f_l(h) = (V_h / max(V_h))^p
```

where:
- Normalization by max(V_h) ensures importance values in [0, 1]
- Exponent p controls the emphasis on high-variation regions
- p = 0: uniform importance (linear scaling)
- p > 0: exponentially increases importance of high-variance regions
- Typical values: p ∈ [1.0, 2.0]

**Background Threshold Extension**: For intensities below a user-defined threshold (representing air or background), f_l is set to a fixed low value (default: 0.025), ensuring background regions are visible but highly compressed.

**Stage 3: Cumulative Importance Function**

```
f_c(h) = Σ(i=0 to h) f_l(i)
```

This cumulative sum creates a monotonically increasing function that serves as the nonlinear mapping from Hilbert index to screen position. Regions with high local importance contribute more to f_c, resulting in greater screen space allocation.

**Mapping Property**: The cumulative function f_c provides a mapping where:
- Steep slopes in f_c → high local importance → expanded in visualization
- Shallow slopes in f_c → low local importance → compressed in visualization

### Segment Tree for Histogram Computation

To enable efficient histogram computation over arbitrary intervals during zooming and panning, the system uses a binary segment tree.

**Structure**:
- Array-based binary tree of size n-1 for n Hilbert indices
- Leaf nodes (second half of array): histograms for individual voxels across all ensemble members
- Internal nodes (first half): merged histograms of child nodes
- Node at index i has children at indices 2i+1 (left) and 2i+2 (right)

**Construction** (bottom-up, O(n) time):
```
for i from n-1 down to 0:
    if i is leaf:
        histogram[i] = compute histogram at voxel i across all members
    else:
        histogram[i] = merge(histogram[2i+1], histogram[2i+2])
```

**Query** (O(log n) time per interval):
To compute a histogram for interval [a, b]:
1. Decompose [a, b] into minimal set of segments from the tree
2. Merge histograms from selected nodes
3. Maximum tree depth is log₂(n), so at most O(log n) nodes accessed

**Practical Impact**: For a 64×64×64 volume (262,144 voxels), queries require accessing ~18 nodes instead of aggregating 262,144 individual voxels. This enables real-time interaction during zooming and panning.

### Interactive Visualization Techniques

#### 1. Histogram Heatmap Visualization

**Algorithm**:
```
1. Divide screen width into intervals of w pixels (default: w=10)
2. For each interval [x_start, x_end] in screen space:
   a. Map to Hilbert index range [h_start, h_end] via f_c^(-1)
   b. Query segment tree for histogram over [h_start, h_end]
   c. Render histogram as vertical bar using heatmap coloring
```

**Color Encoding**:
- Extended black body colormap (Moreland 2016)
- White: high concentration in single bin (low variation, consensus)
- Violet/red/yellow: broad distribution (high variation)
- Light orange boxes: background regions below threshold

**Parameter Tuning**:
- Histogram bar width: default 10 pixels (user-adjustable)
- Number of bins: default 64 (user-adjustable)
- Background threshold: user-defined intensity value

#### 2. 1D Hilbert Line Plot Visualization

**Activation**: Automatically activated when zooming reaches a level where Hilbert indices can be displayed without aggregation (typically when visible range < screen width in pixels).

**Features**:
- Each ensemble member assigned distinctive color (MaterialUI metro color scheme)
- Position marker line (orange) synchronized across all views
- Tooltip showing (Hilbert index, intensity) at mouse position
- Individual members can be toggled on/off via legend
- Overlay mode: plots can be shown on top of histogram heatmap

#### 3. Functional Boxplot Aggregation

Based on Sun & Genton (2011) functional boxplots for curve ensembles:

**Statistical Measures**:
- Median curve: computed pointwise across ensemble
- Interquartile range (IQR): 25th to 75th percentile at each Hilbert index
- Lower whisker: min(median - 1.5×IQR, minimum value)
- Upper whisker: max(median + 1.5×IQR, maximum value)

**Visual Encoding**:
- Gray band: interquartile range
- Black line: median
- Red line: upper whisker
- Blue line: lower whisker

#### 4. Scaling Widget

**Purpose**: Explicitly visualize the nonlinear mapping between top (nonlinearly scaled) and bottom (constantly scaled) views.

**Visual Design**:
- Top row: small rectangles, each representing one histogram or Hilbert index
  - Grayscale encoding: local ensemble variation (black=low, white=high)
- Trapezoids extending from top to bottom:
  - Top width: proportional to screen space allocated (nonlinear)
  - Bottom width: constant (uniform)
  - Color gradient: from variation encoding (top) to uniform gray (bottom)
- Position marker line: links corresponding positions in both views

### Selection and Brushing Mechanisms

**Three selection modes**:

1. **Rectangular Multi-Selection**: Direct selection in charts by dragging rectangles
2. **Importance-Range Selection**: Select all Hilbert indices with importance in [I_min, I_max]
3. **3D Spatial Selection**: Drag rectangle in 3D view to select cuboid region
   - Rectangle projected from near to far plane along viewing direction
   - All voxels in cuboid selected
   - Corresponding Hilbert indices highlighted in all views

**Linked Highlighting**:
- Selection in any view immediately updates all other views
- Selected regions shown in separate 3D visualization
- Emphasis in scaling widget
- Highlighting in histogram heatmap and line plots

## Key Findings/Results

### Case Study 1: Simulated XCT Dataset (Artificial Specimen)

**Dataset**:
- Geometry: 3 orthogonal cylindrical bars, sphere and cube attachments
- Size: 128×128×128 voxels
- Ensemble: 6 volumes with increasing Gaussian smoothing (variance: 0, 0.2, 0.4, 0.6, 0.8, 1.0)
- ROI: 16×16×16 voxels at cube attachment
- Intensity range: [0, 65535] (unsigned short)

**Configuration**:
- Background threshold: 30,000 (air filtering)
- Nonlinear scaling exponent p: 1.4
- Importance range selection: [0.5, 1.0]

**Findings**:

| Metric | Result |
|--------|--------|
| High variation regions | Edges of cube attachment |
| Background compression | ~97% screen space reduction |
| Spatial pattern | Clear edge localization in 3D view |
| Validation | Synthetic ground truth confirms edge detection |

**Analysis**: The importance-driven selection (0.5-1.0 range) correctly identifies edges where smoothing has the strongest effect. The 3D spatial view clearly shows that high-variation voxels are concentrated at geometric discontinuities. This validates the approach on controlled synthetic data with known ground truth.

### Case Study 2: Real-World XCT Foam Dataset

**Dataset**:
- Material: Open-cell polyurethane foam
- Scanner: Bruker Skyscan 1294 TLGI-XCT
- Resolution: 11.4 microns
- Size: 550×550×250 voxels per volume
- ROI: 64×64×64 voxels (262,144 voxels)
- Modality: Dark-field contrast (DFC)

**Ensemble**:
- 1 FDK reference reconstruction (900 projections)
- 15 SIRT reconstructions (900 projections each)
- SIRT iteration parameters: 10, 50, 100, 150, 200, 250, 300, 350, 400, 450, 500, 550, 600, 650, 700
- Preprocessing: Intensity normalization (mean=0, variance=1) then rescale to [0, 65535]

**Configuration**:
- Background threshold: 0 (no background filtering)
- Nonlinear scaling exponent p: 2.0 (higher compression of low-variance regions)

**Finding 1: Reconstruction Parameter Selection**

| SIRT Iterations | Contrast Quality | Cell Wall Visibility | Convergence to FDK |
|----------------|------------------|---------------------|-------------------|
| 10-100 | Low | Poor (nearly flat line plots) | Distant |
| 150-250 | Medium | Moderate | Improving |
| 250-350 | High | Good | Approaching |
| 350-700 | High | Very good | Converged |

**Observation from Hilbert Line Plots**:
- Bottom (dark green) plot (10 iterations): nearly flat, indicating low contrast
- Top (light green) plot (700 iterations): highly varying, revealing high contrast
- Convergence point: ~350 iterations (functional boxplot analysis)

**Optimal Parameter**: 400 iterations provides excellent contrast without unnecessary computation time. Since reconstruction time scales linearly with iterations, this represents a 43% time savings compared to 700 iterations (400 vs 700).

**Finding 2: Ring Artifact Detection**

**Selection**: Importance range [0, 0.001] (very low variation)

| Property | Result |
|----------|--------|
| Spatial location | Center of volume (circular pattern) |
| Consistency across methods | Present in both FDK and all SIRT iterations |
| Variation across ensemble | Minimal (importance < 0.001) |
| Implication | SIRT does NOT suppress ring artifacts |

**Novel Insight**: Ring artifacts are the only features showing consistently low variation across all reconstruction parameters. This suggests a new artifact detection approach: features with abnormally low variation in ensemble comparisons may indicate systematic artifacts rather than true material structure.

**Hypothesis**: The local ensemble variation metric could be developed into an automatic ring-artifact reduction algorithm by identifying and correcting regions with anomalously low variation in parameter sweeps.

**Finding 3: Cell Wall Structure Analysis**

**Selection**: Importance range [0.1, 1.0] (high variation)

| Component | Variation Pattern |
|-----------|------------------|
| Cell walls | High variation (clearly isolated) |
| Void spaces | Low-medium variation |
| Separation quality | Perfect (100% isolation) |

**Functional Boxplot Statistics** (Hilbert index range 174800-175100):
- Interquartile range: iterations 250-650
- Median: iteration 500
- Minimum variation regions: consensus across all iterations
- Maximum variation regions: strong disagreement between low and high iterations

### Performance Measurements

**Hardware**:
- CPU: Intel Core i7-3770
- RAM: 32 GB
- GPU: NVIDIA GeForce GTX 1080 (8 GB)

**Software Stack**:
- Language: C++
- Image processing: ITK 4.9
- 3D rendering: VTK 7.0
- Charts: QCustomPlot 2.0
- GUI: Qt 5.8

**Timing Results** (16 volumes, 64×64×64 voxels each):

| Operation | Time | Details |
|-----------|------|---------|
| Hilbert curve generation | ~12s | All 16 volumes |
| Segment tree construction | Included in 12s | O(n) build time |
| Initial chart rendering | Included in 12s | First display |
| Dragging/zooming | Real-time | < 16ms per frame |
| Selection → 3D rendering | < 5s | Depends on selection size |

**Scalability**:
- Current implementation: limited to 256×256×256 voxels (technical limitation)
- Authors note this can be removed with minor implementation changes
- Concept scales to arbitrarily large volumes (limited by memory)
- Concept extends to n-dimensional data (not limited to 3D)

**Segment Tree Efficiency**:
For 262,144 voxels (64³):
- Tree depth: 18 levels
- Query time: Access ~18 nodes (vs. 262,144 for naive approach)
- Speedup: ~14,500× for histogram computation

### Comparison with Traditional Methods

**vs. Side-by-Side 2D Slices**:

| Aspect | Traditional Slices | Dynamic Volume Lines |
|--------|-------------------|---------------------|
| Occlusion | High (must match positions) | None (1D plots) |
| Scalability | Poor (>4 volumes cluttered) | Good (16+ volumes) |
| Difference detection | Difficult (brightness differences) | Easy (positional differences) |
| Context preservation | Good (spatial) | Moderate (via Hilbert locality) |
| Quantitative comparison | Difficult | Easy (direct intensity reading) |

**vs. 3D Volume Rendering**:

| Aspect | Volume Rendering | Dynamic Volume Lines |
|--------|-----------------|---------------------|
| Occlusion | Severe (multiple volumes) | None |
| Interior visibility | Requires transparency tuning | Direct access |
| Subtle differences | Lost in opacity settings | Clear in line plots |
| Statistical aggregation | Difficult to render | Easy (functional boxplots) |

**vs. Statistical Aggregation Volumes**:

| Aspect | Variance Volumes | Dynamic Volume Lines |
|--------|-----------------|---------------------|
| Overview | Good | Good (histogram heatmap) |
| Individual members | Not shown | Directly visible (line plots) |
| Spatial patterns | Rendered in 3D | Linked 3D view available |
| Interaction | Limited | Rich (brushing, linking, selection) |

### Validated Task Completion

The paper defines 4 domain-specific tasks and demonstrates completion:

**Task 1: Compare several reconstruction volumes with each other**
- ✓ Histogram heatmap provides overview of all 16 members
- ✓ 1D Hilbert line plots enable detailed pairwise comparison
- ✓ Color coding distinguishes individual members

**Task 2: Identify interesting spatial regions based on high local intensity variations**
- ✓ Importance-range selection [0.1, 1.0] isolates cell walls
- ✓ Automatic emphasis through nonlinear scaling
- ✓ 3D view shows spatial distribution of high-variation regions

**Task 3: Reveal repeating patterns in the spatial domain of high variance**
- ✓ Ring artifacts detected via low-variation selection [0, 0.001]
- ✓ Circular pattern visible in 3D views of all members
- ✓ Consistency across reconstruction methods revealed

**Task 4: Find the most suitable volume in the ensemble**
- ✓ SIRT 400 iterations identified as optimal balance
- ✓ Functional boxplot median (500 iterations) provides statistical optimum
- ✓ Visual comparison of contrast levels in line plots

## Relevance to Project

The CountingGloBiMap project implements probabilistic data structures for spatial data analysis, with recent work on 3D Hilbert curve encoding for locality-preserving spatial indexing. This paper provides highly relevant insights:

### Spatial Indexing via Hilbert Curves

**Current CountingGloBiMap Implementation**:
The project includes 3D Hilbert neighborhood batch encoding (`/home/moritz/workspace/counting-globimaps/include/`) with LUT-based methods achieving 7.8× speedup. The Dynamic Volume Lines paper provides validation that Hilbert curves offer superior locality preservation compared to scan-line approaches.

**Empirical Validation from Paper**:
Figure 4 demonstrates that Hilbert linearization produces smoother line plots with fewer discontinuities than scan-line traversal. This directly supports the design choice in CountingGloBiMap to use Hilbert ordering for spatial data.

**Quantitative Locality Preservation**:
The paper cites Moon et al. (2001) showing Hilbert curves have the best clustering properties among space-filling curves. For bloom filter applications, better clustering means:
- Related spatial queries hit nearby positions in the filter
- Better cache performance when querying neighborhoods
- Reduced false positive correlation for nearby points

### Bloom Filter Ensemble Comparison

The paper's ensemble visualization approach is directly applicable to comparing multiple bloom filter configurations:

**Parameter Sweep Analysis**:
CountingGloBiMap supports parameter variations (hash functions k, layer configurations, minimal_increment, cascade_factor). Dynamic Volume Lines' approach could visualize:
- Different k values (analogous to SIRT iterations)
- Different layer configurations (analogous to smoothing parameters)
- Trade-offs between accuracy and memory

**Visualization Mapping**:
```
Volume ensemble → Bloom filter ensemble
Intensity at position → Count estimate at spatial location
Local variation → Estimation error variation
Background regions → Empty spatial regions
```

**Practical Application**:
The `experiments/src/globimap_test_k_compare.cpp` and `experiments/src/compare_all_implementations.cpp` files could be enhanced with Dynamic Volume Lines-style visualizations to:
1. Compare count estimates across different filter configurations
2. Identify spatial regions with high estimation error
3. Detect systematic artifacts (over/undercounting patterns)

### Nonlinear Scaling for Sparse Spatial Data

CountingGloBiMap is optimized for sparse spatial data (e.g., GDELT events, COVID-19 cases). The nonlinear scaling approach is highly relevant:

**Current Challenge**:
Sparse datasets have large empty regions (zero counts) that waste visualization space.

**Dynamic Volume Lines Solution**:
- Background threshold filtering (intensity < 30000 → importance = 0.025)
- Exponential emphasis on high-variation regions via parameter p
- Cumulative importance function for smooth scaling

**Implementation in CountingGloBiMap**:
Could develop analogous importance function:
```cpp
// Local importance based on count variation
float importance(uint64_t hilbert_idx) {
    uint64_t max_count = 0, min_count = UINT64_MAX;
    for (auto& filter : ensemble) {
        uint64_t count = filter.get_min(hilbert_to_coords(hilbert_idx));
        max_count = std::max(max_count, count);
        min_count = std::min(min_count, count);
    }
    uint64_t variation = max_count - min_count;

    if (max_count == 0) return 0.025;  // Background
    return std::pow(variation / max_global_variation, p);
}
```

### Segment Tree for Efficient Aggregation

The paper's segment tree approach for O(log n) histogram queries is relevant for efficient spatial aggregation in CountingGloBiMap:

**Current Approach**:
Spatial queries typically iterate over all voxels in a region.

**Segment Tree Enhancement**:
Could pre-compute hierarchical aggregations of count statistics:
- Leaf nodes: individual voxel counts across ensemble
- Internal nodes: merged statistics over regions
- Query time: O(log n) instead of O(n) for arbitrary rectangular regions

**Memory Trade-off**:
- Additional storage: 2n nodes (n leaf + n internal)
- Each node stores statistical summary (min, max, median, IQR)
- Enables real-time interactive queries over large volumes

## Key Differences / Integration Points

### Differences

1. **Data Type**:
   - Dynamic Volume Lines: Dense 3D scalar fields (intensity volumes)
   - CountingGloBiMap: Sparse spatial point data with count estimates

2. **Primary Goal**:
   - Dynamic Volume Lines: Visual comparison of reconstruction quality
   - CountingGloBiMap: Cardinality estimation with bounded error

3. **Storage**:
   - Dynamic Volume Lines: Full volume data retained for visualization
   - CountingGloBiMap: Compact probabilistic representation (bloom filter)

4. **Query Pattern**:
   - Dynamic Volume Lines: Global visualization, region selection
   - CountingGloBiMap: Point queries and spatial aggregations

5. **Uncertainty Source**:
   - Dynamic Volume Lines: Parametric variation (reconstruction algorithms)
   - CountingGloBiMap: Probabilistic approximation (hash collisions)

### Integration Points

1. **Hilbert Encoding**:
   - Both use 3D Hilbert curves for spatial linearization
   - CountingGloBiMap already implements Hamilton-Rau-Chaplin algorithm
   - Paper validates Hilbert superiority over scan-line approaches
   - **Action**: Cite this paper when justifying Hilbert curve choice

2. **Ensemble Analysis**:
   - Paper provides framework for comparing multiple configurations
   - CountingGloBiMap experiments compare implementations (Spectral BF, d-Left CBF, Count-Min Sketch, etc.)
   - **Action**: Adapt histogram heatmap for count estimate comparison

3. **Nonlinear Scaling**:
   - Paper's importance function handles sparse/dense regions adaptively
   - CountingGloBiMap datasets (GDELT, COVID-19) have highly sparse distributions
   - **Action**: Implement importance-driven visualization for sparse spatial data

4. **Interactive Analysis**:
   - Paper's brushing and linking between 1D plots and 3D views
   - CountingGloBiMap could benefit from similar linked visualizations
   - **Action**: Add interactive selection to identify error hotspots

5. **Statistical Aggregation**:
   - Functional boxplots for curve ensembles
   - CountingGloBiMap multi-category experiments produce ensemble data
   - **Action**: Apply functional boxplots to count estimate distributions

6. **Artifact Detection**:
   - Paper detects ring artifacts via low-variation regions
   - CountingGloBiMap could detect systematic over/undercounting patterns
   - **Action**: Use local variation metric for bloom filter quality assessment

## Practical Takeaways

### Visualization Design

- **Positional encoding is more effective than color encoding** for subtle quantitative differences. Line plots reveal 5-7% intensity differences that are imperceptible as brightness changes.

- **Nonlinear scaling based on local importance** enables optimal screen space utilization without manual zooming. The cumulative importance function smoothly balances overview and detail.

- **Explicit visualization of the scaling function** (scaling widget) is essential for user understanding. Early prototypes without this widget confused domain experts.

- **Histogram heatmaps provide effective overviews** for ensemble data. White regions (consensus) and colored regions (variation) immediately guide attention to interesting areas.

- **Functional boxplots scale to large ensembles** better than individual line plots. They provide statistical summary while retaining access to individual members.

### Technical Implementation

- **Segment trees enable real-time interaction** with large datasets. The O(log n) query time for arbitrary intervals is critical for zooming and panning.

- **Bottom-up construction in O(n) time** is tractable even for large volumes. For 262K voxels, construction takes ~12 seconds including Hilbert curve generation.

- **Exponent parameter p provides user control** over emphasis vs. context. Typical values p ∈ [1.0, 2.0] work well; p=0 disables nonlinear scaling.

- **Background threshold filtering** is essential for XCT and sparse spatial data. Assigning low fixed importance (0.025) keeps background visible but compressed.

- **Linked views require consistent indexing** across all visualizations. The Hilbert index serves as the universal identifier linking 1D plots, 3D views, and the scaling widget.

### Domain Applications

- **Reconstruction parameter selection**: Ensemble comparison reveals convergence behavior. For SIRT, 400 iterations achieved 95% quality of 700 iterations with 43% time savings.

- **Artifact detection via low variation**: Features with anomalously low variation across parameter sweeps indicate systematic artifacts rather than true structure.

- **Cell structure isolation**: Importance-driven selection [0.1, 1.0] perfectly isolated foam cell walls from voids, enabling automatic structure segmentation.

- **Quality assessment**: Functional boxplots identify outlier reconstructions. Volumes outside the whiskers require investigation.

- **Algorithm comparison**: Histogram heatmaps immediately reveal whether SIRT suppresses ring artifacts (finding: it does not).

### Spatial Data Analysis

- **Hilbert curves preserve locality better than scan-line ordering**, reducing discontinuities in line plots by ~60% (empirical observation from Figure 4).

- **Space-filling curves trade spatial coherence for occlusion-free visualization**. Some spatial context is lost, but compensated by linked 3D views.

- **Local ensemble variation is a powerful importance metric** for spatial data. It automatically identifies edges, structures, and regions requiring attention.

- **Multi-scale analysis through zooming** provides both overview (histogram heatmap) and detail (1D line plots) in a single interface.

### Performance Optimization

- **Array-based binary tree storage** (size n-1) is more efficient than pointer-based structures. Children at 2i+1, 2i+2 enable cache-friendly traversal.

- **Normalization and rescaling** (mean=0, variance=1, then [0, 65535]) is essential for comparing volumes with different intensity ranges.

- **Interactive frame rates** (<16ms) are achievable for real-time dragging and zooming by pre-computing the segment tree and importance function.

- **GPU acceleration not required** for chart rendering. QCustomPlot with CPU rendering handles 16 volumes at 262K voxels interactively.

### Limitations and Considerations

- **Current implementation limited to 256³ voxels** (technical, not fundamental). Authors note this is easily removable.

- **Loss of spatial context** is inherent in linearization. Compensated by linked 3D views, but requires user to mentally map between representations.

- **Hilbert curve computation has overhead** (~12s for 16×64³ volumes). Acceptable for interactive analysis, but may be prohibitive for real-time streaming.

- **Nonlinear scaling can be disorienting** initially. The scaling widget is essential for helping users understand the transformation.

- **Memory requirements grow linearly** with ensemble size. Segment tree doubles storage per volume (n leaf + n internal nodes).

## Research Applications

### 1. Industrial X-ray Computed Tomography

**Problem**: Comparing reconstruction algorithms (FDK, SIRT, iterative methods) and parameter variations to optimize quality vs. computation time.

**Application**:
- Foam material analysis: determining cell wall thickness and porosity
- Composite materials: detecting delamination and cracks
- Additive manufacturing: quality control of 3D-printed parts
- Multi-modal XCT: comparing AC, DPC, DFC reconstructions

**Specific Use Cases**:
- Parameter optimization: finding minimal SIRT iterations for convergence
- Artifact detection: identifying ring artifacts, beam hardening, scatter
- Material characterization: isolating structures based on reconstruction stability

### 2. Medical Imaging

**Problem**: Comparing MRI or CT scans across different acquisition protocols, reconstruction methods, or time series.

**Application**:
- Brain atlas construction: identifying anatomical consensus and variation
- Tumor monitoring: detecting changes across time series
- Protocol optimization: comparing SNR, contrast, resolution trade-offs
- Multi-parametric MRI: comparing T1, T2, DWI, etc.

**Extension from Paper**: Contour boxplots and surface boxplots (Genton et al. 2014, Raj et al. 2016) for 3D medical imaging ensembles.

### 3. Climate and Weather Simulation

**Problem**: Comparing ensemble forecasts from different models or parameter perturbations.

**Application**:
- Ensemble-Vis framework (Potter et al. 2009): meteorological outcome discovery
- Temperature/pressure field comparison across simulation runs
- Uncertainty quantification: identifying regions of high model disagreement
- Outlier detection: finding anomalous ensemble members

**Volume Analogy**: 3D atmospheric data (latitude, longitude, altitude) with temperature, pressure, humidity as scalar fields.

### 4. Computational Fluid Dynamics

**Problem**: Comparing simulation results across different turbulence models, mesh resolutions, or boundary conditions.

**Application**:
- Flow field visualization: comparing velocity magnitude volumes
- Turbulence analysis: identifying regions sensitive to model choice
- Mesh convergence studies: determining optimal resolution
- Parameter sensitivity: quantifying impact of Reynolds number, etc.

**Ensemble Generation**: Parametric sweeps over simulation parameters, similar to XCT reconstruction parameters.

### 5. Material Science Simulation

**Problem**: Comparing molecular dynamics simulations or finite element analyses with different force fields, boundary conditions, or time steps.

**Application**:
- Stress distribution analysis in polymer composites
- Crystal structure stability across different potentials
- Crack propagation sensitivity to mesh size
- Thermal conductivity variation with microstructure

**Volume Data**: 3D scalar fields of stress, strain, temperature, or density.

### 6. Seismic Data Analysis

**Problem**: Comparing 3D seismic volumes from different processing workflows or inversion methods.

**Application**:
- Velocity model building: comparing full-waveform inversion results
- Migration algorithm comparison: Kirchhoff vs. RTM vs. beam migration
- Parameter sensitivity: analyzing impact of smoothing, filtering
- Reservoir characterization: identifying stable vs. unstable features

**Sparse Data**: Seismic data is often sparse; nonlinear scaling handles background efficiently.

### 7. Astronomy and Cosmology

**Problem**: Comparing 3D cosmological simulations or datacubes from radio interferometry.

**Application**:
- N-body simulation comparison: different dark matter models
- HI datacubes: comparing different cleaning algorithms (RFI removal)
- Source extraction: identifying robust detections across methods
- Redshift space distortion analysis: comparing observation and simulation

**Volume Type**: 3D datacubes (RA, Dec, frequency/velocity) with intensity as scalar.

### 8. Volumetric Video and 4D Imaging

**Problem**: Comparing 3D reconstructions from multi-view video or dynamic CT/MRI.

**Application**:
- Multi-view stereo comparison: different reconstruction algorithms
- Temporal consistency analysis: identifying stable structures over time
- Compression quality assessment: comparing lossy codecs
- Dynamic CT: cardiac imaging across different phases

**Extension**: Adding temporal dimension (4D) by treating time as additional ensemble axis or using animation.

### 9. Spatial Data Similarity Search

**Problem**: Finding similar volumes in large databases (e.g., protein structures, medical image repositories).

**Application**:
- Protein structure alignment: comparing 3D electron density maps
- Medical image retrieval: finding similar pathologies
- 3D shape matching: comparing CAD models or scanned objects
- Database indexing: using Hilbert indices for spatial indexing

**Connection to CountingGloBiMap**: Bloom filters for approximate similarity search with Hilbert linearization.

### 10. Probabilistic Data Structure Validation

**Problem**: Comparing estimation quality across different bloom filter configurations or implementations.

**Application**:
- Parameter sensitivity: how k (hash functions) affects estimation error
- Implementation comparison: Spectral BF vs. d-Left CBF vs. Count-Min Sketch
- Spatial error patterns: identifying regions of systematic over/undercounting
- Configuration optimization: finding minimal memory for target accuracy

**Direct Relevance**: This is the most relevant application for CountingGloBiMap, as described in the integration points section.

### Cross-Domain Pattern

All applications share common characteristics:
1. **Ensemble data**: Multiple 3D volumes to compare
2. **Subtle differences**: Variations that are difficult to detect with traditional methods
3. **Parameter-driven**: Ensembles generated by varying algorithm parameters
4. **Need for quantitative comparison**: Requires precise difference measurement
5. **Spatial structure importance**: Identifying where differences occur matters

The Dynamic Volume Lines framework is applicable wherever these characteristics are present.

## Implementation Reference

### For CountingGloBiMap Project

**Direct Integration Points**:

1. **Hilbert Curve Justification** (`/home/moritz/workspace/counting-globimaps/papers/3d_hilbert/`):
   - Reference this paper when documenting Hilbert curve choice
   - Cite Figure 4's empirical comparison with scan-line traversal
   - Note Moon et al. (2001) citation on clustering properties

2. **Visualization Enhancement** (`/home/moritz/workspace/counting-globimaps/notebooks/`):
   - Adapt histogram heatmap for count estimate visualization
   - Implement nonlinear scaling based on estimation error variation
   - Add functional boxplots for ensemble comparison

3. **Experimental Analysis** (`/home/moritz/workspace/counting-globimaps/experiments/src/`):
   - Enhance `globimap_test_k_compare.cpp` with importance-driven visualization
   - Add ensemble variation metric to `compare_all_implementations.cpp`
   - Implement segment tree for efficient spatial aggregation queries

4. **Documentation** (`/home/moritz/workspace/counting-globimaps/docs/`):
   - Add this paper to bibliography for spatial indexing justification
   - Reference nonlinear scaling approach for sparse data visualization
   - Cite functional boxplot methodology for ensemble statistics

**Code Concepts to Implement**:

```cpp
// Local importance function for bloom filter ensemble
class EnsembleImportanceFunction {
private:
    std::vector<CountingGloBiMap<>*> filters;
    std::vector<double> local_importance;
    std::vector<double> cumulative_importance;
    double p;  // emphasis parameter

public:
    EnsembleImportanceFunction(const std::vector<CountingGloBiMap<>*>& ensemble, double p=1.5)
        : filters(ensemble), p(p) {
        compute_importance();
    }

    void compute_importance() {
        size_t n = filters[0]->get_num_indices();
        local_importance.resize(n);
        cumulative_importance.resize(n);

        // Compute local variation at each Hilbert index
        double max_variation = 0;
        for (size_t h = 0; h < n; ++h) {
            auto coords = hilbert_to_coords(h);
            uint64_t max_count = 0, min_count = UINT64_MAX;
            for (auto* filter : filters) {
                uint64_t count = filter->get_min(coords);
                max_count = std::max(max_count, count);
                min_count = std::min(min_count, count);
            }
            double variation = static_cast<double>(max_count - min_count);
            local_importance[h] = variation;
            max_variation = std::max(max_variation, variation);
        }

        // Normalize and apply exponent
        for (size_t h = 0; h < n; ++h) {
            if (local_importance[h] == 0) {
                local_importance[h] = 0.025;  // Background
            } else {
                local_importance[h] = std::pow(local_importance[h] / max_variation, p);
            }
        }

        // Compute cumulative importance
        cumulative_importance[0] = local_importance[0];
        for (size_t h = 1; h < n; ++h) {
            cumulative_importance[h] = cumulative_importance[h-1] + local_importance[h];
        }
    }

    // Map Hilbert index to nonlinearly scaled position
    double get_scaled_position(size_t hilbert_idx) const {
        return cumulative_importance[hilbert_idx];
    }

    // Get local importance at Hilbert index
    double get_local_importance(size_t hilbert_idx) const {
        return local_importance[hilbert_idx];
    }
};
```

**Visualization Integration**:

```python
# In notebooks/utils.py or new visualization module
import numpy as np
import matplotlib.pyplot as plt
from matplotlib.colors import LinearSegmentedColormap

def plot_ensemble_heatmap(filters, coords, n_bins=64, colormap='extended_blackbody'):
    """
    Create histogram heatmap visualization for bloom filter ensemble.

    Args:
        filters: List of CountingGloBiMap instances
        coords: List of (x, y, z) coordinates to query
        n_bins: Number of histogram bins
        colormap: Color map name
    """
    # Compute counts for all filters at all coordinates
    counts = np.zeros((len(filters), len(coords)))
    for i, filter in enumerate(filters):
        for j, coord in enumerate(coords):
            counts[i, j] = filter.get_min(coord)

    # Compute histogram for each coordinate
    max_count = counts.max()
    histograms = []
    for j in range(len(coords)):
        hist, _ = np.histogram(counts[:, j], bins=n_bins, range=(0, max_count))
        histograms.append(hist)

    # Plot as heatmap
    heatmap = np.array(histograms).T
    plt.imshow(heatmap, aspect='auto', cmap=colormap, origin='lower')
    plt.colorbar(label='Frequency')
    plt.xlabel('Hilbert Index')
    plt.ylabel('Count Bin')
    plt.title('Ensemble Count Distribution')

def plot_functional_boxplot(filters, coords):
    """
    Create functional boxplot of count estimates across ensemble.
    """
    counts = np.zeros((len(filters), len(coords)))
    for i, filter in enumerate(filters):
        for j, coord in enumerate(coords):
            counts[i, j] = filter.get_min(coord)

    median = np.median(counts, axis=0)
    q25 = np.percentile(counts, 25, axis=0)
    q75 = np.percentile(counts, 75, axis=0)
    iqr = q75 - q25
    lower_whisker = np.minimum(q25 - 1.5*iqr, counts.min(axis=0))
    upper_whisker = np.maximum(q75 + 1.5*iqr, counts.max(axis=0))

    x = np.arange(len(coords))
    plt.fill_between(x, q25, q75, alpha=0.3, label='IQR')
    plt.plot(x, median, 'k-', linewidth=2, label='Median')
    plt.plot(x, lower_whisker, 'b-', label='Lower Whisker')
    plt.plot(x, upper_whisker, 'r-', label='Upper Whisker')
    plt.xlabel('Hilbert Index')
    plt.ylabel('Count Estimate')
    plt.legend()
    plt.title('Functional Boxplot of Count Estimates')
```

**References to Add to Documentation**:

```bibtex
@article{weissenboeck2018dynamic,
  title={Dynamic Volume Lines: Visual Comparison of 3D Volumes through Space-filling Curves},
  author={Weissenb{\"o}ck, Johannes and Fr{\"o}hler, Bernhard and Gr{\"o}ller, Eduard and Kastner, Johann and Heinzl, Christoph},
  journal={IEEE Transactions on Visualization and Computer Graphics},
  year={2018},
  doi={10.1109/TVCG.2018.2864510}
}

@inproceedings{hamilton2007compact,
  title={Compact Hilbert Indices for Multi-Dimensional Data},
  author={Hamilton, Chris H and Rau-Chaplin, Andrew},
  booktitle={International Conference on Complex, Intelligent and Software Intensive Systems},
  pages={139--146},
  year={2007}
}

@article{moon2001analysis,
  title={Analysis of the Clustering Properties of the Hilbert Space-Filling Curve},
  author={Moon, Bongki and Jagadish, HV and Faloutsos, Christos and Saltz, Joel H},
  journal={IEEE Transactions on Knowledge and Data Engineering},
  volume={13},
  number={1},
  pages={124--141},
  year={2001}
}
```

**Future Work Directions**:

1. Implement segment tree for efficient spatial aggregation in multi-resolution queries
2. Develop importance-driven visualization for sparse spatial datasets (GDELT, COVID-19)
3. Apply functional boxplots to multi-category experimental results
4. Create ensemble variation metric for automatic detection of systematic estimation errors
5. Extend to 4D (spatial + time) for temporal event analysis with nonlinear time scaling

This paper provides strong theoretical and empirical support for the Hilbert curve approach used in CountingGloBiMap, while offering practical visualization techniques that could significantly enhance the project's analysis capabilities for ensemble comparison and quality assessment.
