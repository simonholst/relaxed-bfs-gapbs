GRAPH_DIR="graphs"
make converter
cd "$GRAPH_DIR" || exit 1
find . -type f | grep -v "coord" | grep ".mtx" | while read -r FILE; do
    BASENAME=$(basename "$FILE")
    ./../bin/converter -f "$FILE" -b "${BASENAME}.sg"
    if [ $? -ne 0 ]; then
        echo "Error: Conversion failed for file '$FILE'."
        exit 1
    fi
done

echo "All graphs converted successfully into the '$GRAPH_DIR' directory."