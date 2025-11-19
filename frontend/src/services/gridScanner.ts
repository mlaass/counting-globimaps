/**
 * Grid Scanner Service
 *
 * Scans a geographic bounding box and queries the CBF filter at regular grid intervals
 * to generate density data for visualization.
 */

import { CBFFilter } from '../wasm/loader';
import { DatasetBounds, DensityPoint, ScanProgress } from '../types/dataset';
import {
  geoBoundsToGrid,
  gridToLatLng,
  estimateQueryCount,
} from '../utils/coordinates';

export interface ScanConfig {
  bounds: {
    north: number;
    south: number;
    east: number;
    west: number;
  };
  resolution: number;
  categories?: number[];
  datasetBounds: DatasetBounds;
  filter: CBFFilter;
  onProgress?: (progress: ScanProgress) => void;
}

export interface ScanResult {
  points: DensityPoint[];
  resolution: number;
  stats: {
    totalQueries: number;
    nonZeroPoints: number;
    minValue: number;
    maxValue: number;
    avgValue: number;
    scanTimeMs: number;
  };
}

/**
 * Scan a geographic region and query the CBF at regular intervals
 *
 * @param config Scan configuration
 * @returns Density points and statistics
 */
export async function scanGrid(config: ScanConfig): Promise<ScanResult> {
  const startTime = performance.now();

  const {
    bounds,
    resolution,
    categories,
    datasetBounds,
    filter,
    onProgress,
  } = config;

  // Convert geographic bounds to grid coordinates
  const gridBounds = geoBoundsToGrid(
    bounds.north,
    bounds.south,
    bounds.east,
    bounds.west,
    datasetBounds
  );

  // Calculate total queries
  const totalQueries = estimateQueryCount(gridBounds, resolution);

  const points: DensityPoint[] = [];
  let queriesDone = 0;
  let nonZeroPoints = 0;
  let totalValue = 0;
  let minValue = Infinity;
  let maxValue = -Infinity;

  // Determine which categories to query
  const categoriesToQuery = categories || [undefined];

  // Align grid bounds to resolution multiples
  // This ensures queries always hit the same grid coordinates (e.g., 0, 10, 20, ...)
  // regardless of viewport position, matching how data was encoded
  const alignedMinX = Math.floor(gridBounds.minX / resolution) * resolution;
  const alignedMaxX = Math.ceil(gridBounds.maxX / resolution) * resolution;
  const alignedMinY = Math.floor(gridBounds.minY / resolution) * resolution;
  const alignedMaxY = Math.ceil(gridBounds.maxY / resolution) * resolution;

  // Scan the grid with aligned coordinates
  for (let x = alignedMinX; x <= alignedMaxX; x += resolution) {
    for (let y = alignedMinY; y <= alignedMaxY; y += resolution) {
      // Query each category
      for (const category of categoriesToQuery) {
        const result = filter.query({
          x,
          y,
          category: category,
        });

        if (result.count > 0) {
          // Convert grid coordinates to cell center for display
          const { lat, lng } = gridToLatLng(x, y, datasetBounds, true);

          points.push({
            lat,
            lng,
            gridX: x,           // Store original grid coordinate
            gridY: y,           // Store original grid coordinate
            resolution,         // Store resolution used
            value: result.count,
            category,
          });

          nonZeroPoints++;
          totalValue += result.count;
          minValue = Math.min(minValue, result.count);
          maxValue = Math.max(maxValue, result.count);
        }

        queriesDone++;
      }

      // Report progress every 100 queries
      if (onProgress && queriesDone % 100 === 0) {
        onProgress({
          current: queriesDone,
          total: totalQueries * categoriesToQuery.length,
          percentage: (queriesDone / (totalQueries * categoriesToQuery.length)) * 100,
        });
      }
    }
  }

  const scanTimeMs = performance.now() - startTime;

  return {
    points,
    resolution,
    stats: {
      totalQueries: queriesDone,
      nonZeroPoints,
      minValue: nonZeroPoints > 0 ? minValue : 0,
      maxValue: nonZeroPoints > 0 ? maxValue : 0,
      avgValue: nonZeroPoints > 0 ? totalValue / nonZeroPoints : 0,
      scanTimeMs,
    },
  };
}

/**
 * Scan with automatic resolution adjustment based on viewport size
 *
 * Ensures the number of queries stays within reasonable bounds.
 *
 * @param config Base configuration
 * @param maxQueries Maximum number of queries (default: 5000)
 * @returns Scan result with adjusted resolution
 */
export async function scanGridAuto(
  config: Omit<ScanConfig, 'resolution'>,
  maxQueries: number = 5000
): Promise<ScanResult> {
  const { bounds, datasetBounds } = config;

  // Convert to grid bounds
  const gridBounds = geoBoundsToGrid(
    bounds.north,
    bounds.south,
    bounds.east,
    bounds.west,
    datasetBounds
  );

  // Calculate grid dimensions
  const width = gridBounds.maxX - gridBounds.minX;
  const height = gridBounds.maxY - gridBounds.minY;
  const area = width * height;

  // Calculate optimal resolution to stay under maxQueries
  const optimalResolution = Math.max(1, Math.ceil(Math.sqrt(area / maxQueries)));

  console.log(`Auto resolution: ${optimalResolution} (area: ${area}, target: ${maxQueries} queries)`);

  const result = await scanGrid({
    ...config,
    resolution: optimalResolution,
  });

  return result;
}

/**
 * Scan multiple categories in parallel
 *
 * @param config Scan configuration
 * @returns Map of category ID to scan results
 */
export async function scanGridByCategory(
  config: Omit<ScanConfig, 'categories'>
): Promise<Map<number, ScanResult>> {
  const results = new Map<number, ScanResult>();

  // Scan all categories sequentially (could be parallelized if needed)
  const categories = [1, 2, 3, 4]; // GDELT categories

  for (const category of categories) {
    const result = await scanGrid({
      ...config,
      categories: [category],
    });

    results.set(category, result);
  }

  return results;
}
