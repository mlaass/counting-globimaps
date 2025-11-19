/**
 * Coordinate transformation utilities
 *
 * Converts between geographic coordinates (lat/lng) and grid coordinates (x/y)
 * for querying the CBF filter.
 */

export interface LatLng {
  lat: number;
  lng: number;
}

export interface GridPoint {
  x: number;
  y: number;
}

export interface GridBounds {
  minX: number;
  maxX: number;
  minY: number;
  maxY: number;
}

export interface DatasetBounds {
  minLat: number;
  maxLat: number;
  minLng: number;
  maxLng: number;
  gridWidth: number;
  gridHeight: number;
}

/**
 * Convert lat/lng to grid coordinates
 *
 * @param lat Latitude (-90 to 90)
 * @param lng Longitude (-180 to 180)
 * @param bounds Dataset bounds with grid dimensions
 * @returns Grid coordinates {x, y}
 */
export function latLngToGrid(
  lat: number,
  lng: number,
  bounds: DatasetBounds
): GridPoint {
  // Normalize longitude to [0, 360] range
  const normalizedLng = lng + 180;

  // Normalize latitude to [0, 180] range
  const normalizedLat = lat + 90;

  // Convert to grid coordinates
  const x = Math.floor(bounds.gridWidth * (normalizedLng / 360));
  const y = Math.floor(bounds.gridHeight * (normalizedLat / 180));

  // Clamp to grid bounds
  const clampedX = Math.max(0, Math.min(bounds.gridWidth - 1, x));
  const clampedY = Math.max(0, Math.min(bounds.gridHeight - 1, y));

  return { x: clampedX, y: clampedY };
}

/**
 * Convert grid coordinates to lat/lng
 *
 * @param x Grid X coordinate
 * @param y Grid Y coordinate
 * @param bounds Dataset bounds with grid dimensions
 * @param centered If true, return cell center; if false, return top-left corner (default: true)
 * @returns Geographic coordinates {lat, lng}
 */
export function gridToLatLng(
  x: number,
  y: number,
  bounds: DatasetBounds,
  centered: boolean = true
): LatLng {
  // Offset by 0.5 to get cell center instead of corner
  const xOffset = centered ? 0.5 : 0;
  const yOffset = centered ? 0.5 : 0;

  // Convert grid to normalized coordinates
  const lng = ((x + xOffset) / bounds.gridWidth) * 360 - 180;
  const lat = ((y + yOffset) / bounds.gridHeight) * 180 - 90;

  return { lat, lng };
}

/**
 * Convert geographic bounding box to grid bounds
 *
 * @param north Northern latitude
 * @param south Southern latitude
 * @param east Eastern longitude
 * @param west Western longitude
 * @param bounds Dataset bounds
 * @returns Grid bounds {minX, maxX, minY, maxY}
 */
export function geoBoundsToGrid(
  north: number,
  south: number,
  east: number,
  west: number,
  bounds: DatasetBounds
): GridBounds {
  const topLeft = latLngToGrid(north, west, bounds);
  const bottomRight = latLngToGrid(south, east, bounds);

  return {
    minX: Math.min(topLeft.x, bottomRight.x),
    maxX: Math.max(topLeft.x, bottomRight.x),
    minY: Math.min(topLeft.y, bottomRight.y),
    maxY: Math.max(topLeft.y, bottomRight.y),
  };
}

/**
 * Calculate grid resolution based on zoom level
 *
 * Higher zoom = finer resolution (smaller step)
 * Lower zoom = coarser resolution (larger step)
 *
 * @param zoom Leaflet zoom level (0-18)
 * @returns Grid step size (1 = query every cell, 10 = every 10th cell)
 */
export function getGridResolution(zoom: number): number {
  if (zoom <= 2) return 200;      // Very coarse: every 200 grid cells
  if (zoom <= 4) return 100;      // Coarse: every 100 cells
  if (zoom <= 6) return 50;       // Medium-coarse: every 50 cells
  if (zoom <= 8) return 20;       // Medium: every 20 cells
  if (zoom <= 10) return 10;      // Medium-fine: every 10 cells
  if (zoom <= 12) return 5;       // Fine: every 5 cells
  return 2;                       // Very fine: every 2 cells
}

/**
 * Estimate number of queries for a given bounds and resolution
 *
 * @param gridBounds Grid bounding box
 * @param resolution Grid step size
 * @returns Estimated number of queries
 */
export function estimateQueryCount(
  gridBounds: GridBounds,
  resolution: number
): number {
  const width = gridBounds.maxX - gridBounds.minX;
  const height = gridBounds.maxY - gridBounds.minY;

  const stepsX = Math.ceil(width / resolution);
  const stepsY = Math.ceil(height / resolution);

  return stepsX * stepsY;
}
