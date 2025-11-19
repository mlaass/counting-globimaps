/**
 * Filter Info Component
 *
 * Displays metadata about the loaded filter
 */

import React from 'react';
import { CBFFilter } from '../wasm/loader';
import { FilterTypeNames } from '../wasm/types';
import { Card, CardHeader, CardTitle, CardContent } from './ui/card';

interface FilterInfoProps {
  filter: CBFFilter | null;
}

function formatBytes(bytes: number | bigint): string {
  if (bytes === 0 || bytes === 0n) return '0 Bytes';

  // Convert BigInt to Number for Math operations
  const numBytes = typeof bytes === 'bigint' ? Number(bytes) : bytes;

  const k = 1024;
  const sizes = ['Bytes', 'KB', 'MB', 'GB'];
  const i = Math.floor(Math.log(numBytes) / Math.log(k));

  return `${parseFloat((numBytes / Math.pow(k, i)).toFixed(2))} ${sizes[i]}`;
}

export const FilterInfo: React.FC<FilterInfoProps> = ({ filter }) => {
  if (!filter) {
    return (
      <Card>
        <CardHeader>
          <CardTitle className="text-lg">Filter</CardTitle>
        </CardHeader>
        <CardContent>
          <p className="text-sm text-muted-foreground">No filter loaded</p>
        </CardContent>
      </Card>
    );
  }

  const metadata = filter.getMetadata();

  return (
    <Card>
      <CardHeader>
        <CardTitle className="text-lg">Filter</CardTitle>
      </CardHeader>
      <CardContent className="space-y-2 text-sm">
        <div className="flex justify-between">
          <span className="text-muted-foreground">Type:</span>
          <span className="font-medium">{FilterTypeNames[metadata.type]}</span>
        </div>

        <div className="flex justify-between">
          <span className="text-muted-foreground">Memory:</span>
          <span className="font-medium">{formatBytes(metadata.memoryUsage)}</span>
        </div>

        <div className="pt-2 border-t">
          <span className="text-muted-foreground text-xs">{metadata.info}</span>
        </div>
      </CardContent>
    </Card>
  );
};
