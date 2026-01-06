#!/usr/bin/env python3
"""
Convert NYC Taxi trip data to HDF5 format.

Supports:
- TSV format from ClickHouse datasets (has actual lat/long)
- Parquet format from TLC (pre-July 2016 data has actual lat/long)

Creates an HDF5 file with:
- 'coords': [lat, lon] pairs for all pickup locations
- Metadata about the dataset

This enables spatial hotspot analysis of urban mobility patterns.
"""

import argparse
import gzip
import h5py
import numpy as np
import pandas as pd
from pathlib import Path


def convert_taxi_tsv(tsv_path, output_path, sample_rate=1.0):
    """
    Convert NYC Taxi TSV (from ClickHouse) to HDF5.
    """
    print(f"Reading TSV file: {tsv_path}")
    print(f"Sample rate: {sample_rate*100:.1f}%")
    print("=" * 60)

    # Read TSV in chunks to handle malformed rows
    chunks = []
    chunk_size = 100000

    try:
        if str(tsv_path).endswith('.gz'):
            reader = pd.read_csv(tsv_path, sep='\t', compression='gzip',
                                 usecols=['pickup_latitude', 'pickup_longitude'],
                                 encoding='latin-1', on_bad_lines='skip',
                                 chunksize=chunk_size, low_memory=False,
                                 quoting=3)  # QUOTE_NONE to ignore quote chars
        else:
            reader = pd.read_csv(tsv_path, sep='\t',
                                 usecols=['pickup_latitude', 'pickup_longitude'],
                                 encoding='latin-1', on_bad_lines='skip',
                                 chunksize=chunk_size, low_memory=False,
                                 quoting=3)

        for i, chunk in enumerate(reader):
            chunks.append(chunk)
            if (i + 1) % 10 == 0:
                print(f"  Read {(i+1) * chunk_size:,} rows...")

    except Exception as e:
        print(f"Warning: Stopped reading at error: {e}")

    if not chunks:
        print("Error: No data read from file")
        return 0

    df = pd.concat(chunks, ignore_index=True)
    print(f"Total rows: {len(df):,}")

    lat_col = 'pickup_latitude'
    lon_col = 'pickup_longitude'

    # Filter valid coordinates (NYC bounding box)
    valid_mask = (
        (df[lat_col] >= 40.4) & (df[lat_col] <= 41.0) &
        (df[lon_col] >= -74.3) & (df[lon_col] <= -73.7) &
        df[lat_col].notna() & df[lon_col].notna()
    )

    df_valid = df[valid_mask]
    print(f"Valid coordinates: {len(df_valid):,} ({len(df_valid)/len(df)*100:.1f}%)")

    # Sample if requested
    if sample_rate < 1.0:
        df_valid = df_valid.sample(frac=sample_rate, random_state=42)
        print(f"After sampling: {len(df_valid):,}")

    # Extract coordinates
    coords = np.column_stack([
        df_valid[lat_col].values,
        df_valid[lon_col].values
    ]).astype(np.float64)

    print(f"\nCoordinate statistics:")
    print(f"  Latitude range: [{coords[:, 0].min():.4f}, {coords[:, 0].max():.4f}]")
    print(f"  Longitude range: [{coords[:, 1].min():.4f}, {coords[:, 1].max():.4f}]")

    # Create HDF5 file
    print(f"\nWriting HDF5: {output_path}")
    with h5py.File(output_path, 'w') as f:
        f.create_dataset('coords', data=coords, compression='gzip', compression_opts=9)
        f.attrs['description'] = 'NYC Yellow Taxi pickup locations'
        f.attrs['source'] = 'ClickHouse NYC Taxi Dataset'
        f.attrs['sample_rate'] = sample_rate
        f.attrs['num_trips'] = len(coords)

    print(f"Created {output_path} with {len(coords):,} pickup locations")
    return len(coords)


def convert_taxi_parquet(parquet_path, output_path, sample_rate=1.0):
    """
    Convert NYC Taxi Parquet to HDF5.
    """
    try:
        import pyarrow.parquet as pq
    except ImportError:
        print("Error: pyarrow required for parquet. Install with: pip install pyarrow")
        return 0

    print(f"Reading Parquet file: {parquet_path}")
    print(f"Sample rate: {sample_rate*100:.1f}%")
    print("=" * 60)

    table = pq.read_table(parquet_path)
    df = table.to_pandas()

    print(f"Total rows: {len(df):,}")
    print(f"Columns: {list(df.columns)}")

    # Find lat/lon columns
    lat_col = lon_col = None
    for name in ['pickup_latitude', 'Pickup_latitude', 'start_lat']:
        if name in df.columns:
            lat_col = name
            break
    for name in ['pickup_longitude', 'Pickup_longitude', 'start_lon']:
        if name in df.columns:
            lon_col = name
            break

    if lat_col is None or lon_col is None:
        print("Error: Could not find pickup latitude/longitude columns")
        print(f"Available columns: {list(df.columns)}")
        return 0

    print(f"Using columns: {lat_col}, {lon_col}")

    valid_mask = (
        (df[lat_col] >= 40.4) & (df[lat_col] <= 41.0) &
        (df[lon_col] >= -74.3) & (df[lon_col] <= -73.7) &
        df[lat_col].notna() & df[lon_col].notna()
    )

    df_valid = df[valid_mask]
    print(f"Valid coordinates: {len(df_valid):,} ({len(df_valid)/len(df)*100:.1f}%)")

    if sample_rate < 1.0:
        df_valid = df_valid.sample(frac=sample_rate, random_state=42)
        print(f"After sampling: {len(df_valid):,}")

    coords = np.column_stack([
        df_valid[lat_col].values,
        df_valid[lon_col].values
    ]).astype(np.float64)

    print(f"\nCoordinate statistics:")
    print(f"  Latitude range: [{coords[:, 0].min():.4f}, {coords[:, 0].max():.4f}]")
    print(f"  Longitude range: [{coords[:, 1].min():.4f}, {coords[:, 1].max():.4f}]")

    print(f"\nWriting HDF5: {output_path}")
    with h5py.File(output_path, 'w') as f:
        f.create_dataset('coords', data=coords, compression='gzip', compression_opts=9)
        f.attrs['description'] = 'NYC Yellow Taxi pickup locations'
        f.attrs['source'] = 'NYC TLC Trip Record Data'
        f.attrs['sample_rate'] = sample_rate
        f.attrs['num_trips'] = len(coords)

    print(f"Created {output_path} with {len(coords):,} pickup locations")
    return len(coords)


def main():
    parser = argparse.ArgumentParser(
        description='Convert NYC Taxi data (TSV or Parquet) to HDF5'
    )
    parser.add_argument(
        'input',
        type=str,
        help='Input file path (TSV, TSV.gz, or Parquet)'
    )
    parser.add_argument(
        '--output',
        type=str,
        default=None,
        help='Output HDF5 path (default: datasets/hdf5/nyc_taxi.h5)'
    )
    parser.add_argument(
        '--sample-rate',
        type=float,
        default=1.0,
        help='Sampling rate (default: 1.0 = all data)'
    )

    args = parser.parse_args()

    # Paths
    input_path = Path(args.input)
    if not input_path.exists():
        print(f"Error: Input file not found: {input_path}")
        return 1

    base_dir = Path(__file__).parent.parent
    hdf5_dir = base_dir / 'hdf5'
    hdf5_dir.mkdir(exist_ok=True)

    output_path = args.output or (hdf5_dir / 'nyc_taxi.h5')

    # Detect file type and convert
    fname = str(input_path).lower()
    if fname.endswith('.parquet'):
        convert_taxi_parquet(input_path, output_path, args.sample_rate)
    elif fname.endswith('.gz') or fname.endswith('.tsv') or fname.endswith('.csv'):
        convert_taxi_tsv(input_path, output_path, args.sample_rate)
    else:
        print(f"Error: Unknown file type: {input_path}")
        print("Supported formats: .parquet, .tsv, .tsv.gz, .csv, .gz")
        return 1

    return 0


if __name__ == '__main__':
    exit(main())
