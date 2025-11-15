#!/usr/bin/env python3
"""
Convert CSV datasets to HDF5 format for CountingGloBiMap experiments.

This script converts geographic datasets from CSV to HDF5 format with a
standardized 'coords' dataset containing [lat, lon] pairs.
"""

import argparse
import h5py
import numpy as np
import pandas as pd
import sys
from pathlib import Path


def convert_gdelt_to_hdf5(csv_path, hdf5_path, sample_size=None):
    """
    Convert GDELT events CSV to HDF5 format.

    Args:
        csv_path: Path to GDELT CSV file
        hdf5_path: Output HDF5 file path
        sample_size: Optional number of events to sample (None = all)
    """
    print(f"Reading GDELT CSV: {csv_path}")

    # GDELT 2.0 format has tab-separated values with inconsistent header
    # Skip header and use column indices directly
    # ActionGeo_Lat is at index 56, ActionGeo_Long is at index 57 (0-indexed)
    df = pd.read_csv(csv_path, sep='\t', header=None, skiprows=1, usecols=[56, 57])
    df.columns = ['ActionGeo_Lat', 'ActionGeo_Long']

    # Filter out rows with missing coordinates
    df = df.dropna(subset=['ActionGeo_Lat', 'ActionGeo_Long'])

    print(f"Found {len(df)} events with coordinates")

    # Sample if requested
    if sample_size and sample_size < len(df):
        df = df.sample(n=sample_size, random_state=42)
        print(f"Sampled {sample_size} events")

    # Convert to numpy array
    coords = df[['ActionGeo_Lat', 'ActionGeo_Long']].values.astype(np.float64)

    # Create HDF5 file
    print(f"Writing HDF5: {hdf5_path}")
    with h5py.File(hdf5_path, 'w') as f:
        f.create_dataset('coords', data=coords, compression='gzip', compression_opts=9)

    print(f"✓ Created {hdf5_path} with {len(coords)} coordinate pairs")
    return len(coords)


def convert_covid_daily_to_hdf5(csv_path, hdf5_path, sample_rate=0.01):
    """
    Convert COVID-19 daily snapshot CSV to HDF5 format.

    Creates one entry per confirmed case at each location (sampled for efficiency).
    This creates a realistic spatial distribution with hotspots.

    Args:
        csv_path: Path to COVID-19 daily snapshot CSV
        hdf5_path: Output HDF5 file path
        sample_rate: Fraction of cases to sample (default 0.01 = 1%)
    """
    print(f"Reading COVID-19 daily snapshot: {csv_path}")

    # Read location and case count columns
    df = pd.read_csv(csv_path, usecols=['Lat', 'Long_', 'Confirmed'])
    df = df.rename(columns={'Long_': 'Long'})

    # Filter out rows with missing coordinates or zero cases
    df = df.dropna(subset=['Lat', 'Long'])
    df = df[df['Confirmed'] > 0]

    print(f"Found {len(df)} locations with cases")
    total_cases = df['Confirmed'].sum()
    print(f"Total confirmed cases: {total_cases:,}")

    # Create coordinate list with repetition based on case counts (sampled)
    coords_list = []
    for _, row in df.iterrows():
        lat, lon = row['Lat'], row['Long']
        # Sample the confirmed cases
        count = int(row['Confirmed'] * sample_rate)
        if count > 0:
            coords_list.extend([[lat, lon]] * count)

    coords = np.array(coords_list, dtype=np.float64)

    print(f"Generated {len(coords):,} coordinate pairs (sampled at {sample_rate*100:.1f}%)")

    # Create HDF5 file
    print(f"Writing HDF5: {hdf5_path}")
    with h5py.File(hdf5_path, 'w') as f:
        f.create_dataset('coords', data=coords, compression='gzip', compression_opts=9)

    print(f"✓ Created {hdf5_path} with {len(coords):,} coordinate pairs")
    return len(coords)


def main():
    parser = argparse.ArgumentParser(
        description='Convert CSV datasets to HDF5 format for CountingGloBiMap'
    )
    parser.add_argument(
        '--dataset',
        choices=['gdelt', 'covid', 'all'],
        default='all',
        help='Which dataset(s) to convert'
    )
    parser.add_argument(
        '--gdelt-sample',
        type=int,
        default=None,
        help='Sample size for GDELT dataset (default: all events)'
    )
    parser.add_argument(
        '--covid-sample-rate',
        type=float,
        default=0.01,
        help='COVID-19 case sampling rate (default: 0.01 = 1%% of cases)'
    )

    args = parser.parse_args()

    # Paths
    base_dir = Path(__file__).parent.parent
    gdelt_csv = base_dir / 'gdelt' / 'gdelt_events_sample.csv'
    # Use the largest daily snapshot for COVID-19
    covid_csv = base_dir / 'covid19' / '06-30-2021.csv'
    hdf5_dir = base_dir / 'hdf5'

    # Ensure HDF5 directory exists
    hdf5_dir.mkdir(exist_ok=True)

    total_events = 0

    # Convert GDELT
    if args.dataset in ['gdelt', 'all']:
        if gdelt_csv.exists():
            gdelt_hdf5 = hdf5_dir / 'gdelt_events.h5'
            try:
                count = convert_gdelt_to_hdf5(gdelt_csv, gdelt_hdf5, args.gdelt_sample)
                total_events += count
            except Exception as e:
                print(f"✗ Error converting GDELT: {e}", file=sys.stderr)
        else:
            print(f"✗ GDELT CSV not found: {gdelt_csv}", file=sys.stderr)

    # Convert COVID-19
    if args.dataset in ['covid', 'all']:
        if covid_csv.exists():
            covid_hdf5 = hdf5_dir / 'covid19_cases.h5'
            try:
                count = convert_covid_daily_to_hdf5(covid_csv, covid_hdf5, args.covid_sample_rate)
                total_events += count
            except Exception as e:
                print(f"✗ Error converting COVID-19: {e}", file=sys.stderr)
        else:
            print(f"✗ COVID-19 CSV not found: {covid_csv}", file=sys.stderr)

    print(f"\n{'='*50}")
    print(f"Total events converted: {total_events:,}")
    print(f"HDF5 files saved to: {hdf5_dir}")
    print(f"{'='*50}")


if __name__ == '__main__':
    main()
