/**
 * Main App Component
 *
 * CBF Dataset Explorer - WASM-based density visualization
 */

import React, { useState, useEffect, useCallback, useRef } from 'react';
import { LatLngBounds } from 'leaflet';
import { CBFFilter, preloadWasm } from './wasm/loader';
import { Dataset, DensityPoint } from './types/dataset';
import { loadDataset } from './services/datasetService';
import { scanGrid, ScanResult } from './services/gridScanner';
import { getGridResolution } from './utils/coordinates';
import { MapView } from './components/Map/MapView';
import { HeatmapLayer } from './components/Map/HeatmapLayer';
import { DatasetSelector } from './components/Sidebar/DatasetSelector';
import { CategoryControls } from './components/Sidebar/CategoryControls';
import { Statistics } from './components/Sidebar/Statistics';
import { FilterInfo } from './components/FilterInfo';
import './App.css';

function App() {
  // WASM and filter state
  const [wasmReady, setWasmReady] = useState(false);
  const [filter, setFilter] = useState<CBFFilter | null>(null);
  const [loading, setLoading] = useState(false);
  const [error, setError] = useState<string>('');

  // Dataset state
  const [selectedDataset, setSelectedDataset] = useState<Dataset | null>(null);
  const [selectedCategories, setSelectedCategories] = useState<number[]>([]);

  // Map state
  const [densityData, setDensityData] = useState<DensityPoint[]>([]);
  const [scanResult, setScanResult] = useState<ScanResult | null>(null);
  const [currentResolution, setCurrentResolution] = useState<number>(50);
  const [scanning, setScanning] = useState(false);

  // Debounce timer
  const scanTimerRef = useRef<NodeJS.Timeout | null>(null);

  // Preload WASM module on mount
  useEffect(() => {
    preloadWasm()
      .then(() => {
        console.log('WASM module preloaded');
        setWasmReady(true);
      })
      .catch((err) => {
        console.error('Failed to preload WASM:', err);
        setError('Failed to initialize WebAssembly module');
      });
  }, []);

  // Load dataset when selected
  const handleDatasetSelect = useCallback(
    async (dataset: Dataset) => {
      if (!wasmReady) return;

      setSelectedDataset(dataset);
      setLoading(true);
      setError('');

      try {
        // Clean up previous filter
        if (filter) {
          filter.free();
        }

        // Load the .cbf file
        const newFilter = await loadDataset(dataset);
        setFilter(newFilter);

        // Auto-select all categories
        const allCategories = dataset.categories.map((cat) => cat.id);
        setSelectedCategories(allCategories);

        console.log('Dataset loaded:', newFilter.getMetadata());
      } catch (err) {
        const errorMessage = err instanceof Error ? err.message : 'Failed to load dataset';
        console.error('Failed to load dataset:', err);
        console.error('Error details:', {
          message: errorMessage,
          stack: err instanceof Error ? err.stack : undefined,
          dataset: dataset.filename
        });
        setError(errorMessage);
      } finally {
        setLoading(false);
      }
    },
    [wasmReady, filter]
  );

  // Handle category toggle
  const handleCategoryToggle = useCallback(
    (categoryId: number) => {
      setSelectedCategories((prev) =>
        prev.includes(categoryId)
          ? prev.filter((id) => id !== categoryId)
          : [...prev, categoryId]
      );
    },
    []
  );

  // Perform grid scan
  const performScan = useCallback(
    async (bounds: LatLngBounds, zoom: number) => {
      if (!filter || !selectedDataset) return;

      // Auto-adjust resolution based on zoom
      const resolution = getGridResolution(zoom);
      setCurrentResolution(resolution);

      setScanning(true);

      try {
        const result = await scanGrid({
          bounds: {
            north: bounds.getNorth(),
            south: bounds.getSouth(),
            east: bounds.getEast(),
            west: bounds.getWest(),
          },
          resolution,
          categories: selectedCategories.length > 0 ? selectedCategories : undefined,
          datasetBounds: selectedDataset.bounds,
          filter,
        });

        setDensityData(result.points);
        setScanResult(result);
        console.log('Scan complete:', result.stats);
      } catch (err) {
        console.error('Scan failed:', err);
      } finally {
        setScanning(false);
      }
    },
    [filter, selectedDataset, selectedCategories]
  );

  // Handle map viewport change with debouncing
  const handleViewportChange = useCallback(
    (bounds: LatLngBounds, zoom: number) => {
      // Clear previous timer
      if (scanTimerRef.current) {
        clearTimeout(scanTimerRef.current);
      }

      // Debounce scan by 500ms
      scanTimerRef.current = setTimeout(() => {
        performScan(bounds, zoom);
      }, 500);
    },
    [performScan]
  );

  // Re-scan when categories change
  useEffect(() => {
    if (filter && selectedDataset && densityData.length > 0) {
      // Trigger a new scan with updated categories
      // We'll use the last scan bounds if available
      // For now, just clear the data - user will need to pan/zoom to re-trigger
      setDensityData([]);
      setScanResult(null);
    }
  }, [selectedCategories]);

  // Cleanup on unmount
  useEffect(() => {
    return () => {
      if (filter) {
        filter.free();
      }
      if (scanTimerRef.current) {
        clearTimeout(scanTimerRef.current);
      }
    };
  }, [filter]);

  return (
    <div className="min-h-screen bg-background flex flex-col">
      {/* Header */}
      <header className="border-b bg-card shadow-sm">
        <div className="container mx-auto px-4 py-4">
          <h1 className="text-2xl font-bold">🗺️ CBF Dataset Explorer</h1>
          <p className="text-sm text-muted-foreground">
            Explore spatial datasets with WebAssembly-powered density visualization
          </p>
        </div>
      </header>

      {/* Main Content */}
      <main className="flex-1 flex overflow-hidden">
        {/* Loading State */}
        {!wasmReady && (
          <div className="flex-1 flex items-center justify-center">
            <div className="text-center">
              <h2 className="text-xl font-semibold mb-2">⏳ Loading...</h2>
              <p className="text-muted-foreground">
                Initializing WebAssembly module
              </p>
            </div>
          </div>
        )}

        {/* Error State */}
        {error && (
          <div className="flex-1 flex items-center justify-center">
            <div className="text-center max-w-md">
              <h2 className="text-xl font-semibold mb-2 text-destructive">
                ❌ Error
              </h2>
              <p className="text-muted-foreground">{error}</p>
            </div>
          </div>
        )}

        {/* Main UI */}
        {wasmReady && !error && (
          <>
            {/* Map View */}
            <div className="flex-1 relative">
              <MapView onViewportChange={handleViewportChange}>
                {filter && <HeatmapLayer data={densityData} />}
              </MapView>

              {/* Scanning Indicator */}
              {scanning && (
                <div className="absolute top-4 left-1/2 transform -translate-x-1/2 bg-card border shadow-lg rounded-lg px-4 py-2 z-[1000]">
                  <p className="text-sm font-medium">Scanning grid...</p>
                </div>
              )}
            </div>

            {/* Sidebar */}
            <aside className="w-80 bg-background border-l overflow-y-auto">
              <div className="p-4 space-y-4">
                {/* Dataset Selector */}
                <DatasetSelector
                  onDatasetSelect={handleDatasetSelect}
                  loading={loading}
                  selectedDataset={selectedDataset || undefined}
                />

                {/* Category Controls */}
                {selectedDataset && selectedDataset.categories.length > 0 && (
                  <CategoryControls
                    categories={selectedDataset.categories}
                    selectedCategories={selectedCategories}
                    onCategoryToggle={handleCategoryToggle}
                    disabled={loading || scanning}
                  />
                )}

                {/* Statistics */}
                <Statistics
                  scanResult={scanResult || undefined}
                  currentResolution={currentResolution}
                  loading={scanning}
                />

                {/* Filter Info */}
                {filter && (
                  <div className="pt-4 border-t">
                    <FilterInfo filter={filter} />
                  </div>
                )}
              </div>
            </aside>
          </>
        )}
      </main>
    </div>
  );
}

export default App;
