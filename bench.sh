#!/bin/bash

# Example usage: ./bench.sh "-f USA-road-d.USA.gr -n 25 -r 0" 1 2 4 8 16
bfs_arguments="$1"
shift
thread_numbers="$@"

timestamp=$(date +%Y%m%d_%H%M%S)
base_path="bench_results/$timestamp"

queues=("DCBO_FAA" "DCBO_MS" "FAA" "MS")

for queue in "${queues[@]}"
do
    make relax_concurrent_bfs_node QUEUE=$queue DEBUG=TRUE
    for threads in $thread_numbers
    do
        OMP_NUM_THREADS=$threads ./bin/relax_concurrent_bfs_node $bfs_arguments -o ${queue}_debug_${threads}
    done

    mkdir -p $base_path/${queue}_debug
    mv ${queue}_debug_* $base_path/${queue}_debug
done

for queue in "${queues[@]}"
do
    make relax_concurrent_bfs_node QUEUE=$queue DEBUG=FALSE
    for threads in $thread_numbers
    do
        OMP_NUM_THREADS=$threads ./bin/relax_concurrent_bfs_node $bfs_arguments -o ${queue}_${threads}
    done

    mkdir -p $base_path/$queue
    mv ${queue}_* $base_path/$queue
done


make bfs
for threads in $thread_numbers
do
    OMP_NUM_THREADS=$threads ./bin/bfs $bfs_arguments -o DO_${threads}
done

mkdir -p $base_path/DO
mv DO_* $base_path/DO

echo "input arguments: $bfs_arguments" > $base_path/arguments.txt
