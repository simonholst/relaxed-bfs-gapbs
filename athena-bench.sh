python3 bench.py -args "-f graphs/road_usa/road_usa.mtx -n 32"                   -t 2 4 8 16 -o do_complementary/usa         -a DO -p athena_ht
python3 bench.py -args "-f graphs/europe_osm/europe_osm.mtx -n 32"               -t 2 4 8 16 -o do_complementary/europe      -a DO -p athena_ht
python3 bench.py -args "-f graphs/asia_osm/asia_osm.mtx -n 32"                   -t 2 4 8 16 -o do_complementary/asia        -a DO -p athena_ht
python3 bench.py -args "-f graphs/hugebubbles-00020/hugebubbles-00020.mtx -n 32" -t 2 4 8 16 -o do_complementary/hugebubbles -a DO -p athena_ht