PROGRESS_FILE="bench-progress.txt"

touch $PROGRESS_FILE

get_progress() {
    progress=$(cat bench-progress.txt)
    if [ ! -s $PROGRESS_FILE ]; then
        progress=0
    elif ! [[ "$progress" =~ ^[0-9]+$ ]]; then
        echo "Error: $PROGRESS_FILE does not contain a valid number."
        exit 1
    fi
}

get_progress

##### DO_TD benchmarks #####

if [ $progress -eq 0 ]; then
    echo "Running DO_TD road_usa benchmarks..."
    python3 bench.py -args "-f graphs/road_usa.mtx.sg -n 32"          -t 1 2 4 8 16 32 64 128 256 512 -d no -o athena/DO_TD/usa -a DO_TD -p athena_ht
    echo 1 > $PROGRESS_FILE
else
    echo "Skipping DO_TD road_usa benchmarks, already completed."
fi

get_progress

if [ $progress -eq 1 ]; then
    echo "Running DO_TD road-europe benchmarks..."
    python3 bench.py -args "-f graphs/europe_osm.mtx.sg -n 32"           -t 1 2 4 8 16 32 64 128 256 512 -d no -o athena/DO_TD/europe -a DO_TD -p athena_ht
    echo 2 > $PROGRESS_FILE
else
    echo "Skipping DO_TD road-europe benchmarks, already completed."
fi

get_progress

if [ $progress -eq 2 ]; then
    echo "Running DO_TD road-asia benchmarks..."
    python3 bench.py -args "-f graphs/asia_osm.mtx.sg -n 32"          -t 1 2 4 8 16 32 64 128 256 512 -d no -o athena/DO_TD/asia -a DO_TD -p athena_ht
    echo 3 > $PROGRESS_FILE
else
    echo "Skipping DO_TD road-asia benchmarks, already completed."
fi

get_progress

if [ $progress -eq 3 ]; then
    echo "Running DO_TD hugebubbles-00020 benchmarks..."
    python3 bench.py -args "-f graphs/hugebubbles-00020.mtx.sg -n 32" -t 1 2 4 8 16 32 64 128 256 512 -d no -o athena/DO_TD/hugebubbles -a DO_TD -p athena_ht
    echo 4 > $PROGRESS_FILE
else
    echo "Skipping DO_TD hugebubbles-00020 benchmarks, already completed."
fi

get_progress

if [ $progress -eq 4 ]; then
    echo "Running kronecker benchmarks - DO_TD"
    python3 bench.py -args "-g 24 -n 32" -t 1 2 4 8 16 32 64 128 256 512 -d no -o athena/kronecker -a DO_TD -p athena_ht
    echo 5 > $PROGRESS_FILE
else
    echo "Skipping kronecker DO_TD benchmarks, already completed."
fi