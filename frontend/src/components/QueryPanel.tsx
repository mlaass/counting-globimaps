/**
 * Query Panel Component
 *
 * Simple form interface to query coordinates in the CBF filter
 */

import React, { useState } from 'react';
import { CBFFilter } from '../wasm/loader';
import { Point, QueryResult } from '../wasm/types';

interface QueryPanelProps {
  filter: CBFFilter | null;
  loading: boolean;
}

export const QueryPanel: React.FC<QueryPanelProps> = ({ filter, loading }) => {
  const [x, setX] = useState<string>('100');
  const [y, setY] = useState<string>('200');
  const [category, setCategory] = useState<string>('1');
  const [useCategory, setUseCategory] = useState<boolean>(true);
  const [result, setResult] = useState<QueryResult | null>(null);
  const [error, setError] = useState<string>('');

  const handleQuery = () => {
    if (!filter) {
      setError('No filter loaded');
      return;
    }

    try {
      const point: Point = {
        x: parseFloat(x),
        y: parseFloat(y),
      };

      if (useCategory) {
        point.category = parseInt(category, 10);
      }

      const queryResult = filter.query(point);
      setResult(queryResult);
      setError('');
    } catch (err) {
      setError(err instanceof Error ? err.message : 'Query failed');
      setResult(null);
    }
  };

  if (loading) {
    return (
      <div className="query-panel loading">
        <h2>Query Interface</h2>
        <p>Loading WASM module...</p>
      </div>
    );
  }

  if (!filter) {
    return (
      <div className="query-panel error">
        <h2>Query Interface</h2>
        <p>No filter loaded. Please load a dataset first.</p>
      </div>
    );
  }

  return (
    <div className="query-panel">
      <h2>Query Interface</h2>

      <div className="form-group">
        <label>
          X Coordinate:
          <input
            type="number"
            value={x}
            onChange={(e) => setX(e.target.value)}
            step="1"
          />
        </label>
      </div>

      <div className="form-group">
        <label>
          Y Coordinate:
          <input
            type="number"
            value={y}
            onChange={(e) => setY(e.target.value)}
            step="1"
          />
        </label>
      </div>

      <div className="form-group checkbox">
        <label>
          <input
            type="checkbox"
            checked={useCategory}
            onChange={(e) => setUseCategory(e.target.checked)}
          />
          Use Category
        </label>
      </div>

      {useCategory && (
        <div className="form-group">
          <label>
            Category ID:
            <input
              type="number"
              value={category}
              onChange={(e) => setCategory(e.target.value)}
              min="0"
              step="1"
            />
          </label>
        </div>
      )}

      <button onClick={handleQuery} className="query-button">
        Query
      </button>

      {error && (
        <div className="error-message">
          <strong>Error:</strong> {error}
        </div>
      )}

      {result && (
        <div className="result">
          <h3>Result</h3>
          <div className="result-item">
            <strong>Count:</strong> {result.count}
          </div>
          <div className="result-item">
            <strong>Query Time:</strong> {result.queryTime.toFixed(3)} ms
          </div>
          <div className="result-item">
            <strong>Queried Point:</strong> ({x}, {y}
            {useCategory && `, category ${category}`})
          </div>
        </div>
      )}
    </div>
  );
};
