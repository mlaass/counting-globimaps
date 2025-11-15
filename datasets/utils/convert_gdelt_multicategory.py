#!/usr/bin/env python3
"""
Convert GDELT events to multi-category HDF5 format.

This script extracts GDELT events with geographic coordinates and QuadClass categories,
creating an HDF5 file with [lat, lon, category] triplets for multi-category counting
bloom filter experiments.

GDELT 2.0 column reference:
- Column 30 (0-indexed): GoldsteinScale (used to derive QuadClass)
- Column 56: ActionGeo_Lat
- Column 57: ActionGeo_Long

QuadClass derivation from Goldstein scale:
- QuadClass 1 (Verbal Cooperation): 0 < Goldstein <= 5
- QuadClass 2 (Material Cooperation): Goldstein > 5
- QuadClass 3 (Verbal Conflict): -5 <= Goldstein < 0
- QuadClass 4 (Material Conflict): Goldstein < -5
"""

import argparse
import h5py
import numpy as np
import pandas as pd
from pathlib import Path


def convert_gdelt_multicategory(csv_path, hdf5_path, sample_size=None):
    """
    Convert GDELT events CSV to multi-category HDF5 format.

    Creates [lat, lon, category] triplets where category is QuadClass (1-4).

    Args:
        csv_path: Path to GDELT CSV file (tab-separated)
        hdf5_path: Output HDF5 file path
        sample_size: Optional number of events to sample (None = all)

    Returns:
        Number of events written
    """
    print(f"Reading GDELT CSV: {csv_path}")
    print("Extracting: ActionGeo_Lat (col 56), ActionGeo_Long (col 57), GoldsteinScale (col 30)")

    # GDELT 2.0 format: tab-separated, no header (or skip first row)
    # Column indices: GoldsteinScale=30, ActionGeo_Lat=56, ActionGeo_Long=57
    df = pd.read_csv(
        csv_path,
        sep='\t',
        header=None,
        skiprows=1,
        usecols=[30, 56, 57],
        names=['GoldsteinScale', 'ActionGeo_Lat', 'ActionGeo_Long']
    )

    print(f"Loaded {len(df)} total rows")

    # Filter out rows with missing coordinates or Goldstein scale
    initial_count = len(df)
    df = df.dropna(subset=['ActionGeo_Lat', 'ActionGeo_Long', 'GoldsteinScale'])
    print(f"Dropped {initial_count - len(df)} rows with missing data")

    # Derive QuadClass from Goldstein scale
    def goldstein_to_quadclass(goldstein):
        if goldstein > 5:
            return 2  # Material Cooperation
        elif goldstein > 0:
            return 1  # Verbal Cooperation
        elif goldstein >= -5:
            return 3  # Verbal Conflict
        else:
            return 4  # Material Conflict

    df['QuadClass'] = df['GoldsteinScale'].apply(goldstein_to_quadclass)
    print(f"Found {len(df)} events with valid coordinates and derived QuadClass")

    # Show category distribution
    print("\nCategory distribution:")
    for cat in [1, 2, 3, 4]:
        count = len(df[df['QuadClass'] == cat])
        pct = count / len(df) * 100 if len(df) > 0 else 0
        cat_name = {
            1: "Verbal Cooperation",
            2: "Material Cooperation",
            3: "Verbal Conflict",
            4: "Material Conflict"
        }[cat]
        print(f"  QuadClass {cat} ({cat_name:20s}): {count:8,} events ({pct:5.1f}%)")

    # Sample if requested
    if sample_size and sample_size < len(df):
        df = df.sample(n=sample_size, random_state=42)
        print(f"\nSampled {sample_size:,} events")

    # Convert to numpy array: [lat, lon, category]
    coords_with_category = df[['ActionGeo_Lat', 'ActionGeo_Long', 'QuadClass']].values.astype(np.float64)

    # Create HDF5 file
    print(f"\nWriting HDF5: {hdf5_path}")
    with h5py.File(hdf5_path, 'w') as f:
        f.create_dataset('coords', data=coords_with_category, compression='gzip', compression_opts=9)

        # Add metadata
        f.attrs['description'] = 'GDELT events with QuadClass categories'
        f.attrs['format'] = '[latitude, longitude, category]'
        f.attrs['category_1'] = 'Verbal Cooperation'
        f.attrs['category_2'] = 'Material Cooperation'
        f.attrs['category_3'] = 'Verbal Conflict'
        f.attrs['category_4'] = 'Material Conflict'

    print(f"✓ Created {hdf5_path} with {len(coords_with_category):,} events")

    # Calculate and display file size
    file_size_mb = Path(hdf5_path).stat().st_size / (1024 * 1024)
    print(f"  File size: {file_size_mb:.2f} MB")

    return len(coords_with_category)


def main():
    parser = argparse.ArgumentParser(
        description='Convert GDELT events to multi-category HDF5 format with QuadClass'
    )
    parser.add_argument(
        'csv_path',
        type=str,
        help='Path to GDELT CSV file (tab-separated, GDELT 2.0 format)'
    )
    parser.add_argument(
        '-o', '--output',
        type=str,
        default='gdelt_events_multicategory.h5',
        help='Output HDF5 file path (default: gdelt_events_multicategory.h5)'
    )
    parser.add_argument(
        '-n', '--sample-size',
        type=int,
        default=None,
        help='Sample size (number of events to keep, default: all)'
    )

    args = parser.parse_args()

    # Validate input file exists
    if not Path(args.csv_path).exists():
        print(f"Error: Input file not found: {args.csv_path}")
        return 1

    # Create output directory if needed
    output_path = Path(args.output)
    output_path.parent.mkdir(parents=True, exist_ok=True)

    # Convert
    try:
        convert_gdelt_multicategory(args.csv_path, args.output, args.sample_size)
        return 0
    except Exception as e:
        print(f"Error during conversion: {e}")
        import traceback
        traceback.print_exc()
        return 1


if __name__ == '__main__':
    exit(main())
