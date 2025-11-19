/**
 * GridSquareLayer Component
 *
 * Renders density data as colored grid squares on the map
 */

import { Rectangle } from 'react-leaflet';
import { LatLngBounds, LatLngBoundsExpression } from 'leaflet';
import {
  DensityPoint,
  DatasetBounds,
  RenderingSettings,
} from '../../types/dataset';
import {
  mapValueToColor,
  calculateVariableOpacity,
} from '../../utils/colorScale';
import { gridToLatLng } from '../../utils/coordinates';

interface GridSquareLayerProps {
  data: DensityPoint[];
  resolution: number;
  datasetBounds: DatasetBounds;
  renderingSettings: RenderingSettings;
  globalStats?: {
    minValue: number;
    maxValue: number;
  };
}

/**
 * Calculate the bounding box for a grid cell
 *
 * Given grid coordinates and resolution, calculates the full cell boundaries
 * Uses corner coordinates (not centered) to get precise rectangle boundaries
 */
function calculateCellBounds(
  gridX: number,
  gridY: number,
  resolution: number,
  datasetBounds: DatasetBounds
): LatLngBounds {
  // Cell covers [gridX, gridX+resolution) × [gridY, gridY+resolution)
  // Convert corners to lat/lng (use corner mode, not centered)
  const topLeft = gridToLatLng(gridX, gridY, datasetBounds, false);
  const bottomRight = gridToLatLng(gridX + resolution, gridY + resolution, datasetBounds, false);

  return new LatLngBounds(
    [topLeft.lat, topLeft.lng],
    [bottomRight.lat, bottomRight.lng]
  );
}

export const GridSquareLayer: React.FC<GridSquareLayerProps> = ({
  data,
  resolution,
  datasetBounds,
  renderingSettings,
  globalStats,
}) => {
  if (!data || data.length === 0) {
    return null;
  }

  // Determine min/max for color scaling
  let minValue: number;
  let maxValue: number;

  if (renderingSettings.autoAdjust) {
    // Auto-adjust: use min/max from current viewport data
    minValue = Math.min(...data.map((p) => p.value));
    maxValue = Math.max(...data.map((p) => p.value));
  } else if (globalStats) {
    // Use global dataset stats
    minValue = globalStats.minValue;
    maxValue = globalStats.maxValue;
  } else {
    // Fallback: use current data
    minValue = Math.min(...data.map((p) => p.value));
    maxValue = Math.max(...data.map((p) => p.value));
  }

  // Ensure we have a valid range
  if (minValue === maxValue) {
    maxValue = minValue + 1;
  }

  return (
    <>
      {data.map((point, index) => {
        // Calculate cell boundaries using grid coordinates (not lat/lng)
        // This eliminates round-trip conversion errors
        const bounds = calculateCellBounds(
          point.gridX,
          point.gridY,
          point.resolution,
          datasetBounds
        );

        // Map value to color
        const color = mapValueToColor(
          point.value,
          minValue,
          maxValue,
          renderingSettings.scaleType,
          renderingSettings.gradientPalette
        );

        // Calculate opacity
        const opacity = renderingSettings.opacityMode === 'variable'
          ? calculateVariableOpacity(
              point.value,
              minValue,
              maxValue,
              renderingSettings.scaleType
            )
          : renderingSettings.fixedOpacity / 100;

        // Define rectangle options
        const pathOptions = {
          fillColor: color,
          fillOpacity: opacity,
          color: renderingSettings.showBorders ? '#000000' : color,
          weight: renderingSettings.showBorders ? 1 : 0,
          opacity: renderingSettings.showBorders ? 0.3 : 0,
        };

        return (
          <Rectangle
            key={`${point.lat}-${point.lng}-${point.category || 0}-${index}`}
            bounds={bounds as LatLngBoundsExpression}
            pathOptions={pathOptions}
          />
        );
      })}
    </>
  );
};
