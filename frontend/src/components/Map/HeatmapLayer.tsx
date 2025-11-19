/**
 * HeatmapLayer Component
 *
 * Renders density data as a heatmap overlay on the map
 */

import { useEffect } from 'react';
import { useMap } from 'react-leaflet';
import L from 'leaflet';
import 'leaflet.heat';
import { DensityPoint } from '../../types/dataset';

interface HeatmapLayerProps {
  data: DensityPoint[];
  options?: {
    radius?: number;
    blur?: number;
    maxZoom?: number;
    max?: number;
    gradient?: { [key: number]: string };
  };
}

export const HeatmapLayer: React.FC<HeatmapLayerProps> = ({ data, options }) => {
  const map = useMap();

  useEffect(() => {
    if (!data || data.length === 0) {
      return;
    }

    // Convert DensityPoint[] to format expected by leaflet.heat
    // Format: [lat, lng, intensity]
    const heatData: [number, number, number][] = data.map((point) => [
      point.lat,
      point.lng,
      point.value,
    ]);

    // Create heatmap layer
    const heatLayer = (L as any).heatLayer(heatData, {
      radius: options?.radius || 25,
      blur: options?.blur || 15,
      maxZoom: options?.maxZoom || 10,
      max: options?.max || 1.0,
      gradient: options?.gradient || {
        0.0: 'blue',
        0.2: 'cyan',
        0.4: 'lime',
        0.6: 'yellow',
        0.8: 'orange',
        1.0: 'red',
      },
    }).addTo(map);

    // Cleanup on unmount or data change
    return () => {
      map.removeLayer(heatLayer);
    };
  }, [data, map, options]);

  return null;
};
