#!/bin/bash

# Create a directory for the graphs
GRAPH_DIR="graphs"
mkdir -p "$GRAPH_DIR"

# Array of graph URLs
GRAPH_URLS=(
    "https://suitesparse-collection-website.herokuapp.com/MM/DIMACS10/road_usa.tar.gz"
    "https://suitesparse-collection-website.herokuapp.com/MM/DIMACS10/europe_osm.tar.gz"
    "https://suitesparse-collection-website.herokuapp.com/MM/DIMACS10/asia_osm.tar.gz"
    "https://suitesparse-collection-website.herokuapp.com/MM/DIMACS10/hugebubbles-00020.tar.gz"
)

# Download, extract, and clean up
for URL in "${GRAPH_URLS[@]}"; do
    FILENAME=$(basename "$URL")
    echo "Downloading $FILENAME..."
    if wget "$URL" -O "$FILENAME"; then
        echo "Extracting $FILENAME..."
        if tar -xzf "$FILENAME" -C "$GRAPH_DIR" 2>/dev/null || gunzip -c "$FILENAME" > "$GRAPH_DIR/${FILENAME%.gz}"; then
            echo "Extraction successful for $FILENAME."
            rm "$FILENAME"
        else
            echo "Failed to extract $FILENAME."
            exit 1
        fi
    else
        echo "Failed to download $FILENAME."
        exit 1
    fi
done

# Check if the graphs directory contains files
if [ "$(ls -A "$GRAPH_DIR")" ]; then
    echo "All graphs downloaded and extracted successfully into the '$GRAPH_DIR' directory."
else
    echo "No files were downloaded or extracted. Please check the script and URLs."
    exit 1
fi