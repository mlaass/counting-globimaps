/**
 * Dataset Service
 *
 * Loads dataset manifest, .cbf files, and metadata
 */

import { Dataset, DatasetManifest, CBFMetadata } from '../types/dataset';
import { CBFFilter } from '../wasm/loader';

/**
 * Load dataset manifest
 */
export async function loadManifest(): Promise<DatasetManifest> {
  const response = await fetch('/datasets/manifest.json');
  if (!response.ok) {
    throw new Error('Failed to load dataset manifest');
  }
  return response.json();
}

/**
 * Load CBF metadata from .cbf.json sidecar file
 */
export async function loadMetadata(filename: string): Promise<CBFMetadata> {
  const metadataUrl = `/datasets/${filename}.json`;
  const response = await fetch(metadataUrl);

  if (!response.ok) {
    throw new Error(`Failed to load metadata: ${metadataUrl}`);
  }

  const metadata = await response.json();

  // Validate metadata structure
  if (!metadata.version || !metadata.grid || !metadata.filter) {
    throw new Error('Invalid metadata format');
  }

  return metadata;
}

/**
 * Load a dataset's CBF file and validate against metadata
 */
export async function loadDataset(dataset: Dataset): Promise<CBFFilter> {
  const url = `/datasets/${dataset.filename}`;

  // Load metadata first to validate grid dimensions
  try {
    const metadata = await loadMetadata(dataset.filename);

    // Validate that dataset bounds match metadata
    if (metadata.grid.width !== dataset.bounds.gridWidth ||
        metadata.grid.height !== dataset.bounds.gridHeight) {
      console.warn('Grid dimensions mismatch between manifest and metadata:', {
        manifest: { width: dataset.bounds.gridWidth, height: dataset.bounds.gridHeight },
        metadata: { width: metadata.grid.width, height: metadata.grid.height }
      });
    }

    console.log('Loaded metadata:', metadata);
  } catch (err) {
    console.warn('Failed to load metadata, using manifest values:', err);
  }

  return await CBFFilter.fromURL(url);
}
