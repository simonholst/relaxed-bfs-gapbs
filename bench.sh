#!/bin/bash

# Example usage: ./bench.sh "-f USA-road-d.USA.gr -n 25 -r 0" true 1 2 4 8 16
bfs_arguments="$1"
shift
thread_numbers="$@"

timestamp=$(date +%Y%m%d_%H%M%S)

make relax_concurrent_bfs_node QUEUE=FAA DEBUG=TRUE
for threads in $thread_numbers
do
    OMP_NUM_THREADS=$threads ./bin/relax_concurrent_bfs_node $bfs_arguments -o FAA_$threads
done

make relax_concurrent_bfs_node QUEUE=MS DEBUG=TRUE
for threads in $thread_numbers
do
    OMP_NUM_THREADS=$threads ./bin/relax_concurrent_bfs_node $bfs_arguments -o MS_$threads
done

make bfs
for threads in $thread_numbers
do
    OMP_NUM_THREADS=$threads ./bin/bfs $bfs_arguments -o DO_$threads
done

base_path="bench_results/debug/$timestamp"

mkdir -p $base_path/FAA
mv FAA_* $base_path/FAA

mkdir -p $base_path/MS
mv MS_* $base_path/MS

mkdir -p $base_path/DO
mv DO_* $base_path/DO

echo "input arguments: $bfs_arguments" > $base_path/arguments.txt

base_path="bench_results/$timestamp"

timestamp=$(date +%Y%m%d_%H%M%S)

make relax_concurrent_bfs_node QUEUE=FAA DEBUG=FALSE
for threads in $thread_numbers
do
    OMP_NUM_THREADS=$threads ./bin/relax_concurrent_bfs_node $bfs_arguments -o FAA_$threads
done

make relax_concurrent_bfs_node QUEUE=MS DEBUG=FALSE
for threads in $thread_numbers
do
    OMP_NUM_THREADS=$threads ./bin/relax_concurrent_bfs_node $bfs_arguments -o MS_$threads
done

make bfs
for threads in $thread_numbers
do
    OMP_NUM_THREADS=$threads ./bin/bfs $bfs_arguments -o DO_$threads
done

base_path="bench_results/$timestamp"

mkdir -p $base_path/FAA
mv FAA_* $base_path/FAA

mkdir -p $base_path/MS
mv MS_* $base_path/MS

mkdir -p $base_path/DO
mv DO_* $base_path/DO

echo "input arguments: $bfs_arguments" > $base_path/arguments.txt