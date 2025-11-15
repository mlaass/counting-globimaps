#!/bin/bash
###############################################################################
# CountingGloBiMap Dataset Downloader
#
# Downloads spatial datasets showcasing sparse data with hotspots for
# testing and benchmarking CountingGloBiMap implementations.
#
# Usage: ./download_datasets.sh [dataset_name]
#   - If no dataset_name provided, downloads all available datasets
#   - Use --list to see available datasets
###############################################################################

set -e  # Exit on error

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Base directory for datasets
DATASET_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)/datasets"

# Create datasets directory
mkdir -p "$DATASET_DIR"

###############################################################################
# Helper Functions
###############################################################################

log_info() {
    echo -e "${BLUE}[INFO]${NC} $1"
}

log_success() {
    echo -e "${GREEN}[SUCCESS]${NC} $1"
}

log_warning() {
    echo -e "${YELLOW}[WARNING]${NC} $1"
}

log_error() {
    echo -e "${RED}[ERROR]${NC} $1"
}

check_command() {
    if ! command -v $1 &> /dev/null; then
        log_error "$1 is required but not installed. Please install it first."
        exit 1
    fi
}

file_exists() {
    if [ -f "$1" ]; then
        log_info "File already exists: $(basename $1)"
        return 0
    fi
    return 1
}

download_file() {
    local url=$1
    local output=$2

    if file_exists "$output"; then
        return 0
    fi

    log_info "Downloading $(basename $output)..."
    if command -v wget &> /dev/null; then
        wget -q --show-progress -O "$output" "$url" || {
            log_error "Download failed: $url"
            rm -f "$output"
            return 1
        }
    elif command -v curl &> /dev/null; then
        curl -L -o "$output" --progress-bar "$url" || {
            log_error "Download failed: $url"
            rm -f "$output"
            return 1
        }
    else
        log_error "Neither wget nor curl is installed"
        return 1
    fi

    log_success "Downloaded $(basename $output)"
    return 0
}

extract_archive() {
    local archive=$1
    local dest_dir=$2

    if [ ! -f "$archive" ]; then
        log_error "Archive not found: $archive"
        return 1
    fi

    log_info "Extracting $(basename $archive)..."

    case "$archive" in
        *.tar.gz|*.tgz)
            tar -xzf "$archive" -C "$dest_dir"
            ;;
        *.tar.bz2|*.tbz2)
            tar -xjf "$archive" -C "$dest_dir"
            ;;
        *.zip)
            unzip -q -o "$archive" -d "$dest_dir"
            ;;
        *.gz)
            gunzip -k "$archive"
            ;;
        *)
            log_error "Unknown archive format: $archive"
            return 1
            ;;
    esac

    log_success "Extracted $(basename $archive)"
    return 0
}

###############################################################################
# Dataset Download Functions
###############################################################################

download_covid19_data() {
    log_info "========================================="
    log_info "COVID-19 Case Data (Johns Hopkins CSSE)"
    log_info "========================================="

    local covid_dir="$DATASET_DIR/covid19"
    mkdir -p "$covid_dir"

    # Johns Hopkins CSSE COVID-19 Data Repository
    # Daily case reports (CSV format)
    local base_url="https://raw.githubusercontent.com/CSSEGISandData/COVID-19/master/csse_covid_19_data/csse_covid_19_daily_reports"

    log_info "Downloading COVID-19 daily reports (sample dates)..."

    # Download a selection of dates to show temporal evolution
    local dates=(
        "01-22-2020"  # First report
        "03-15-2020"  # Early pandemic
        "06-15-2020"  # Mid-2020
        "12-31-2020"  # End of 2020
        "06-30-2021"  # Mid-2021
        "12-31-2021"  # End of 2021
        "06-30-2022"  # Recent data
    )

    for date in "${dates[@]}"; do
        download_file "${base_url}/${date}.csv" "${covid_dir}/${date}.csv"
    done

    # Download time series data
    local ts_url="https://raw.githubusercontent.com/CSSEGISandData/COVID-19/master/csse_covid_19_data/csse_covid_19_time_series"
    download_file "${ts_url}/time_series_covid19_confirmed_global.csv" "${covid_dir}/time_series_confirmed_global.csv"
    download_file "${ts_url}/time_series_covid19_deaths_global.csv" "${covid_dir}/time_series_deaths_global.csv"

    log_success "COVID-19 data downloaded to: $covid_dir"
}

download_lightning_data() {
    log_info "========================================="
    log_info "Lightning Strike Data (NOAA)"
    log_info "========================================="

    local lightning_dir="$DATASET_DIR/lightning"
    mkdir -p "$lightning_dir"

    log_warning "NOAA lightning data requires registration or comes in large NetCDF files"
    log_info "Alternative: Using GHCN (Global Historical Climatology Network) lightning data"

    # Sample lightning data - WWLLN (World Wide Lightning Location Network) is open
    # but requires account. Providing alternative instructions.

    cat > "$lightning_dir/README.txt" <<EOF
Lightning Strike Data Sources
==============================

Option 1: NOAA Lightning Data (Recommended)
--------------------------------------------
Website: https://www.ncei.noaa.gov/products/lightning
- Geostationary Lightning Mapper (GLM) data
- Requires downloading large NetCDF files
- Free but requires NOAA account

Download steps:
1. Visit https://www.ncei.noaa.gov/products/lightning
2. Create free NOAA account
3. Download GLM Level 2 data (gridded flash extent density)

Option 2: WWLLN (World Wide Lightning Location Network)
--------------------------------------------------------
Website: https://wwlln.net/
- Real-time lightning detection
- Requires research account (free for academics)
- ASCII format with lat/lon/time

Download steps:
1. Visit https://wwlln.net/
2. Request research access
3. Download archived data

Option 3: Vaisala National Lightning Detection Network
-------------------------------------------------------
Commercial data - contact Vaisala for access

Sample Data Format
------------------
CSV format expected:
timestamp,latitude,longitude,peak_current
2023-01-01T12:00:00,45.5,-122.6,25.3
...

For testing, you can generate synthetic data:
python3 << 'PYEOF'
import numpy as np
import pandas as pd
from datetime import datetime, timedelta

# Generate synthetic lightning strikes (thunderstorm patterns)
np.random.seed(42)
n_strikes = 100000

# Create hotspots (thunderstorm centers)
centers = [
    (30.0, -90.0),   # Gulf Coast
    (35.0, -105.0),  # Southwest US
    (40.0, -85.0),   # Midwest
]

data = []
for center_lat, center_lon in centers:
    n_local = n_strikes // len(centers)
    lats = np.random.normal(center_lat, 2.0, n_local)
    lons = np.random.normal(center_lon, 3.0, n_local)

    for lat, lon in zip(lats, lons):
        data.append({
            'timestamp': datetime(2023, 6, 15) + timedelta(hours=np.random.randint(0, 24)),
            'latitude': lat,
            'longitude': lon,
            'peak_current': np.random.gamma(5, 5)
        })

df = pd.DataFrame(data)
df.to_csv('lightning_synthetic.csv', index=False)
print(f"Generated {len(df)} synthetic lightning strikes")
PYEOF
EOF

    log_success "Lightning data instructions created: $lightning_dir/README.txt"
}

download_wildlife_poaching_data() {
    log_info "========================================="
    log_info "Wildlife Poaching / CITES Trade Data"
    log_info "========================================="

    local wildlife_dir="$DATASET_DIR/wildlife_poaching"
    mkdir -p "$wildlife_dir"

    log_info "Downloading CITES trade database metadata..."

    # CITES trade database is available but requires web interface
    # Download instructions and sample processor

    cat > "$wildlife_dir/README.txt" <<EOF
Wildlife Crime and CITES Trade Data
====================================

Option 1: CITES Trade Database (Recommended)
---------------------------------------------
Website: https://trade.cites.org/
- Official CITES trade database
- Free access via web interface
- Can export to CSV

Download steps:
1. Visit https://trade.cites.org/
2. Select "Download" tab
3. Choose:
   - Years: 2015-2023
   - Purpose: All purposes
   - Trade terms: Live specimens, ivory, etc.
   - Export format: CSV
4. Save as: cites_trade_data.csv

Option 2: Global Biodiversity Information Facility (GBIF)
----------------------------------------------------------
Website: https://www.gbif.org/
- Species occurrence data (includes poaching hotspots)
- Free bulk download with account

Option 3: TRAFFIC (Wildlife Trade Monitoring)
----------------------------------------------
Website: https://www.traffic.org/
- Reports and datasets on illegal wildlife trade
- Some datasets available for research

Sample Datasets:
----------------
- Elephant poaching (MIKE - Monitoring Illegal Killing of Elephants)
- Rhino poaching incidents (African and Asian populations)
- Pangolin trafficking
- Tiger parts trade

Expected CSV Format:
--------------------
year,origin_country,destination_country,species,quantity,latitude,longitude
2020,ZA,CN,Loxodonta africana,2,-25.7479,28.2293
2020,KE,VN,Diceros bicornis,1,-1.2921,36.8219
...

Geocoding Trade Data:
---------------------
If CITES data doesn't include coordinates, use this script to geocode:

python3 << 'PYEOF'
import pandas as pd
from geopy.geocoders import Nominatim
import time

# Load CITES data
df = pd.read_csv('cites_trade_data.csv')

# Simple country centroids for demonstration
# For better accuracy, use port/airport locations
country_coords = {
    'CN': (35.8617, 104.1954),  # China
    'ZA': (-30.5595, 22.9375),  # South Africa
    'KE': (-0.0236, 37.9062),   # Kenya
    # Add more countries...
}

df['origin_lat'] = df['Origin'].map(lambda x: country_coords.get(x, (None, None))[0])
df['origin_lon'] = df['Origin'].map(lambda x: country_coords.get(x, (None, None))[1])

df.to_csv('cites_geocoded.csv', index=False)
PYEOF
EOF

    # Try to download a sample wildlife crime dataset
    log_info "Checking for publicly available wildlife crime data..."

    # GRID-Arendal has some public datasets
    # This is a placeholder - actual URL would need verification

    log_success "Wildlife poaching data instructions created: $wildlife_dir/README.txt"
}

download_infrastructure_failures() {
    log_info "========================================="
    log_info "Infrastructure Failures Data"
    log_info "========================================="

    local infra_dir="$DATASET_DIR/infrastructure_failures"
    mkdir -p "$infra_dir"

    log_info "Downloading sample infrastructure failure datasets..."

    # NYC Open Data - 311 Service Requests (includes infrastructure issues)
    local nyc_311_sample="https://data.cityofnewyork.us/resource/erm2-nwe9.csv?\$limit=50000&complaint_type=Water%20System"
    download_file "$nyc_311_sample" "$infra_dir/nyc_water_complaints.csv"

    # Power outage data - sample from various sources
    cat > "$infra_dir/README.txt" <<EOF
Infrastructure Failure Data Sources
====================================

Option 1: NYC Open Data - 311 Service Requests
-----------------------------------------------
Website: https://data.cityofnewyork.us/
- Water main breaks, power outages, street defects
- CSV/JSON APIs available
- Updated daily

API Examples:
# Water system issues (last 50k)
https://data.cityofnewyork.us/resource/erm2-nwe9.csv?\$limit=50000&complaint_type=Water%20System

# Power outages
https://data.cityofnewyork.us/resource/erm2-nwe9.csv?\$limit=50000&complaint_type=Power

Option 2: DOE Power Outage Data
--------------------------------
Website: https://www.eia.gov/electricity/data/eia861/
- US Department of Energy electricity reliability data
- Annual outage statistics by utility
- Form EIA-861 data

Option 3: City-Specific Open Data Portals
------------------------------------------
- Chicago: https://data.cityofchicago.org/
- Los Angeles: https://data.lacity.org/
- San Francisco: https://datasf.org/
- Seattle: https://data.seattle.gov/

Search for:
- "water main breaks"
- "power outage"
- "infrastructure failures"
- "311 service requests"

Option 4: UK Water Supply Zones
--------------------------------
Website: https://www.data.gov.uk/
- Water supply interruptions
- Burst mains data
- Geographic boundaries

Expected Format:
----------------
timestamp,latitude,longitude,type,severity,duration_hours
2023-01-15T14:30:00,40.7128,-74.0060,water_main_break,major,6.5
2023-01-16T08:00:00,40.7580,-73.9855,power_outage,moderate,2.0
...

Sample Data Generator:
----------------------
python3 << 'PYEOF'
import numpy as np
import pandas as pd
from datetime import datetime, timedelta

np.random.seed(42)

# Generate synthetic infrastructure failures for a city
city_center = (40.7128, -74.0060)  # NYC
n_events = 10000

failure_types = ['water_main_break', 'power_outage', 'gas_leak', 'sewer_backup']
severities = ['minor', 'moderate', 'major']

data = []
for i in range(n_events):
    # Cluster failures along infrastructure lines (grid pattern)
    if np.random.random() < 0.3:  # 30% are clustered
        # Hotspot (old infrastructure area)
        lat = np.random.normal(city_center[0], 0.02)
        lon = np.random.normal(city_center[1], 0.02)
    else:
        # Sparse distribution across city
        lat = city_center[0] + np.random.uniform(-0.1, 0.1)
        lon = city_center[1] + np.random.uniform(-0.1, 0.1)

    data.append({
        'timestamp': datetime(2023, 1, 1) + timedelta(days=np.random.randint(0, 365)),
        'latitude': lat,
        'longitude': lon,
        'type': np.random.choice(failure_types),
        'severity': np.random.choice(severities),
        'duration_hours': np.random.gamma(2, 2)
    })

df = pd.DataFrame(data)
df.to_csv('infrastructure_failures_synthetic.csv', index=False)
print(f"Generated {len(df)} synthetic infrastructure failures")
PYEOF
EOF

    log_success "Infrastructure failure data downloaded/documented: $infra_dir"
}

download_human_trafficking_data() {
    log_info "========================================="
    log_info "Human Trafficking Database (HTD)"
    log_info "========================================="

    local trafficking_dir="$DATASET_DIR/human_trafficking"
    mkdir -p "$trafficking_dir"

    log_warning "Human trafficking data is sensitive and may require approval"

    cat > "$trafficking_dir/README.txt" <<EOF
Human Trafficking Data Sources
===============================

IMPORTANT: This data is sensitive and should be used responsibly for research
purposes only. Always follow ethical guidelines and data use agreements.

Option 1: Counter-Trafficking Data Collaborative (CTDC)
--------------------------------------------------------
Website: https://www.ctdatacollaborative.org/
- Global human trafficking database
- 100,000+ cases from 171 countries
- Requires registration (FREE for researchers)

Download steps:
1. Visit https://www.ctdatacollaborative.org/
2. Click "Access the Database"
3. Register for free account (academic/research)
4. Download anonymized datasets
5. Read and accept data use agreement

Data includes:
- Victim demographics
- Trafficking routes (origin → transit → destination)
- Exploitation types
- Recruitment methods
- Geographic coordinates (aggregated for privacy)

Option 2: UNODC Global Report on Trafficking in Persons
--------------------------------------------------------
Website: https://www.unodc.org/unodc/data-and-analysis/glotip.html
- Statistical data by country
- Victim profiles and trafficking flows
- Available as PDF reports and Excel datasets

Option 3: IOM Counter-Trafficking Module Database
--------------------------------------------------
Website: https://www.iom.int/counter-trafficking
- International Organization for Migration data
- Case management data (anonymized)
- May require institutional access

Option 4: Published Research Datasets
--------------------------------------
Search Google Scholar/Zenodo for:
- "human trafficking dataset"
- "trafficking routes GIS"
- "victim assistance database"

Some researchers have published anonymized versions for academic use.

Expected Format:
----------------
case_id,year,origin_country,transit_country,destination_country,victim_age,gender,exploitation_type,origin_lat,origin_lon,dest_lat,dest_lon
HTD001,2019,TH,MM,CN,25,F,labor,-,98.9500,22.8600,101.7800
HTD002,2020,MX,-,US,17,F,sexual,19.4326,-99.1332,32.7157,-117.1611
...

Note: Coordinates are often aggregated to country/regional level for victim protection

Privacy Considerations:
-----------------------
- Data is typically aggregated or anonymized
- Exact locations are obscured
- Use only for research, not investigative purposes
- Follow all data use agreements
- Do not attempt to de-anonymize

Synthetic Data for Testing (Non-sensitive):
--------------------------------------------
python3 << 'PYEOF'
import numpy as np
import pandas as pd

# Generate synthetic trafficking flow data (hotspot patterns)
# This is FICTIONAL data for algorithm testing only

np.random.seed(42)

# Known trafficking route hotspots (generalized)
routes = [
    {'origin': 'SE Asia', 'origin_coords': (13.7563, 100.5018), 'dest': 'Middle East', 'dest_coords': (25.2048, 55.2708)},
    {'origin': 'E Europe', 'origin_coords': (50.4501, 30.5234), 'dest': 'W Europe', 'dest_coords': (48.8566, 2.3522)},
    {'origin': 'C America', 'origin_coords': (14.6349, -90.5069), 'dest': 'N America', 'dest_coords': (34.0522, -118.2437)},
    {'origin': 'W Africa', 'origin_coords': (6.5244, 3.3792), 'dest': 'Europe', 'dest_coords': (41.9028, 12.4964)},
]

data = []
case_id = 1000

for route in routes:
    n_cases = np.random.randint(50, 200)

    for _ in range(n_cases):
        # Add noise to coordinates (regional aggregation)
        origin_lat = route['origin_coords'][0] + np.random.normal(0, 3)
        origin_lon = route['origin_coords'][1] + np.random.normal(0, 3)
        dest_lat = route['dest_coords'][0] + np.random.normal(0, 2)
        dest_lon = route['dest_coords'][1] + np.random.normal(0, 2)

        data.append({
            'case_id': f'SYN{case_id:05d}',
            'year': np.random.randint(2015, 2024),
            'origin': route['origin'],
            'destination': route['dest'],
            'origin_lat': origin_lat,
            'origin_lon': origin_lon,
            'dest_lat': dest_lat,
            'dest_lon': dest_lon,
            'victim_age': np.random.randint(15, 45),
            'gender': np.random.choice(['M', 'F']),
            'exploitation_type': np.random.choice(['labor', 'sexual', 'forced_marriage', 'organ'])
        })
        case_id += 1

df = pd.DataFrame(data)
df.to_csv('trafficking_synthetic.csv', index=False)
print(f"Generated {len(df)} SYNTHETIC trafficking cases for testing")
print("NOTE: This is fictional data for algorithm testing only!")
PYEOF
EOF

    log_success "Human trafficking data documentation created: $trafficking_dir/README.txt"
    log_warning "Please review README.txt for data access instructions and ethical guidelines"
}

download_admin_boundaries() {
    log_info "========================================="
    log_info "Natural Earth Admin Boundaries"
    log_info "========================================="

    local boundaries_dir="$DATASET_DIR/admin_boundaries"
    mkdir -p "$boundaries_dir"

    log_info "Downloading Natural Earth administrative boundary shapefiles..."
    log_info "Natural Earth is a public domain map dataset available at naturalearthdata.com"

    # Download Admin 0 - Countries (10m resolution, detailed)
    local countries_10m_url="https://naciscdn.org/naturalearth/10m/cultural/ne_10m_admin_0_countries.zip"
    local countries_10m_zip="$boundaries_dir/ne_10m_admin_0_countries.zip"

    if download_file "$countries_10m_url" "$countries_10m_zip"; then
        log_info "Extracting countries (10m resolution)..."
        unzip -o -q "$countries_10m_zip" -d "$boundaries_dir/ne_10m_admin_0_countries"
        log_success "Extracted: ne_10m_admin_0_countries/"
    fi

    # Download Admin 1 - States/Provinces (10m resolution, detailed)
    local states_10m_url="https://naciscdn.org/naturalearth/10m/cultural/ne_10m_admin_1_states_provinces.zip"
    local states_10m_zip="$boundaries_dir/ne_10m_admin_1_states_provinces.zip"

    if download_file "$states_10m_url" "$states_10m_zip"; then
        log_info "Extracting states/provinces (10m resolution)..."
        unzip -o -q "$states_10m_zip" -d "$boundaries_dir/ne_10m_admin_1_states_provinces"
        log_success "Extracted: ne_10m_admin_1_states_provinces/"
    fi

    # Download Admin 0 - Countries (50m resolution, medium detail)
    local countries_50m_url="https://naciscdn.org/naturalearth/50m/cultural/ne_50m_admin_0_countries.zip"
    local countries_50m_zip="$boundaries_dir/ne_50m_admin_0_countries.zip"

    if download_file "$countries_50m_url" "$countries_50m_zip"; then
        log_info "Extracting countries (50m resolution)..."
        unzip -o -q "$countries_50m_zip" -d "$boundaries_dir/ne_50m_admin_0_countries"
        log_success "Extracted: ne_50m_admin_0_countries/"
    fi

    # Create README
    cat > "$boundaries_dir/README.txt" <<EOF
Natural Earth Administrative Boundaries
========================================

Source: Natural Earth (https://www.naturalearthdata.com/)
License: Public Domain

This dataset contains global administrative boundary shapefiles from Natural Earth.
Natural Earth is a public domain map dataset created by volunteer cartographers
and GIS analysts.

Downloaded Files
----------------

1. ne_10m_admin_0_countries/
   - Country boundaries at 1:10 million scale (most detailed)
   - ~250 countries and territories
   - File: ne_10m_admin_0_countries.shp
   - ~10 MB uncompressed

2. ne_10m_admin_1_states_provinces/
   - State/province boundaries at 1:10 million scale
   - ~4,600 first-level administrative divisions
   - File: ne_10m_admin_1_states_provinces.shp
   - ~50 MB uncompressed

3. ne_50m_admin_0_countries/
   - Country boundaries at 1:50 million scale (medium detail)
   - Smaller file size, good for testing
   - File: ne_50m_admin_0_countries.shp
   - ~2 MB uncompressed

Usage with CountingGloBiMap
---------------------------

These shapefiles can be used to:
1. Rasterize administrative boundaries into GloBiMap
2. Test polygon masking and region queries
3. Aggregate spatial data by country/state
4. Benchmark polygon processing performance

Example (Python):
import geopandas as gpd
countries = gpd.read_file('ne_10m_admin_0_countries/ne_10m_admin_0_countries.shp')
# Use with rasterization or polygon masking

Example (C++ experiments):
Update POLYGON_DATASETS in dataset_config.hpp to include:
  "admin_boundaries/ne_10m_admin_0_countries/ne_10m_admin_0_countries"

Attribution
-----------
Made with Natural Earth. Free vector and raster map data @ naturalearthdata.com.

Data Structure
--------------
Each shapefile includes attributes like:
- NAME: Country/region name
- ISO_A2/ISO_A3: ISO country codes
- CONTINENT: Continent name
- REGION_UN: UN region classification
- POP_EST: Population estimate
- GDP_MD: GDP in millions USD
- And many more (50+ attributes)

For full documentation, visit:
https://www.naturalearthdata.com/downloads/

EOF

    # Count features in shapefiles
    log_info "Dataset summary:"
    for shp_dir in "$boundaries_dir"/ne_*_admin_*/; do
        if [ -d "$shp_dir" ]; then
            local shp_name=$(basename "$shp_dir")
            local shp_file="$shp_dir${shp_name}.shp"
            if [ -f "$shp_file" ]; then
                log_success "  ✓ $shp_name"
            fi
        fi
    done

    log_success "Natural Earth admin boundaries downloaded: $boundaries_dir"
    log_info "See $boundaries_dir/README.txt for usage information"
}

###############################################################################
# Additional Utility Functions
###############################################################################

create_dataset_converter() {
    log_info "Creating dataset conversion utilities..."

    local utils_dir="$DATASET_DIR/utils"
    mkdir -p "$utils_dir"

    # Python script to convert CSV to HDF5 for efficient loading
    cat > "$utils_dir/csv_to_hdf5.py" <<'EOF'
#!/usr/bin/env python3
"""
Convert CSV spatial datasets to HDF5 format for efficient loading in C++ experiments.

Usage:
    python csv_to_hdf5.py input.csv output.h5 --lat-col latitude --lon-col longitude
"""

import argparse
import pandas as pd
import h5py
import numpy as np

def csv_to_hdf5(csv_file, hdf5_file, lat_col='latitude', lon_col='longitude'):
    """Convert CSV with lat/lon columns to HDF5 coordinate array."""
    print(f"Reading {csv_file}...")
    df = pd.read_csv(csv_file)

    if lat_col not in df.columns or lon_col not in df.columns:
        print(f"ERROR: Columns '{lat_col}' and/or '{lon_col}' not found")
        print(f"Available columns: {', '.join(df.columns)}")
        return

    # Extract coordinates
    coords = df[[lat_col, lon_col]].values

    # Remove NaN values
    coords = coords[~np.isnan(coords).any(axis=1)]

    print(f"Found {len(coords)} valid coordinate pairs")

    # Save to HDF5
    with h5py.File(hdf5_file, 'w') as f:
        f.create_dataset('coords', data=coords, compression='gzip')

        # Store metadata
        f.attrs['source_file'] = csv_file
        f.attrs['num_points'] = len(coords)
        f.attrs['lat_col'] = lat_col
        f.attrs['lon_col'] = lon_col

    print(f"Saved to {hdf5_file}")
    print(f"Dataset shape: {coords.shape}")

if __name__ == '__main__':
    parser = argparse.ArgumentParser(description='Convert CSV to HDF5')
    parser.add_argument('input_csv', help='Input CSV file')
    parser.add_argument('output_h5', help='Output HDF5 file')
    parser.add_argument('--lat-col', default='latitude', help='Latitude column name')
    parser.add_argument('--lon-col', default='longitude', help='Longitude column name')

    args = parser.parse_args()

    csv_to_hdf5(args.input_csv, args.output_h5, args.lat_col, args.lon_col)
EOF

    chmod +x "$utils_dir/csv_to_hdf5.py"

    log_success "Conversion utilities created in: $utils_dir"
}

###############################################################################
# GDELT: Global Database of Events, Language, and Tone
###############################################################################

download_gdelt_data() {
    local dataset_name="gdelt"
    local gdelt_dir="$DATASET_DIR/$dataset_name"
    local readme="$gdelt_dir/README.txt"

    log_info "Downloading GDELT (Global Database of Events, Language, and Tone)..."

    mkdir -p "$gdelt_dir"

    # Check if already downloaded
    if [ -f "$gdelt_dir/gdelt_events_sample.csv" ]; then
        log_warning "GDELT sample already exists, skipping download"
        return 0
    fi

    # GDELT 2.0 is updated every 15 minutes with global event data
    # We'll download last year of data sampled every 6 hours (about 1.5-2 GB compressed)

    log_info "Downloading GDELT 2.0 Events (last 12 months)..."
    log_info "This may take 10-20 minutes (1.5-2 GB compressed)..."

    # Get the master file list to find recent files
    local master_list="$gdelt_dir/masterfilelist.txt"

    if command -v wget &> /dev/null; then
        wget -q -O "$master_list" "http://data.gdeltproject.org/gdeltv2/masterfilelist.txt" || {
            log_error "Failed to download GDELT master file list"
            return 1
        }
    else
        curl -s -o "$master_list" "http://data.gdeltproject.org/gdeltv2/masterfilelist.txt" || {
            log_error "Failed to download GDELT master file list"
            return 1
        }
    fi

    # Get last 365 days of export files (events data)
    # GDELT publishes files every 15 minutes, so ~35,040 files per year
    # We'll sample every 6 hours (4 files per day = 1,460 files per year) for manageable size

    log_info "Selecting sample files (every 6 hours for 365 days)..."

    local sample_files=$(grep "\.export\.CSV\.zip" "$master_list" | tail -35040 | awk 'NR % 24 == 0' | head -1460)
    local file_count=$(echo "$sample_files" | wc -l)

    log_info "Downloading $file_count GDELT event files..."

    local downloaded=0
    local combined_csv="$gdelt_dir/gdelt_events_sample.csv"
    local temp_dir="$gdelt_dir/temp"
    mkdir -p "$temp_dir"

    # Write CSV header
    echo "GlobalEventID,Day,MonthYear,Year,FractionDate,Actor1Code,Actor1Name,Actor1CountryCode,Actor1Type1Code,Actor2Code,Actor2Name,Actor2CountryCode,Actor2Type1Code,IsRootEvent,EventCode,EventBaseCode,EventRootCode,QuadClass,GoldsteinScale,NumMentions,NumSources,NumArticles,AvgTone,Actor1Geo_Type,Actor1Geo_FullName,Actor1Geo_CountryCode,Actor1Geo_ADM1Code,Actor1Geo_Lat,Actor1Geo_Long,Actor1Geo_FeatureID,Actor2Geo_Type,Actor2Geo_FullName,Actor2Geo_CountryCode,Actor2Geo_ADM1Code,Actor2Geo_Lat,Actor2Geo_Long,Actor2Geo_FeatureID,ActionGeo_Type,ActionGeo_FullName,ActionGeo_CountryCode,ActionGeo_ADM1Code,ActionGeo_Lat,ActionGeo_Long,ActionGeo_FeatureID,DATEADDED,SOURCEURL" > "$combined_csv"

    while IFS= read -r line; do
        local url=$(echo "$line" | awk '{print $3}')
        local filename=$(basename "$url")
        local temp_zip="$temp_dir/$filename"

        downloaded=$((downloaded + 1))
        echo -ne "\r  Progress: $downloaded/$file_count files"

        # Download file
        if command -v wget &> /dev/null; then
            wget -q -O "$temp_zip" "$url" 2>/dev/null || continue
        else
            curl -s -o "$temp_zip" "$url" 2>/dev/null || continue
        fi

        # Unzip and append to combined file (skip header)
        unzip -q -p "$temp_zip" >> "$combined_csv" 2>/dev/null || true

        rm -f "$temp_zip"
    done <<< "$sample_files"

    echo ""  # New line after progress

    # Cleanup
    rm -rf "$temp_dir"
    rm -f "$master_list"

    # Get file stats
    local line_count=$(wc -l < "$combined_csv")
    local file_size=$(du -h "$combined_csv" | cut -f1)

    log_success "Downloaded GDELT sample data"
    log_info "  Events: $((line_count - 1))"
    log_info "  File size: $file_size"
    log_info "  Time period: Last 365 days (6-hour intervals)"

    # Create README
    cat > "$readme" <<'EOF'
GDELT: Global Database of Events, Language, and Tone
=====================================================

Dataset: GDELT 2.0 Events Database (sample)
Source: http://data.gdeltproject.org/
Update Frequency: Every 15 minutes (this is a 12-month sample)

DESCRIPTION:
------------
The GDELT Project monitors print, broadcast, and web news media in over 100
languages from across every country in the world. It identifies people,
locations, organizations, themes, emotions, and events driving global society.

This sample contains 12 months of event data sampled at 6-hour intervals,
providing comprehensive global coverage of significant events including:
- Political conflicts and cooperation
- Protests and demonstrations
- Natural disasters
- Economic events
- Social movements

SPATIAL CHARACTERISTICS:
------------------------
- Sparse globally (most grid cells have no events)
- Strong hotspots in:
  * Conflict zones (Middle East, Ukraine, etc.)
  * Political centers (Washington DC, Brussels, etc.)
  * Major cities with protests/demonstrations
  * Natural disaster locations

PERFECT FOR COUNTING GLOBIMAP:
-------------------------------
- Highly sparse spatial distribution (< 0.01% of globe)
- Clear hotspot patterns (recurring event locations)
- Varying event frequencies (some locations have 1 event, others hundreds)
- Global scale (requires efficient compression)

FILE FORMAT:
------------
gdelt_events_sample.csv - Tab-separated CSV with columns:
  - GlobalEventID: Unique event identifier
  - Day: Date of event (YYYYMMDD)
  - Actor1/Actor2: Entities involved in event
  - EventCode: CAMEO event classification code
  - QuadClass: Event category (1=verbal coop, 2=material coop, 3=verbal conflict, 4=material conflict)
  - GoldsteinScale: Event impact (-10 to +10)
  - ActionGeo_Lat/ActionGeo_Long: Event location
  - NumMentions: Number of mentions in source documents
  - AvgTone: Average tone of coverage (-100 to +100)

USAGE EXAMPLES:
---------------
1. Count events by location:
   - Filter events by QuadClass (e.g., conflicts only)
   - Convert lat/lon to pixel coordinates
   - Insert into CountingGloBiMap
   - Query event density in regions

2. Analyze conflict hotspots:
   - Filter for QuadClass=4 (material conflict)
   - Weight by GoldsteinScale (negative = more severe)
   - Identify regions with recurring conflicts

3. Track event evolution:
   - Group by Day field
   - Compare spatial distributions over time
   - Identify emerging hotspots

CAMEO EVENT CODES (EventCode):
-------------------------------
Major categories:
  01-05: Verbal cooperation (appeal, express intent, agree, consult)
  06-08: Material cooperation (aid, cooperate, yield)
  09-13: Verbal conflict (demand, disapprove, reject, threaten, protest)
  14-20: Material conflict (exhibit force, reduce relations, coerce, assault, fight, unconventional violence)

QuadClass simplifies to:
  1 = Verbal cooperation
  2 = Material cooperation
  3 = Verbal conflict
  4 = Material conflict

REFERENCES:
-----------
- GDELT Project: https://www.gdeltproject.org/
- Documentation: https://www.gdeltproject.org/data.html#documentation
- CAMEO Codebook: https://www.gdeltproject.org/data/documentation/CAMEO.Manual.1.1b3.pdf
- Academic Papers: https://www.gdeltproject.org/about.html#publications

CITATION:
---------
Leetaru, K., & Schrodt, P. A. (2013). GDELT: Global data on events, location,
and tone, 1979–2012. ISA annual convention (Vol. 2, No. 4, pp. 1-49).

ETHICAL CONSIDERATIONS:
-----------------------
- GDELT data is aggregated from news sources and may contain biases
- Event locations may be approximate or inferred
- Not all global events are equally covered by media
- Use for research and analysis, not for targeting or surveillance
- Respect privacy and human rights in all applications

EOF

    log_success "GDELT dataset documentation created: $readme"
}

list_datasets() {
    cat <<EOF

Available Datasets:
===================

1. covid19              - COVID-19 case data (Johns Hopkins CSSE)
                         Sparse globally, hotspots in affected regions
                         ~2-3 MB, CSV format, auto-download

2. gdelt                - GDELT global events database (conflicts, protests, etc.)
                         Highly sparse, strong hotspots in conflict/political zones
                         ~1.5-2 GB compressed, 365-day sample, auto-download

3. lightning            - Lightning strike data (NOAA)
                         Sparse but concentrated in storm regions
                         Requires manual download (see README)

4. infrastructure       - Infrastructure failures (power, water)
                         Sparse failures, hotspots in aging infrastructure
                         NYC sample auto-download, more sources in README

5. admin_boundaries     - Natural Earth global administrative boundaries
                         Vector data: country & state/province polygons
                         ~60 MB total, shapefiles, auto-download

Usage:
  ./download_datasets.sh                # Download all available datasets
  ./download_datasets.sh covid19        # Download specific dataset
  ./download_datasets.sh gdelt          # Download GDELT events data
  ./download_datasets.sh admin_boundaries  # Download admin boundary shapefiles
  ./download_datasets.sh --list         # Show this list

EOF
}

###############################################################################
# Main Script
###############################################################################

main() {
    echo ""
    echo "╔════════════════════════════════════════════════════════════╗"
    echo "║    CountingGloBiMap Dataset Downloader                    ║"
    echo "║    Spatial datasets with sparse data + hotspot patterns   ║"
    echo "╚════════════════════════════════════════════════════════════╝"
    echo ""

    # Check for required commands
    if ! command -v wget &> /dev/null && ! command -v curl &> /dev/null; then
        log_error "Either wget or curl is required"
        exit 1
    fi

    # Parse arguments
    if [ "$1" == "--list" ] || [ "$1" == "-l" ]; then
        list_datasets
        exit 0
    fi

    # Download specific dataset or all
    if [ -z "$1" ]; then
        log_info "No dataset specified, downloading all available datasets..."
        download_covid19_data
        download_gdelt_data
        download_lightning_data
        download_infrastructure_failures
        download_admin_boundaries
        create_dataset_converter
    else
        case "$1" in
            covid19)
                download_covid19_data
                ;;
            gdelt)
                download_gdelt_data
                ;;
            lightning)
                download_lightning_data
                ;;
            infrastructure|infra)
                download_infrastructure_failures
                ;;
            admin_boundaries|admin|boundaries)
                download_admin_boundaries
                ;;
            *)
                log_error "Unknown dataset: $1"
                list_datasets
                exit 1
                ;;
        esac
        create_dataset_converter
    fi

    echo ""
    log_success "Dataset download complete!"
    log_info "Datasets location: $DATASET_DIR"
    echo ""
    log_info "Next steps:"
    echo "  1. Review README.txt files in each dataset directory"
    echo "  2. For datasets requiring manual download, follow instructions"
    echo "  3. Configure dataset paths for C++ experiments:"
    echo "     ./configure_datasets.sh"
    echo "  4. Run Python examples:"
    echo "     python examples/example_gdelt_analysis.py"
    echo "     python examples/example_covid19_analysis.py"
    echo "  5. Convert CSV to HDF5 (optional, for C++ experiments):"
    echo "     python datasets/utils/csv_to_hdf5.py input.csv output.h5"
    echo "  6. Rebuild and run C++ experiments (optional):"
    echo "     cd build && make && ./test_datasets"
    echo ""
}

# Run main function
main "$@"
