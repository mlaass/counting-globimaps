/**
 * DatasetSelector Component
 *
 * Dropdown to select which dataset to load
 */

import React, { useEffect, useState } from 'react';
import { Dataset } from '../../types/dataset';
import { loadManifest } from '../../services/datasetService';
import { Card, CardHeader, CardTitle, CardContent } from '../ui/card';
import { Button } from '../ui/button';

interface DatasetSelectorProps {
  onDatasetSelect: (dataset: Dataset) => void;
  loading?: boolean;
  selectedDataset?: Dataset;
}

export const DatasetSelector: React.FC<DatasetSelectorProps> = ({
  onDatasetSelect,
  loading,
  selectedDataset,
}) => {
  const [datasets, setDatasets] = useState<Dataset[]>([]);
  const [loadingManifest, setLoadingManifest] = useState(true);
  const [error, setError] = useState<string>('');

  useEffect(() => {
    loadManifest()
      .then((manifest) => {
        setDatasets(manifest.datasets);
        setLoadingManifest(false);
        // Auto-select first dataset
        if (manifest.datasets.length > 0 && !selectedDataset) {
          onDatasetSelect(manifest.datasets[0]);
        }
      })
      .catch((err) => {
        setError('Failed to load datasets');
        setLoadingManifest(false);
      });
  }, []);

  if (loadingManifest) {
    return (
      <Card>
        <CardHeader>
          <CardTitle className="text-lg">Dataset</CardTitle>
        </CardHeader>
        <CardContent>
          <p className="text-sm text-muted-foreground">Loading datasets...</p>
        </CardContent>
      </Card>
    );
  }

  if (error) {
    return (
      <Card>
        <CardHeader>
          <CardTitle className="text-lg">Dataset</CardTitle>
        </CardHeader>
        <CardContent>
          <p className="text-sm text-destructive">{error}</p>
        </CardContent>
      </Card>
    );
  }

  return (
    <Card>
      <CardHeader>
        <CardTitle className="text-lg">Dataset</CardTitle>
      </CardHeader>
      <CardContent className="space-y-4">
        {/* Simple list for now - could upgrade to Select component */}
        <div className="space-y-2">
          {datasets.map((dataset) => (
            <Button
              key={dataset.id}
              variant={selectedDataset?.id === dataset.id ? 'default' : 'outline'}
              className="w-full justify-start"
              onClick={() => onDatasetSelect(dataset)}
              disabled={loading}
            >
              <div className="flex flex-col items-start">
                <span className="font-medium">{dataset.name}</span>
                <span className="text-xs opacity-70">{dataset.description}</span>
              </div>
            </Button>
          ))}
        </div>

        {selectedDataset && (
          <div className="text-xs text-muted-foreground space-y-1">
            <div>
              <strong>Type:</strong> {selectedDataset.type}
            </div>
            <div>
              <strong>Events:</strong> {selectedDataset.stats.totalEvents.toLocaleString()}
            </div>
            <div>
              <strong>Size:</strong> {(selectedDataset.stats.encodedSize / 1024).toFixed(1)} KB
            </div>
            <div>
              <strong>Ratio:</strong> {selectedDataset.stats.compressionRatio}:1
            </div>
          </div>
        )}
      </CardContent>
    </Card>
  );
};
