/**
 * Main App Component
 *
 * CBF Dataset Explorer - WASM-based density visualization
 */

import React, { useState, useEffect, useCallback, useRef } from 'react';
import { LatLngBounds } from 'leaflet';
import { CBFFilter, preloadWasm } from './wasm/loader';
import { Dataset, DensityPoint, RenderingSettings } from './types/dataset';
import { loadDataset } from './services/datasetService';
import { scanGrid, ScanResult } from './services/gridScanner';
import { getGridResolution } from './utils/coordinates';
import { MapView } from './components/Map/MapView';
import { GridSquareLayer } from './components/Map/GridSquareLayer';
import { DatasetSelector } from './components/Sidebar/DatasetSelector';
import { CategoryControls } from './components/Sidebar/CategoryControls';
import { RenderingSettings as RenderingSettingsComponent } from './components/Sidebar/RenderingSettings';
import { Statistics } from './components/Sidebar/Statistics';
import { FilterInfo } from './components/FilterInfo';
import { Tabs } from './components/Sidebar/Tabs';
import { Legend } from './components/Legend';
import './App.css';

function App() {
  // WASM and filter state
  const [wasmReady, setWasmReady] = useState(false);
  const [filter, setFilter] = useState<CBFFilter | null>(null);
  const [loading, setLoading] = useState(false);
  const [error, setError] = useState<string>('');

  // Dataset state
  const [selectedDataset, setSelectedDataset] = useState<Dataset | null>(null);
  const [selectedCategory, setSelectedCategory] = useState<number | null>(null);

  // Map state
  const [densityData, setDensityData] = useState<DensityPoint[]>([]);
  const [scanResult, setScanResult] = useState<ScanResult | null>(null);
  const [currentResolution, setCurrentResolution] = useState<number>(50);
  const [scanning, setScanning] = useState(false);

  // Rendering settings
  const [renderingSettings, setRenderingSettings] = useState<RenderingSettings>({
    scaleType: 'linear',
    gradientPalette: 'heat',
    autoAdjust: false,
    opacityMode: 'variable',
    fixedOpacity: 60,
    showBorders: false,
    autoResolution: true,
    manualResolution: 10,
  });

  // Sidebar tab state
  const [activeTab, setActiveTab] = useState<string>('data');

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

        // Reset category selection to "All"
        setSelectedCategory(null);

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

  // Handle category selection
  const handleCategorySelect = useCallback(
    (categoryId: number | null) => {
      setSelectedCategory(categoryId);
    },
    []
  );

  // Perform grid scan
  const performScan = useCallback(
    async (bounds: LatLngBounds, zoom: number) => {
      if (!filter || !selectedDataset) return;

      // Determine resolution: auto-select from zoom or use manual setting
      const resolution = renderingSettings.autoResolution
        ? getGridResolution(zoom)
        : renderingSettings.manualResolution;
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
          categories: selectedCategory !== null ? [selectedCategory] : undefined,
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
    [filter, selectedDataset, selectedCategory, renderingSettings.autoResolution, renderingSettings.manualResolution]
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
  }, [selectedCategory]);

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
                {filter && selectedDataset && (
                  <GridSquareLayer
                    data={densityData}
                    resolution={scanResult?.resolution || currentResolution}
                    datasetBounds={selectedDataset.bounds}
                    renderingSettings={renderingSettings}
                    globalStats={
                      scanResult
                        ? {
                            minValue: scanResult.stats.minValue,
                            maxValue: scanResult.stats.maxValue,
                          }
                        : undefined
                    }
                  />
                )}
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
              <div className="p-4">
                {/* Tabs */}
                <Tabs
                  tabs={[
                    { id: 'data', label: 'Data' },
                    { id: 'rendering', label: 'Rendering' },
                  ]}
                  activeTab={activeTab}
                  onTabChange={setActiveTab}
                />

                {/* Tab Content */}
                <div className="space-y-4">
                  {activeTab === 'data' && (
                    <>
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
                          selectedCategory={selectedCategory}
                          onCategorySelect={handleCategorySelect}
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
                    </>
                  )}

                  {activeTab === 'rendering' && (
                    <>
                      {/* Rendering Settings */}
                      <RenderingSettingsComponent
                        settings={renderingSettings}
                        onChange={setRenderingSettings}
                        disabled={loading || scanning}
                      />

                      {/* Legend */}
                      {scanResult && scanResult.stats.nonZeroPoints > 0 && (
                        <Legend
                          minValue={scanResult.stats.minValue}
                          maxValue={scanResult.stats.maxValue}
                          scaleType={renderingSettings.scaleType}
                          gradientPalette={renderingSettings.gradientPalette}
                        />
                      )}
                    </>
                  )}
                </div>
              </div>
            </aside>
          </>
        )}
      </main>
    </div>
  );
}

export default App;
