/**
 * TypeScript type definitions for CBF WASM module
 */

/**
 * Filter type identifiers (matches C++ enum)
 */
export enum FilterType {
  COUNT_MIN_SKETCH = 0,
  SPECTRAL_BF = 1,
  COUNTING_GLOBIMAP = 2,
  DLEFT_CBF = 3,
  UNKNOWN = 255,
}

/**
 * Filter type names for display
 */
export const FilterTypeNames: Record<FilterType, string> = {
  [FilterType.COUNT_MIN_SKETCH]: 'Count-Min Sketch',
  [FilterType.SPECTRAL_BF]: 'Spectral Bloom Filter',
  [FilterType.COUNTING_GLOBIMAP]: 'CountingGloBiMap',
  [FilterType.DLEFT_CBF]: 'd-Left Counting BF',
  [FilterType.UNKNOWN]: 'Unknown',
};

/**
 * Configuration for Count-Min Sketch
 */
export interface CMSConfig {
  epsilon: number;
  delta: number;
  conservative: boolean;
  counterBits: number;
}

/**
 * Filter configuration (union of all filter configs)
 */
export interface FilterConfig {
  type: FilterType;
  cms?: CMSConfig;
  // Add other filter configs as needed
}

/**
 * Filter metadata
 */
export interface FilterMetadata {
  type: FilterType;
  typeName: string;
  memoryUsage: number;  // Converted from WASM BigInt to Number
  info: string;
}

/**
 * Query result
 */
export interface QueryResult {
  count: number;
  queryTime: number; // milliseconds
}

/**
 * Point in the filter (2D or 3D with category)
 */
export interface Point {
  x: number;
  y: number;
  category?: number;
}

/**
 * Dataset metadata
 */
export interface DatasetMetadata {
  name: string;
  description: string;
  url: string;
  size: number;
  categories?: Array<{
    id: number;
    name: string;
    description: string;
  }>;
}
