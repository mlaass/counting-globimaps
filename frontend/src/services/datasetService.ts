/**
 * Dataset Service
 *
 * Loads dataset manifest and .cbf files
 */

import { Dataset, DatasetManifest } from '../types/dataset';
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
 * Load a dataset's CBF file
 */
export async function loadDataset(dataset: Dataset): Promise<CBFFilter> {
  const url = `/datasets/${dataset.filename}`;
  return await CBFFilter.fromURL(url);
}
