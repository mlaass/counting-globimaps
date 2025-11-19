/**
 * RenderingSettings Component
 *
 * Controls for visualization rendering (colors, opacity, borders, etc.)
 */

import React from 'react';
import { Card, CardHeader, CardTitle, CardContent } from '../ui/card';
import { RenderingSettings as RenderingSettingsType } from '../../types/dataset';
import { ScaleType, GradientPalette, PALETTE_NAMES } from '../../utils/colorScale';

interface RenderingSettingsProps {
  settings: RenderingSettingsType;
  onChange: (settings: RenderingSettingsType) => void;
  disabled?: boolean;
}

export const RenderingSettings: React.FC<RenderingSettingsProps> = ({
  settings,
  onChange,
  disabled,
}) => {
  const updateSetting = <K extends keyof RenderingSettingsType>(
    key: K,
    value: RenderingSettingsType[K]
  ) => {
    onChange({ ...settings, [key]: value });
  };

  return (
    <div className="space-y-4">
      {/* Scale Type */}
      <Card>
        <CardHeader>
          <CardTitle className="text-lg">Color Scale</CardTitle>
        </CardHeader>
        <CardContent className="space-y-4">
          {/* Scale Type Selector */}
          <div>
            <label className="text-sm font-medium mb-2 block">
              Scale Type
            </label>
            <select
              value={settings.scaleType}
              onChange={(e) => updateSetting('scaleType', e.target.value as ScaleType)}
              disabled={disabled}
              className="w-full px-3 py-2 bg-background border border-input rounded-md text-sm focus:outline-none focus:ring-2 focus:ring-primary"
            >
              <option value="linear">Linear</option>
              <option value="logarithmic">Logarithmic</option>
            </select>
            <p className="text-xs text-muted-foreground mt-1">
              {settings.scaleType === 'linear'
                ? 'Even distribution across value range'
                : 'Emphasize differences in lower values (good for wide ranges)'}
            </p>
          </div>

          {/* Gradient Palette Selector */}
          <div>
            <label className="text-sm font-medium mb-2 block">
              Color Gradient
            </label>
            <select
              value={settings.gradientPalette}
              onChange={(e) =>
                updateSetting('gradientPalette', e.target.value as GradientPalette)
              }
              disabled={disabled}
              className="w-full px-3 py-2 bg-background border border-input rounded-md text-sm focus:outline-none focus:ring-2 focus:ring-primary"
            >
              {(Object.entries(PALETTE_NAMES) as [GradientPalette, string][]).map(
                ([key, name]) => (
                  <option key={key} value={key}>
                    {name}
                  </option>
                )
              )}
            </select>
          </div>

          {/* Auto-adjust Toggle */}
          <label
            className={`
              flex items-center gap-3 p-3 rounded-lg border cursor-pointer
              transition-colors bg-card hover:bg-accent/50
              ${disabled ? 'opacity-50 cursor-not-allowed' : ''}
            `}
          >
            <input
              type="checkbox"
              checked={settings.autoAdjust}
              onChange={(e) => updateSetting('autoAdjust', e.target.checked)}
              disabled={disabled}
              className="h-4 w-4 rounded border-gray-300"
            />
            <div className="flex-1">
              <span className="font-medium text-sm">Auto-adjust to viewport</span>
              <p className="text-xs text-muted-foreground mt-1">
                Scale colors to current view instead of global dataset
              </p>
            </div>
          </label>
        </CardContent>
      </Card>

      {/* Opacity Settings */}
      <Card>
        <CardHeader>
          <CardTitle className="text-lg">Opacity</CardTitle>
        </CardHeader>
        <CardContent className="space-y-4">
          {/* Opacity Mode Selector */}
          <div>
            <label className="text-sm font-medium mb-2 block">
              Opacity Mode
            </label>
            <div className="space-y-2">
              <label
                className={`
                  flex items-center gap-3 p-3 rounded-lg border cursor-pointer
                  transition-colors
                  ${
                    settings.opacityMode === 'variable'
                      ? 'bg-accent border-primary'
                      : 'bg-card hover:bg-accent/50'
                  }
                  ${disabled ? 'opacity-50 cursor-not-allowed' : ''}
                `}
              >
                <input
                  type="radio"
                  name="opacityMode"
                  checked={settings.opacityMode === 'variable'}
                  onChange={() => updateSetting('opacityMode', 'variable')}
                  disabled={disabled}
                  className="h-4 w-4"
                />
                <div className="flex-1">
                  <span className="font-medium text-sm">Variable</span>
                  <p className="text-xs text-muted-foreground mt-1">
                    Higher values are more opaque (30-90%)
                  </p>
                </div>
              </label>

              <label
                className={`
                  flex items-center gap-3 p-3 rounded-lg border cursor-pointer
                  transition-colors
                  ${
                    settings.opacityMode === 'fixed'
                      ? 'bg-accent border-primary'
                      : 'bg-card hover:bg-accent/50'
                  }
                  ${disabled ? 'opacity-50 cursor-not-allowed' : ''}
                `}
              >
                <input
                  type="radio"
                  name="opacityMode"
                  checked={settings.opacityMode === 'fixed'}
                  onChange={() => updateSetting('opacityMode', 'fixed')}
                  disabled={disabled}
                  className="h-4 w-4"
                />
                <div className="flex-1">
                  <span className="font-medium text-sm">Fixed</span>
                  <p className="text-xs text-muted-foreground mt-1">
                    Same opacity for all squares
                  </p>
                </div>
              </label>
            </div>
          </div>

          {/* Opacity Slider (only visible in Fixed mode) */}
          {settings.opacityMode === 'fixed' && (
            <div>
              <label className="text-sm font-medium mb-2 block flex justify-between">
                <span>Opacity Level</span>
                <span className="text-muted-foreground">{settings.fixedOpacity}%</span>
              </label>
              <input
                type="range"
                min="10"
                max="100"
                step="5"
                value={settings.fixedOpacity}
                onChange={(e) => updateSetting('fixedOpacity', parseInt(e.target.value))}
                disabled={disabled}
                className="w-full h-2 bg-gray-200 rounded-lg appearance-none cursor-pointer"
              />
              <div className="flex justify-between text-xs text-muted-foreground mt-1">
                <span>10%</span>
                <span>100%</span>
              </div>
            </div>
          )}
        </CardContent>
      </Card>

      {/* Display Options */}
      <Card>
        <CardHeader>
          <CardTitle className="text-lg">Display</CardTitle>
        </CardHeader>
        <CardContent>
          <label
            className={`
              flex items-center gap-3 p-3 rounded-lg border cursor-pointer
              transition-colors bg-card hover:bg-accent/50
              ${disabled ? 'opacity-50 cursor-not-allowed' : ''}
            `}
          >
            <input
              type="checkbox"
              checked={settings.showBorders}
              onChange={(e) => updateSetting('showBorders', e.target.checked)}
              disabled={disabled}
              className="h-4 w-4 rounded border-gray-300"
            />
            <div className="flex-1">
              <span className="font-medium text-sm">Show grid borders</span>
              <p className="text-xs text-muted-foreground mt-1">
                Display thin borders around each square
              </p>
            </div>
          </label>
        </CardContent>
      </Card>

      {/* Grid Resolution */}
      <Card>
        <CardHeader>
          <CardTitle className="text-lg">Grid Resolution</CardTitle>
        </CardHeader>
        <CardContent className="space-y-4">
          {/* Auto-select Toggle */}
          <label
            className={`
              flex items-center gap-3 p-3 rounded-lg border cursor-pointer
              transition-colors bg-card hover:bg-accent/50
              ${disabled ? 'opacity-50 cursor-not-allowed' : ''}
            `}
          >
            <input
              type="checkbox"
              checked={settings.autoResolution}
              onChange={(e) => updateSetting('autoResolution', e.target.checked)}
              disabled={disabled}
              className="h-4 w-4 rounded border-gray-300"
            />
            <div className="flex-1">
              <span className="font-medium text-sm">Auto-select from zoom level</span>
              <p className="text-xs text-muted-foreground mt-1">
                Automatically adjust resolution based on map zoom
              </p>
            </div>
          </label>

          {/* Manual Resolution Selector */}
          {!settings.autoResolution && (
            <div>
              <label className="text-sm font-medium mb-2 block">
                Resolution (grid cells)
              </label>
              <select
                value={settings.manualResolution}
                onChange={(e) =>
                  updateSetting('manualResolution', parseInt(e.target.value))
                }
                disabled={disabled}
                className="w-full px-3 py-2 bg-background border border-input rounded-md text-sm focus:outline-none focus:ring-2 focus:ring-primary"
              >
                <option value="1">1 (Very Fine - Query every cell)</option>
                <option value="2">2 (Fine - Query every 2 cells)</option>
                <option value="5">5 (Query every 5 cells)</option>
                <option value="10">10 (Medium - Query every 10 cells)</option>
                <option value="20">20 (Query every 20 cells)</option>
                <option value="50">50 (Coarse - Query every 50 cells)</option>
                <option value="100">100 (Very Coarse - Query every 100 cells)</option>
                <option value="200">200 (Ultra Coarse - Query every 200 cells)</option>
              </select>
              <p className="text-xs text-muted-foreground mt-1">
                Lower values = more detail but slower queries. Higher values = faster but less detail.
              </p>
            </div>
          )}
        </CardContent>
      </Card>
    </div>
  );
};
