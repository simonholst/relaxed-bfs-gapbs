#!/bin/bash

# Example usage: ./bench.sh "-f USA-road-d.USA.gr -n 25 -r 0" 1 2 4 8 16
bfs_arguments="$1"
shift
thread_numbers="$@"

timestamp=$(date +%Y%m%d_%H%M%S)
base_path="bench_results/$timestamp"

queues=("DCBO_FAA" "FAA_BATCHING")

subqueue_sizes=("32" "64" "128")
batch_sizes=("16" "32")

for queue in "${queues[@]}"
do
    for batch_size in "${batch_sizes[@]}"
    do
    for subqueue_size in "${subqueue_sizes[@]}"
    do
       make relax_rbfs_batching QUEUE=$queue BATCH_SIZE=$batch_size SUBQUEUE_SIZE=$subqueue_size
           bfs_name=${queue}_B_${batch_size}_S_${subqueue_size}
       for threads in $thread_numbers
           do
                OMP_NUM_THREADS=$threads ./bin/relax_rbfs_batching $bfs_arguments -o ${bfs_name}_T${threads}
           done

       mkdir -p $base_path/${bfs_name}
       mv ${bfs_name}_* $base_path/${bfs_name}
        done
   done
done


for subqueue_size in "${subqueue_sizes[@]}"
    do
        make relax_rbfs QUEUE=DCBO_FAA SUBQUEUE_SIZE=$subqueue_size
        bfs_name=DCBO_FAA_S_${subqueue_size}
    for threads in $thread_numbers
        do
            OMP_NUM_THREADS=$threads ./bin/relax_rbfs $bfs_arguments -o ${bfs_name}_T${threads}
        done
        mkdir -p $base_path/${bfs_name}
        mv ${bfs_name}_* $base_path/${bfs_name}
    done

make bfs
for threads in $thread_numbers
do
    OMP_NUM_THREADS=$threads ./bin/bfs $bfs_arguments -o DO_${threads}
done

mkdir -p $base_path/DO
mv DO_* $base_path/DO

echo "input arguments: $bfs_arguments" > $base_path/arguments.txt
