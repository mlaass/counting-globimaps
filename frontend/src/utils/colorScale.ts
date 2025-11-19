/**
 * Color Scale Utilities for Grid Square Visualization
 *
 * Provides linear and logarithmic color mapping with multiple gradient palettes.
 */

export type ScaleType = 'linear' | 'logarithmic';
export type GradientPalette = 'heat' | 'viridis' | 'plasma' | 'cool' | 'turbo';

/**
 * Color stop definition: [position, r, g, b]
 * Position is in range [0, 1]
 */
type ColorStop = [number, number, number, number];

/**
 * Gradient palette definitions
 * Each palette is an array of color stops for smooth interpolation
 */
const GRADIENTS: Record<GradientPalette, ColorStop[]> = {
  // Heat: white → yellow → orange → red
  heat: [
    [0.0, 255, 255, 255],   // White
    [0.25, 255, 255, 0],    // Yellow
    [0.5, 255, 165, 0],     // Orange
    [0.75, 255, 69, 0],     // Orange-red
    [1.0, 178, 34, 34],     // Dark red
  ],

  // Viridis: purple → blue → green → yellow (colorblind-friendly)
  viridis: [
    [0.0, 68, 1, 84],       // Dark purple
    [0.25, 59, 82, 139],    // Blue-purple
    [0.5, 33, 145, 140],    // Teal
    [0.75, 94, 201, 98],    // Green
    [1.0, 253, 231, 37],    // Yellow
  ],

  // Plasma: purple → pink → orange → yellow (colorblind-friendly)
  plasma: [
    [0.0, 13, 8, 135],      // Dark purple
    [0.25, 126, 3, 168],    // Purple
    [0.5, 204, 71, 120],    // Pink
    [0.75, 248, 149, 64],   // Orange
    [1.0, 240, 249, 33],    // Yellow
  ],

  // Cool: cyan → blue → purple
  cool: [
    [0.0, 0, 255, 255],     // Cyan
    [0.33, 0, 128, 255],    // Sky blue
    [0.67, 128, 0, 255],    // Blue-purple
    [1.0, 128, 0, 128],     // Purple
  ],

  // Turbo: blue → cyan → green → yellow → red (Google's perceptually uniform)
  turbo: [
    [0.0, 48, 18, 59],      // Dark blue
    [0.2, 0, 147, 233],     // Cyan
    [0.4, 0, 225, 131],     // Green
    [0.6, 233, 230, 59],    // Yellow
    [0.8, 253, 128, 4],     // Orange
    [1.0, 193, 0, 16],      // Red
  ],
};

/**
 * Normalize a value using linear scaling
 *
 * @param value The value to normalize
 * @param min Minimum value in range
 * @param max Maximum value in range
 * @returns Normalized value in [0, 1]
 */
export function linearScale(value: number, min: number, max: number): number {
  if (max === min) return 0.5; // Avoid division by zero
  return Math.max(0, Math.min(1, (value - min) / (max - min)));
}

/**
 * Normalize a value using logarithmic scaling
 *
 * Useful for data with wide ranges (e.g., 1 to 10000)
 *
 * @param value The value to normalize
 * @param min Minimum value in range (must be > 0)
 * @param max Maximum value in range
 * @returns Normalized value in [0, 1]
 */
export function logScale(value: number, min: number, max: number): number {
  if (max === min || min <= 0) return 0.5;

  // Add small epsilon to avoid log(0)
  const epsilon = 0.1;
  const logValue = Math.log(value + epsilon);
  const logMin = Math.log(min + epsilon);
  const logMax = Math.log(max + epsilon);

  return Math.max(0, Math.min(1, (logValue - logMin) / (logMax - logMin)));
}

/**
 * Interpolate between two colors
 *
 * @param color1 First color [r, g, b]
 * @param color2 Second color [r, g, b]
 * @param t Interpolation factor in [0, 1]
 * @returns Interpolated color [r, g, b]
 */
function interpolateColor(
  color1: [number, number, number],
  color2: [number, number, number],
  t: number
): [number, number, number] {
  return [
    Math.round(color1[0] + (color2[0] - color1[0]) * t),
    Math.round(color1[1] + (color2[1] - color1[1]) * t),
    Math.round(color1[2] + (color2[2] - color1[2]) * t),
  ];
}

/**
 * Apply gradient to a normalized value
 *
 * @param normalized Value in [0, 1]
 * @param palette Gradient palette name
 * @returns RGB color as [r, g, b]
 */
export function applyGradient(
  normalized: number,
  palette: GradientPalette
): [number, number, number] {
  const stops = GRADIENTS[palette];

  // Find the two color stops to interpolate between
  for (let i = 0; i < stops.length - 1; i++) {
    const [pos1, r1, g1, b1] = stops[i];
    const [pos2, r2, g2, b2] = stops[i + 1];

    if (normalized >= pos1 && normalized <= pos2) {
      // Interpolate between these two stops
      const t = (normalized - pos1) / (pos2 - pos1);
      return interpolateColor([r1, g1, b1], [r2, g2, b2], t);
    }
  }

  // If we get here, return the last color
  const lastStop = stops[stops.length - 1];
  return [lastStop[1], lastStop[2], lastStop[3]];
}

/**
 * Convert RGB to hex color string
 *
 * @param r Red component (0-255)
 * @param g Green component (0-255)
 * @param b Blue component (0-255)
 * @returns Hex color string (e.g., "#ff0000")
 */
export function rgbToHex(r: number, g: number, b: number): string {
  const toHex = (n: number) => {
    const hex = Math.round(n).toString(16);
    return hex.length === 1 ? '0' + hex : hex;
  };
  return `#${toHex(r)}${toHex(g)}${toHex(b)}`;
}

/**
 * Map a value to a color using the specified scale and palette
 *
 * @param value The value to map
 * @param min Minimum value in range
 * @param max Maximum value in range
 * @param scaleType Linear or logarithmic scaling
 * @param palette Color gradient palette
 * @returns Hex color string (e.g., "#ff0000")
 */
export function mapValueToColor(
  value: number,
  min: number,
  max: number,
  scaleType: ScaleType,
  palette: GradientPalette
): string {
  // Normalize the value based on scale type
  const normalized = scaleType === 'linear'
    ? linearScale(value, min, max)
    : logScale(value, min, max);

  // Apply gradient
  const [r, g, b] = applyGradient(normalized, palette);

  return rgbToHex(r, g, b);
}

/**
 * Calculate variable opacity based on normalized value
 *
 * Maps value to opacity range [0.3, 0.9] (30% to 90%)
 * Higher values are more opaque
 *
 * @param value The value
 * @param min Minimum value in range
 * @param max Maximum value in range
 * @param scaleType Linear or logarithmic scaling
 * @returns Opacity in [0.3, 0.9]
 */
export function calculateVariableOpacity(
  value: number,
  min: number,
  max: number,
  scaleType: ScaleType
): number {
  const normalized = scaleType === 'linear'
    ? linearScale(value, min, max)
    : logScale(value, min, max);

  return 0.3 + (normalized * 0.6); // Range: 30% to 90%
}

/**
 * Get display-friendly palette names
 */
export const PALETTE_NAMES: Record<GradientPalette, string> = {
  heat: 'Heat (White → Red)',
  viridis: 'Viridis (Purple → Yellow)',
  plasma: 'Plasma (Purple → Pink → Yellow)',
  cool: 'Cool (Cyan → Purple)',
  turbo: 'Turbo (Blue → Red)',
};
