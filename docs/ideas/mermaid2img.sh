#!/bin/bash

# Convert Mermaid diagrams to SVG and/or PNG

set -e

usage() {
    echo "Usage: $0 <input.mermaid> [--svg] [--png] [--both]"
    echo ""
    echo "Options:"
    echo "  --svg   Generate SVG output (default if no format specified)"
    echo "  --png   Generate PNG output"
    echo "  --both  Generate both SVG and PNG"
    exit 1
}

if [[ $# -lt 1 ]]; then
    usage
fi

INPUT="$1"
shift

if [[ ! -f "$INPUT" ]]; then
    echo "Error: File '$INPUT' not found"
    exit 1
fi

BASENAME="${INPUT%.*}"
GEN_SVG=false
GEN_PNG=false

# Parse options
while [[ $# -gt 0 ]]; do
    case "$1" in
        --svg)  GEN_SVG=true ;;
        --png)  GEN_PNG=true ;;
        --both) GEN_SVG=true; GEN_PNG=true ;;
        *)      echo "Unknown option: $1"; usage ;;
    esac
    shift
done

# Default to SVG if no format specified
if [[ "$GEN_SVG" == false && "$GEN_PNG" == false ]]; then
    GEN_SVG=true
fi

# Generate outputs
if [[ "$GEN_SVG" == true ]]; then
    echo "Generating ${BASENAME}.svg..."
    npx --yes @mermaid-js/mermaid-cli -i "$INPUT" -o "${BASENAME}.svg"
fi

if [[ "$GEN_PNG" == true ]]; then
    echo "Generating ${BASENAME}.png..."
    npx --yes @mermaid-js/mermaid-cli -i "$INPUT" -o "${BASENAME}.png"
fi

echo "Done!"
