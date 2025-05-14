#include <iostream>
#include <vector>
#include <queue>

#include "../benchmark.h"
#include "../bitmap.h"
#include "../builder.h"
#include "../command_line.h"
#include "../graph.h"
#include "../platform_atomics.h"
#include "../pvector.h"
#include "../util.h"
#include "../json.h"
#include "bfs_helper.h"
#include "node.h"
#include "queues/queues.h"
#include <boost/lockfree/queue.hpp>
#include <chrono>
#include <omp.h>

using json = nlohmann::json;

std::vector<uint64_t> source_node_vec;
std::vector<uint64_t> nodes_visited_vec;
std::vector<uint64_t> nodes_revisited_vec;

pvector<NodeID> ConcurrentBFS(const Graph &g, NodeID source_id, bool logging_enabled = false, bool structured_output = false)
{
    #ifdef DEBUG
    uint64_t nodes_revisited_local = 0;
    uint64_t nodes_revisited_total = 0;
    uint64_t nodes_visited_local = 0;
    uint64_t nodes_visited_total = 0;
    if (logging_enabled) {
        PrintAligned("Source", source_id);
    }
    source_node_vec.push_back(source_id);
    #endif

    int thread_id = omp_get_thread_num();

    pvector<Node> parent_array = pvector<Node>(g.num_nodes());
    QUEUE(NodeIdArray);
    parent_array[source_id] = {source_id, 0};

    #ifdef SEQ_START
        SequentialStart(g, parent_array, queue, source_id, thread_id, SEQ_START);
    #endif
    #ifndef SEQ_START
    #define SEQ_START 0
    NodeIdArray source;
    source[0] = source_id;
    source[1] = -1;
    ENQUEUE(source);
    #endif
    
    termination_detection::TerminationDetection termination_detection(omp_get_max_threads());

    #ifdef DEBUG
    #pragma omp parallel private(thread_id, nodes_revisited_local, nodes_visited_local)
    #endif
    #ifndef DEBUG
    #pragma omp parallel private(thread_id)
    #endif
    {
        NodeIdArray consumer_batch;
        NodeIdArray producer_batch;
        NodeIdArray backup_batch;
        
        thread_id = omp_get_thread_num();
        #ifdef DEBUG
        nodes_revisited_local = 0;
        nodes_visited_local = 0;
        #endif

        bool do_backup = false;

        while (termination_detection.repeat([&]() { return DEQUEUE(consumer_batch); })) 
        {

            uint8_t producer_counter = 0;

search_neighbors:
            for (NodeID node_id : consumer_batch) {

                if (node_id == -1) {
                    break;
                }

                #ifdef DEBUG
                nodes_visited_local += 1;
                #endif

                Node node = parent_array[node_id];
                uint32_t new_depth = node.depth + 1;

                for (NodeID neighbor_id : g.out_neigh(node_id)) {
                    Node neighbor = parent_array[neighbor_id];
                    while (new_depth < neighbor.depth) {
                        #ifdef DEBUG
                        if (neighbor.depth != MAX_DEPTH)
                        {
                            nodes_revisited_local += 1;
                        }
                        #endif
                        Node updated_node = {node_id, new_depth};
                        if (compare_and_swap(parent_array[neighbor_id], neighbor, updated_node)) {
                            producer_batch[producer_counter] = neighbor_id;
                            if (producer_counter >= BATCH_SIZE - 1) {
                                ENQUEUE(producer_batch);
                                producer_batch = NodeIdArray();
                                producer_counter = 0;
                            } else {
                                producer_counter++;
                            }
                            break;
                        }
                        neighbor = parent_array[neighbor_id];
                    }
                }
            }

            if (do_backup) { // Do we have a previous backup_batch that hasn't been handled yet?
                consumer_batch = backup_batch;
                do_backup = false;
                goto search_neighbors;
            }

            if (producer_counter <= 0) { continue; } // No leftover elements in producer_batch

            if (SINGLE_DEQUEUE(backup_batch)) {
                auto deq_depth = parent_array[backup_batch[0]].depth;
                auto enq_depth = parent_array[producer_batch[0]].depth;

                int32_t diff = deq_depth - enq_depth;
                auto threshold = 5;
                bool deq_has_ge_depth = diff >= 0;
                bool exceeds_depth_threshold = abs(diff) >= threshold;

                // If the dequeued backup_batch has greater or equal depth to producer_array, we search our producer_array before it
                if (deq_has_ge_depth) {
                    // If the depth difference exceeds the threshold, send the dequeued backup_batch back into the tail of the queue
                    if (exceeds_depth_threshold) {
                        ENQUEUE(backup_batch);
                        do_backup = false;
                    } else {
                        do_backup = true;
                    }
                    producer_batch[producer_counter] = -1;
                    consumer_batch = producer_batch;
                    producer_counter = 0;
                    goto search_neighbors;
                } else {
                    // If the depth difference exceeds the threshold, send the producer_batch to the tail of the queue
                    if (exceeds_depth_threshold) {
                        producer_batch[producer_counter] = -1;
                        producer_counter = 0;
                        ENQUEUE(producer_batch);
                    } 
                    consumer_batch = backup_batch;
                    goto search_neighbors;
                }
            } else {
                producer_batch[producer_counter] = -1;
                consumer_batch = producer_batch;
                producer_counter = 0;
                goto search_neighbors;
            }
        }
        

        #ifdef DEBUG
        #pragma omp atomic
        nodes_revisited_total += nodes_revisited_local;
        #pragma omp atomic
        nodes_visited_total += nodes_visited_local;
        #endif
    }

    pvector<NodeID> result(parent_array.size());
    #pragma omp parallel for
    for (size_t i = 0; i < parent_array.size(); i++) {
        result[i] = parent_array[i].parent;
    }
    #ifdef DEBUG
    if (logging_enabled) {
        PrintAligned("Seq-Start", SEQ_START);
        PrintAligned("Nodes visited", nodes_visited_total);
        PrintAligned("Nodes revisited", nodes_revisited_total);
    }
    nodes_visited_vec.push_back(nodes_visited_total);
    nodes_revisited_vec.push_back(nodes_revisited_total);
    #endif
    return result;
}

int main(int argc, char *argv[]) {
    CLBFSApp cli(argc, argv, "Concurrent BFS Batching Pre-Dequeue Depth Threshold");

    if (!cli.ParseArgs()) {
        printf("Exiting");
        return -1;
    }
    Builder b(cli);
    Graph g = b.MakeGraph();

    // Pick start-vertex in BFS graph traversal
    SourcePicker<Graph> sp(g, cli.start_vertex());

    auto BFSBound = [&sp, &cli](const Graph &g)
    {
        return ConcurrentBFS(g, sp.PickNext(), cli.logging_en());
    };

    SourcePicker<Graph> vsp(g, cli.start_vertex());
    auto VerifierBound = [&vsp](const Graph &g, const pvector<NodeID> &parent)
    {
        return BFSVerifier(g, vsp.PickNext(), parent);
    };

    PrintAligned("Threads", omp_get_max_threads());
    PrintLabel("Queue", QUEUE_TYPE);
    PrintAligned("Batch Size", BATCH_SIZE);
    auto structured_output = BenchmarkKernelWithStructuredOutput(cli, g, BFSBound, PrintBFSStats, VerifierBound);

    if (cli.structured_output()) {
        auto runs = structured_output["run_details"];
        structured_output["queue"] = QUEUE_TYPE;
        structured_output["seq_start"] = SEQ_START;
        for (size_t i = 0; i < source_node_vec.size(); i++) {
            auto run = runs[i];
            run["nodes_visited"] = nodes_visited_vec[i];
            run["nodes_revisited"] = nodes_revisited_vec[i];
            run["source"] = source_node_vec[i];
            runs[i] = run;
        }
        structured_output["run_details"] = runs;
        WriteJsonToFile(cli.output_name(), structured_output);
    }

    return 0;
}