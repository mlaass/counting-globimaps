/**
 * Statistics Component
 *
 * Displays scan metrics and performance stats
 */

import React from 'react';
import { Card, CardHeader, CardTitle, CardContent } from '../ui/card';
import { ScanResult } from '../../services/gridScanner';

interface StatisticsProps {
  scanResult?: ScanResult;
  currentResolution: number;
  loading?: boolean;
}

export const Statistics: React.FC<StatisticsProps> = ({
  scanResult,
  currentResolution,
  loading,
}) => {
  if (loading) {
    return (
      <Card>
        <CardHeader>
          <CardTitle className="text-lg">Statistics</CardTitle>
        </CardHeader>
        <CardContent>
          <p className="text-sm text-muted-foreground">Scanning...</p>
        </CardContent>
      </Card>
    );
  }

  if (!scanResult) {
    return (
      <Card>
        <CardHeader>
          <CardTitle className="text-lg">Statistics</CardTitle>
        </CardHeader>
        <CardContent>
          <p className="text-sm text-muted-foreground">
            Pan or zoom the map to scan an area
          </p>
        </CardContent>
      </Card>
    );
  }

  const { stats } = scanResult;

  return (
    <Card>
      <CardHeader>
        <CardTitle className="text-lg">Statistics</CardTitle>
      </CardHeader>
      <CardContent className="space-y-4">
        {/* Scan Performance */}
        <div className="space-y-2">
          <h4 className="text-xs font-semibold text-muted-foreground uppercase">
            Scan Performance
          </h4>
          <div className="grid grid-cols-2 gap-2 text-sm">
            <div>
              <span className="text-muted-foreground">Time:</span>
              <span className="ml-2 font-medium">
                {stats.scanTimeMs.toFixed(1)}ms
              </span>
            </div>
            <div>
              <span className="text-muted-foreground">Queries:</span>
              <span className="ml-2 font-medium">
                {stats.totalQueries.toLocaleString()}
              </span>
            </div>
            <div>
              <span className="text-muted-foreground">Resolution:</span>
              <span className="ml-2 font-medium">{currentResolution}px</span>
            </div>
            <div>
              <span className="text-muted-foreground">Speed:</span>
              <span className="ml-2 font-medium">
                {(stats.totalQueries / (stats.scanTimeMs / 1000)).toFixed(0)} q/s
              </span>
            </div>
          </div>
        </div>

        {/* Density Stats */}
        <div className="space-y-2 pt-2 border-t">
          <h4 className="text-xs font-semibold text-muted-foreground uppercase">
            Density Data
          </h4>
          <div className="grid grid-cols-2 gap-2 text-sm">
            <div>
              <span className="text-muted-foreground">Points:</span>
              <span className="ml-2 font-medium">
                {stats.nonZeroPoints.toLocaleString()}
              </span>
            </div>
            <div>
              <span className="text-muted-foreground">Avg:</span>
              <span className="ml-2 font-medium">
                {stats.avgValue.toFixed(1)}
              </span>
            </div>
            <div>
              <span className="text-muted-foreground">Min:</span>
              <span className="ml-2 font-medium">
                {stats.minValue === Infinity ? 'N/A' : stats.minValue}
              </span>
            </div>
            <div>
              <span className="text-muted-foreground">Max:</span>
              <span className="ml-2 font-medium">
                {stats.maxValue === -Infinity ? 'N/A' : stats.maxValue}
              </span>
            </div>
          </div>
        </div>

        {/* Coverage */}
        <div className="pt-2 border-t">
          <div className="text-sm">
            <span className="text-muted-foreground">Coverage:</span>
            <span className="ml-2 font-medium">
              {((stats.nonZeroPoints / stats.totalQueries) * 100).toFixed(1)}%
            </span>
          </div>
        </div>
      </CardContent>
    </Card>
  );
};
