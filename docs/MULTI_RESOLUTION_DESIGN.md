# Multi-Resolution CBF: Level-of-Detail Spatial Index Design

**Version**: 1.0
**Date**: 2025-11-19
**Status**: Vision Document

---

## 1. Vision

Build a hierarchical counting bloom filter system with **17 resolution levels** (1m to 65km) that enables:
- **Zoom-adaptive querying**: Automatically select appropriate resolution based on map zoom
- **Fast global overview**: Aggregate high-level statistics without scanning billions of points
- **Smooth transitions**: Seamless visual experience when zooming in/out
- **Massive scalability**: Handle billion-point datasets efficiently

## 2. Use Cases

### Real-World Scenarios

| Zoom Level | Cell Size | Use Case | Query Time Target |
|------------|-----------|----------|-------------------|
| 0-4 | 16-65 km | Global overview, country-level patterns | < 10ms |
| 5-8 | 1-8 km | City-level hotspot detection | < 50ms |
| 9-12 | 64-512 m | Neighborhood analysis, street-level | < 200ms |
| 13-16 | 4-32 m | Building clusters, precise locations | < 500ms |
| 17+ | 1-2 m | High-precision individual events | < 1s |

### Example: GDELT Global Events
- **Level 0 (65km)**: See major conflict zones (Middle East, Ukraine)
- **Level 8 (256m)**: Zoom to Kyiv, see event density per district
- **Level 16 (1m)**: Individual protest locations, embassy addresses

---

## 3. Data Structure

### Hierarchical Grid Pyramid

```
Level 0:  2^0m  cells  (65,536m = 65km)  →  360×180 grid     (global)
Level 4:  2^4m  cells  (16m)             →  1,440×720
Level 8:  2^8m  cells  (256m)            →  5,760×2,880
Level 12: 2^12m cells  (4,096m = 4km)    →  23,040×11,520
Level 16: 2^16m cells  (65,536m = 65km)  →  92,160×46,080    (fine)
```

Each level is a **complete 2D grid** covering the globe at different resolutions.

### Storage Options

**Option A: Separate CBF per Level**
```
dataset_level_00.cbf  (360×180,    ~5 KB)
dataset_level_04.cbf  (1.4K×720,   ~20 KB)
dataset_level_08.cbf  (5.7K×2.8K,  ~300 KB)
dataset_level_12.cbf  (23K×11.5K,  ~5 MB)
dataset_level_16.cbf  (92K×46K,    ~80 MB)
```

**Option B: Unified Multi-Level File**
```
dataset.cbfml
├── Header: Level metadata, offsets
├── Level 0 data
├── Level 4 data
├── ...
└── Level 16 data
```

**Trade-off**: Separate files allow lazy loading (only download needed levels), unified file simplifies deployment.

---

## 4. Aggregation Strategy

### Bottom-Up Construction

1. **Build finest level** (Level 16): Insert all raw points
2. **Aggregate to Level 15**: For each 2×2 cell block, sum counts → insert into Level 15
3. **Repeat** up to Level 0

### Aggregation Function

**Sum** (default): Higher levels contain total count of all child cells
```
Level 8 cell (10, 20) = SUM of Level 9 cells (20-21, 40-41)
```

**Max** (alternative): Track maximum hotspot intensity
```
Level 8 cell (10, 20) = MAX of Level 9 cells (20-21, 40-41)
```

**Average** (for density): Normalize by cell count
```
Level 8 cell (10, 20) = AVG of Level 9 cells / 4
```

---

## 5. Query API

### Current (Single-Resolution)
```typescript
filter.query({ x: 1800, y: 900, category: 1 })
```

### Proposed (Multi-Resolution)
```typescript
interface MultiResQuery {
  lat: number;
  lng: number;
  level: number;        // 0-16 (or auto from zoom)
  category?: number;
  aggregation?: 'sum' | 'max' | 'avg';
}

filter.queryAtLevel({ lat: 40.7, lng: -74.0, level: 12 })
// → Returns count for 4km×4km cell containing NYC
```

### Auto-Level Selection
```typescript
function selectLevel(zoom: number): number {
  // Zoom 0-4  → Level 0-2   (country/state)
  // Zoom 5-8  → Level 6-9   (city)
  // Zoom 9-12 → Level 10-13 (neighborhood)
  // Zoom 13+  → Level 14-16 (street)
  return Math.min(16, Math.max(0, zoom * 1.2));
}
```

---

## 6. Implementation Phases

### Phase 1: Foundation (Current Sprint)
- ✅ Single-resolution with proper quantization
- ✅ Grid metadata in JSON sidecar
- ✅ Correct cell boundary rendering
- 🎯 **Goal**: Solid baseline, no gaps/overlaps

### Phase 2: Dual-Resolution Prototype
- Add Level 8 (256m) and Level 12 (4km) to encoder
- Implement level selection in frontend
- Test performance: Does coarse level query 100x faster?
- **Validation**: Prove concept before full build

### Phase 3: Full Pyramid (17 Levels)
- Encoder: Build all 17 levels from raw data
- Storage: Choose separate vs. unified format
- Frontend: Auto-level selection based on zoom
- **Challenge**: Manage ~100x storage increase

### Phase 4: Optimization
- Lazy loading: Fetch levels on-demand
- Level caching: Keep coarse levels in memory
- Smooth transitions: Blend between levels during zoom
- Progressive enhancement: Show coarse → refine to fine

---

## 7. Technical Challenges

### Storage Overhead
- **Problem**: 17 levels ≈ 1.33x raw data size (geometric series)
- **Solution**: Compress coarse levels (fewer non-zero cells), lazy load

### Query Routing
- **Problem**: Which level to query at zoom X?
- **Solution**: Heuristic table + user override

### Aggregation Semantics
- **Problem**: SUM vs DENSITY (counts per area)
- **Solution**: Store both? Or calculate density dynamically from sum

### Boundary Alignment
- **Problem**: Cells at level N don't align with level N+1
- **Solution**: Each level is independent, no alignment needed (power-of-2 sizes)

### Transition Smoothness
- **Problem**: Jarring change when switching levels
- **Solution**: Brief fade, or blend between levels for 1 second

---

## 8. Related Work & Inspiration

### Quadtrees
- Standard hierarchical spatial structure
- Our approach: Pre-computed levels (faster) vs. dynamic subdivision

### Google S2
- Hierarchical hexagonal/square cells
- Lesson: Consistent cell IDs across levels

### Mapbox Vector Tiles
- Pre-rendered tiles at multiple zooms
- Lesson: Level-of-detail is essential for web maps

### H3 (Uber's Hexagonal Index)
- 16 resolution levels, hexagonal cells
- Advantage: No edge effects, uniform neighbors
- Our approach: Square grid (simpler), Mercator-aligned

---

## 9. Success Metrics

### Performance Goals
- **Level 0-4 query**: < 10ms (global overview)
- **Level 8-12 query**: < 100ms (city view)
- **Level 16 query**: < 500ms (street view)
- **Level switching**: < 200ms (seamless zoom)

### Accuracy Goals
- **Coarse levels**: Within 5% of true sum
- **Fine levels**: < 1% error (same as current)

### User Experience Goals
- **No visible gaps** during zoom transitions
- **Instant global overview** on dataset load
- **Smooth zooming** without lag or flicker

---

## 10. Next Steps

### Immediate Actions (Post Phase 1)
1. Implement dual-resolution encoder (Levels 8 & 12)
2. Measure query performance improvement
3. Design .cbfml file format spec
4. Prototype auto-level selection algorithm

### Research Questions
- Optimal number of levels? (17 vs. 12 vs. 24)
- Cell overlap strategy for smoother visuals?
- Hybrid approach: CBF for coarse, raw data for fine?

### Open Decisions
- Storage format: Separate files or unified?
- Aggregation: Sum, max, avg, or all three?
- Fetch strategy: Eager load all vs. lazy per level?

---

## 11. Conclusion

A multi-resolution CBF transforms the current system from a **fixed-grid snapshot** to a **scalable spatial database**. The hierarchical structure enables:
- Fast exploration at any scale
- Efficient storage of massive datasets
- Smooth, responsive user experience

**Investment**: Moderate complexity, significant payoff for large-scale applications.

**Timeline**: Phase 2 prototype feasible in 1-2 weeks, full implementation 4-6 weeks.
