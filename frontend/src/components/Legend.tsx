/**
 * Legend Component
 *
 * Displays color scale legend for grid square visualization
 */

import React from 'react';
import { Card, CardHeader, CardTitle, CardContent } from './ui/card';
import {
  mapValueToColor,
  ScaleType,
  GradientPalette,
} from '../utils/colorScale';

interface LegendProps {
  minValue: number;
  maxValue: number;
  scaleType: ScaleType;
  gradientPalette: GradientPalette;
}

export const Legend: React.FC<LegendProps> = ({
  minValue,
  maxValue,
  scaleType,
  gradientPalette,
}) => {
  // Generate color stops for the gradient bar
  const numStops = 100;
  const stops: { value: number; color: string; position: number }[] = [];

  for (let i = 0; i <= numStops; i++) {
    const position = i / numStops; // 0 to 1

    // Calculate value based on position and scale type
    let value: number;
    if (scaleType === 'linear') {
      value = minValue + (maxValue - minValue) * position;
    } else {
      // Logarithmic: convert position to log space
      const logMin = Math.log(minValue + 0.1);
      const logMax = Math.log(maxValue + 0.1);
      value = Math.exp(logMin + (logMax - logMin) * position) - 0.1;
    }

    const color = mapValueToColor(value, minValue, maxValue, scaleType, gradientPalette);
    stops.push({ value, color, position });
  }

  // Generate tick marks (6 evenly spaced)
  const numTicks = 6;
  const ticks: { value: number; label: string; position: number }[] = [];

  for (let i = 0; i < numTicks; i++) {
    const position = i / (numTicks - 1); // 0 to 1

    // Calculate value
    let value: number;
    if (scaleType === 'linear') {
      value = minValue + (maxValue - minValue) * position;
    } else {
      const logMin = Math.log(minValue + 0.1);
      const logMax = Math.log(maxValue + 0.1);
      value = Math.exp(logMin + (logMax - logMin) * position) - 0.1;
    }

    // Format label
    const label = value >= 1000
      ? `${(value / 1000).toFixed(1)}k`
      : value >= 100
      ? value.toFixed(0)
      : value.toFixed(1);

    ticks.push({ value, label, position: position * 100 });
  }

  return (
    <Card>
      <CardHeader>
        <CardTitle className="text-sm">Color Scale</CardTitle>
      </CardHeader>
      <CardContent>
        <div className="relative">
          {/* Gradient Bar */}
          <div
            className="h-6 w-full rounded"
            style={{
              background: `linear-gradient(to right, ${stops.map(s => s.color).join(', ')})`,
            }}
          />

          {/* Tick Marks */}
          <div className="relative mt-2 h-12">
            {ticks.map((tick, index) => (
              <div
                key={index}
                className="absolute"
                style={{ left: `${tick.position}%`, transform: 'translateX(-50%)' }}
              >
                {/* Tick mark line */}
                <div className="w-px h-2 bg-border mx-auto" />

                {/* Tick label */}
                <div className="text-xs text-muted-foreground mt-1 whitespace-nowrap">
                  {tick.label}
                </div>
              </div>
            ))}
          </div>

          {/* Scale type indicator */}
          <div className="text-xs text-muted-foreground text-center mt-2 border-t pt-2">
            {scaleType === 'linear' ? 'Linear Scale' : 'Logarithmic Scale'}
          </div>
        </div>
      </CardContent>
    </Card>
  );
};
