/**
 * Dataset-related type definitions
 */

import { ScaleType, GradientPalette } from '../utils/colorScale';

export interface Category {
  id: number;
  name: string;
  description: string;
  color: string;
}

export interface DatasetStats {
  totalEvents: number;
  encodedSize: number;
  compressionRatio: number;
}

export interface DatasetBounds {
  minLat: number;
  maxLat: number;
  minLng: number;
  maxLng: number;
  gridWidth: number;
  gridHeight: number;
}

export interface Dataset {
  id: string;
  name: string;
  description: string;
  filename: string;
  type: string;
  bounds: DatasetBounds;
  categories: Category[];
  stats: DatasetStats;
}

export interface DatasetManifest {
  datasets: Dataset[];
}

export interface DensityPoint {
  lat: number;      // Display position (cell center in geographic coordinates)
  lng: number;      // Display position (cell center in geographic coordinates)
  gridX: number;    // Original grid X coordinate (for accurate rectangle rendering)
  gridY: number;    // Original grid Y coordinate (for accurate rectangle rendering)
  resolution: number; // Grid step/resolution used for this query
  value: number;
  category?: number;
}

export interface ScanProgress {
  current: number;
  total: number;
  percentage: number;
}

export interface RenderingSettings {
  scaleType: ScaleType;
  gradientPalette: GradientPalette;
  autoAdjust: boolean;
  opacityMode: 'variable' | 'fixed';
  fixedOpacity: number; // 0-100
  showBorders: boolean;
  autoResolution: boolean;      // Auto-select resolution from zoom level
  manualResolution: number;     // User-selected resolution (used when autoResolution is false)
}

export interface CBFMetadata {
  version: string;
  grid: {
    width: number;
    height: number;
    resolution_degrees: number;
    bounds: {
      min_lat: number;
      max_lat: number;
      min_lng: number;
      max_lng: number;
    };
  };
  filter: {
    type: string;
    epsilon: number;
    delta: number;
    conservative: boolean;
    counter_bits: number;
    width: number;
    depth: number;
  };
  dataset: {
    total_points: number;
    encoded_date: string;
  };
}
