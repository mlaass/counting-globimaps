#!/usr/bin/env python3
"""
Convert COVID-19 time-series data to time-indexed HDF5 for hotspot evolution analysis.

Uses the JHU CSSE time_series_confirmed_global.csv which has:
- Lat/Long for each location
- Daily cumulative case counts from Jan 2020 onwards

Creates an HDF5 file with:
- 'coords': [lat, lon, timestamp_idx] triplets (sampled)
- 'timestamps': list of selected date columns
- 'cumulative_counts': total cases at each timestamp

This enables analyzing how spatial hotspots evolve over time.
"""

import argparse
import h5py
import numpy as np
import pandas as pd
from pathlib import Path


# Selected time points for analysis (date column names in JHU format M/D/YY)
SELECTED_DATES = [
    ("3/15/20", 0, "2020-03-15"),   # Global spread begins
    ("6/15/20", 1, "2020-06-15"),   # First wave peak
    ("12/31/20", 2, "2020-12-31"),  # End of year 1
    ("6/30/21", 3, "2021-06-30"),   # Delta variant (peak cases)
    ("12/31/21", 4, "2021-12-31"),  # Omicron emergence
    ("6/30/22", 5, "2022-06-30"),   # Late pandemic
]


def convert_covid_timeseries(timeseries_csv, output_path, sample_rate=0.001):
    """
    Convert COVID-19 time series to time-indexed HDF5.

    Args:
        timeseries_csv: Path to time_series_confirmed_global.csv
        output_path: Output HDF5 file path
        sample_rate: Fraction of cases to sample (default 0.001 = 0.1%)
    """
    print(f"Reading COVID-19 time series: {timeseries_csv}")
    print(f"Sample rate: {sample_rate*100:.2f}%")
    print("=" * 60)

    # Read the time series file
    df = pd.read_csv(timeseries_csv)

    # Get the available date columns
    date_cols = [col for col in df.columns if '/' in col]
    print(f"Found {len(date_cols)} date columns ({date_cols[0]} to {date_cols[-1]})")

    # Filter out rows with missing coordinates
    df = df.dropna(subset=['Lat', 'Long'])
    print(f"Found {len(df)} locations with coordinates")

    all_coords = []
    timestamps = []
    cumulative_counts = []

    for date_col, time_idx, date_label in SELECTED_DATES:
        if date_col not in df.columns:
            print(f"  [{time_idx}] {date_label}: SKIPPED (column not found)")
            continue

        # Get case counts for this date
        cases = df[['Lat', 'Long', date_col]].copy()
        cases.columns = ['Lat', 'Long', 'Confirmed']
        cases = cases[cases['Confirmed'] > 0]

        # Group by location and sum (in case of duplicates)
        location_cases = cases.groupby(['Lat', 'Long'])['Confirmed'].sum().reset_index()

        # Sample cases
        location_cases['sampled'] = (location_cases['Confirmed'] * sample_rate).astype(int)
        location_cases = location_cases[location_cases['sampled'] > 0]

        # Create coordinate triplets [lat, lon, time_idx]
        snapshot_coords = []
        for _, row in location_cases.iterrows():
            for _ in range(int(row['sampled'])):
                snapshot_coords.append([row['Lat'], row['Long'], time_idx])

        total_cases = location_cases['sampled'].sum()
        raw_total = cases['Confirmed'].sum()

        print(f"  [{time_idx}] {date_label}: {len(location_cases):,} locations, "
              f"{total_cases:,} sampled from {raw_total:,.0f} total")

        all_coords.extend(snapshot_coords)
        timestamps.append(date_label)
        cumulative_counts.append(int(total_cases))

    # Convert to numpy array
    coords = np.array(all_coords, dtype=np.float64)

    print("=" * 60)
    print(f"Total: {len(coords):,} coordinate triplets across {len(timestamps)} snapshots")

    # Create HDF5 file
    print(f"\nWriting HDF5: {output_path}")
    with h5py.File(output_path, 'w') as f:
        # Main coordinate dataset [lat, lon, timestamp_idx]
        f.create_dataset('coords', data=coords, compression='gzip', compression_opts=9)

        # Metadata
        f.create_dataset('timestamps', data=np.array(timestamps, dtype='S10'))
        f.create_dataset('cumulative_counts', data=np.array(cumulative_counts, dtype=np.int64))

        # Attributes
        f.attrs['description'] = 'COVID-19 time-series data for CascadeCBF evaluation'
        f.attrs['sample_rate'] = sample_rate
        f.attrs['num_timestamps'] = len(timestamps)

    print(f"Created {output_path} with {len(coords):,} coordinate triplets")

    return len(coords)


def main():
    parser = argparse.ArgumentParser(
        description='Convert COVID-19 time series to time-indexed HDF5'
    )
    parser.add_argument(
        '--sample-rate',
        type=float,
        default=0.001,
        help='Case sampling rate (default: 0.001 = 0.1%%)'
    )
    parser.add_argument(
        '--output',
        type=str,
        default=None,
        help='Output HDF5 path (default: datasets/hdf5/covid19_timeseries.h5)'
    )

    args = parser.parse_args()

    # Paths
    base_dir = Path(__file__).parent.parent
    timeseries_csv = base_dir / 'covid19' / 'time_series_confirmed_global.csv'
    hdf5_dir = base_dir / 'hdf5'

    hdf5_dir.mkdir(exist_ok=True)

    if not timeseries_csv.exists():
        print(f"Error: Time series file not found: {timeseries_csv}")
        return 1

    output_path = args.output or (hdf5_dir / 'covid19_timeseries.h5')

    convert_covid_timeseries(timeseries_csv, output_path, args.sample_rate)
    return 0


if __name__ == '__main__':
    exit(main())
