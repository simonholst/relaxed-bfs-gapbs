python3 bench.py -args "-f graphs/road_usa/road_usa.mtx -n 64"                   -t 1 16 32 64 128 256 -sq 64 128 -bs 16 32 64 -d both -o athena_results/usa           -a DCBO_FAA_BATCHING DCBO_FAA DCBO_FAA_PREDEQ DCBO_FAA_DEPTH_THRESH DO
python3 bench.py -args "-f graphs/europe_osm/europe_osm.mtx -n 64"               -t 1 16 32 64 128 256 -sq 64 128 -bs 16 32 64 -d both -o athena_results/europe        -a DCBO_FAA_BATCHING DCBO_FAA DCBO_FAA_PREDEQ DCBO_FAA_DEPTH_THRESH DO
python3 bench.py -args "-f graphs/asia_osm/asia_osm.mtx -n 64"                   -t 1 16 32 64 128 256 -sq 64 128 -bs 16 32 64 -d both -o athena_results/asia          -a DCBO_FAA_BATCHING DCBO_FAA DCBO_FAA_PREDEQ DCBO_FAA_DEPTH_THRESH DO
python3 bench.py -args "-f graphs/hugebubbles-00020/hugebubbles-00020.mtx -n 64" -t 1 16 32 64 128 256 -sq 64 128 -bs 16 32 64 -d both -o athena_results/hugebubbles   -a DCBO_FAA_BATCHING DCBO_FAA DCBO_FAA_PREDEQ DCBO_FAA_DEPTH_THRESH DO

python3 bench.py -args "-f graphs/road_usa/road_usa.mtx -n 16"                   -t 1  -d both -o athena_results/usa         -a Sequential
python3 bench.py -args "-f graphs/europe_osm/europe_osm.mtx -n 16"               -t 1  -d both -o athena_results/europe      -a Sequential
python3 bench.py -args "-f graphs/asia_osm/asia_osm.mtx -n 16"                   -t 1  -d both -o athena_results/asia        -a Sequential
python3 bench.py -args "-f graphs/hugebubbles-00020/hugebubbles-00020.mtx -n 16" -t 1  -d both -o athena_results/hugebubbles -a Sequential