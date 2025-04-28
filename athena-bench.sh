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
    python3 bench.py -args "-f graphs/road_usa.mtx.sg -n 2"          -t 1 2 4 8 16 32 64 128 256 512 -d no -o athena/DO_TD/usa -a DO_TD -p ithaca_ht
    echo 1 > $PROGRESS_FILE
else
    echo "Skipping DO_TD road_usa benchmarks, already completed."
fi

get_progress

if [ $progress -eq 1 ]; then
    echo "Running DO_TD road-europe benchmarks..."
    python3 bench.py -args "-f graphs/europe_osm.mtx.sg -n 2"           -t 1 2 4 8 16 32 64 128 256 512 -d no -o athena/DO_TD/europe -a DO_TD -p ithaca_ht
    echo 2 > $PROGRESS_FILE
else
    echo "Skipping DO_TD road-europe benchmarks, already completed."
fi

get_progress

if [ $progress -eq 2 ]; then
    echo "Running DO_TD road-asia benchmarks..."
    python3 bench.py -args "-f graphs/asia_osm.mtx.sg -n 2"          -t 1 2 4 8 16 32 64 128 256 512 -d no -o athena/DO_TD/asia -a DO_TD -p ithaca_ht
    echo 3 > $PROGRESS_FILE
else
    echo "Skipping DO_TD road-asia benchmarks, already completed."
fi

get_progress

if [ $progress -eq 3 ]; then
    echo "Running DO_TD hugebubbles-00020 benchmarks..."
    python3 bench.py -args "-f graphs/hugebubbles-00020.mtx.sg -n 2" -t 1 2 4 8 16 32 64 128 256 512 -d no -o athena/DO_TD/hugebubbles -a DO_TD -p ithaca_ht
    echo 4 > $PROGRESS_FILE
else
    echo "Skipping DO_TD hugebubbles-00020 benchmarks, already completed."
fi

get_progress




# ##### Kronecker benchmarks #####

if [ $progress -eq 4 ]; then
    echo "Running kronecker benchmarks - DO"
    python3 bench.py -args "-g 24 -n 2" -t 1 2 4 8 16 32 64 128 256 512 -d no -o athena/kronecker -a DO -p ithaca_ht
    echo 5 > $PROGRESS_FILE
else
    echo "Skipping kronecker DO benchmarks, already completed."
fi

get_progress

if [ $progress -eq 5 ]; then
    echo "Running kronecker benchmarks - DO_TD"
    python3 bench.py -args "-g 24 -n 2" -t 1 2 4 8 16 32 64 128 256 512 -d no -o athena/kronecker -a DO_TD -p ithaca_ht
    echo 6 > $PROGRESS_FILE
else
    echo "Skipping kronecker DO_TD benchmarks, already completed."
fi

get_progress
loop_progress_count=6

for algorithm in "DCBO_FAA_BOD" "DCBO_FAA_DAD" "DCBO_FAA_BATCHING" "DCBO_FAA"; do
    if [ $progress -eq $loop_progress_count ]; then
        echo "Running kronecker benchmarks - $algorithm"
        python3 bench.py -args "-g 24 -n 2" -t 1 32 64 128 256 512 -d both -bs 16 32 64 -sq 64 128 -o athena/kronecker -a $algorithm -p ithaca_ht
        echo $(($progress + 1)) > $PROGRESS_FILE
    else
        echo "Skipping kronecker $algorithm benchmarks, already completed."
    fi
    loop_progress_count=$(($loop_progress_count + 1))
    get_progress
done





##### FAA benchmarks #####

get_progress

if [ $progress -eq 10 ]; then
    echo "Running FAA road_usa benchmarks..."
    python3 bench.py -args "-f graphs/road_usa.mtx.sg -n 2"          -t 1 32 64 128 256 512 -d both -o athena/FAA/usa -a FAA -p ithaca_ht
    echo 11 > $PROGRESS_FILE
else
    echo "Skipping FAA road_usa benchmarks, already completed."
fi

get_progress

if [ $progress -eq 11 ]; then
    echo "Running FAA road-europe benchmarks..."
    python3 bench.py -args "-f graphs/europe_osm.mtx.sg -n 2"           -t 1 32 64 128 256 512 -d both -o athena/FAA/europe -a FAA -p ithaca_ht
    echo 12 > $PROGRESS_FILE
else
    echo "Skipping FAA road-europe benchmarks, already completed."
fi

get_progress

if [ $progress -eq 12 ]; then
    echo "Running FAA road-asia benchmarks..."
    python3 bench.py -args "-f graphs/asia_osm.mtx.sg -n 2"          -t 1 32 64 128 256 512 -d both -o athena/FAA/asia -a FAA -p ithaca_ht
    echo 13 > $PROGRESS_FILE
else
    echo "Skipping FAA road-asia benchmarks, already completed."
fi

get_progress

if [ $progress -eq 13 ]; then
    echo "Running FAA hugebubbles-00020 benchmarks..."
    python3 bench.py -args "-f graphs/hugebubbles-00020.mtx.sg -n 2" -t 1 32 64 128 256 512 -d both -o athena/FAA/hugebubbles -a FAA -p ithaca_ht
    echo 14 > $PROGRESS_FILE
else
    echo "Skipping FAA hugebubbles-00020 benchmarks, already completed."
fi

get_progress

if [ $progress -eq 14 ]; then
    echo "Running kronecker benchmarks - FAA"
    python3 bench.py -args "-g 24 -n 2" -t 1 32 64 128 256 512 -d both -o athena/kronecker -a FAA -p ithaca_ht
    echo 15 > $PROGRESS_FILE
else
    echo "Skipping kronecker FAA benchmarks, already completed."
fi