/**
 * Dataset-related type definitions
 */

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
  lat: number;
  lng: number;
  value: number;
  category?: number;
}

export interface ScanProgress {
  current: number;
  total: number;
  percentage: number;
}
