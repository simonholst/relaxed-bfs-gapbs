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

#ifndef BATCH_SIZE
    #define BATCH_SIZE 8
#endif

std::vector<uint64_t> source_node_vec;
std::vector<uint64_t> nodes_visited_vec;
std::vector<uint64_t> nodes_revisited_vec;
typedef std::array<NodeID, BATCH_SIZE> NodeIdArray;

template <typename Q>
void SequentialStart(const Graph &g, pvector<Node> &parent_array, Q &queue, NodeID source_id, int thread_id, int nr_iterations) {
    std::queue<NodeID> seq_queue;
    seq_queue.push(source_id);
    int counter = 0;
    NodeID node_id;
    while (!seq_queue.empty() && counter < nr_iterations) {
        node_id = seq_queue.front();
        seq_queue.pop();
        g.out_neigh(node_id);
        for (NodeID neighbor_id : g.out_neigh(node_id)) {
            NodeID curr_parent = parent_array[neighbor_id].parent;
            if (curr_parent < 0) {
                uint32_t curr_depth = parent_array[node_id].depth;
                uint32_t new_depth = curr_depth + 1;
                Node updated_node = {node_id, new_depth};
                parent_array[neighbor_id] = updated_node;
                seq_queue.push(neighbor_id);
            }
        }
        counter++;
    }

    uint8_t enqueue_counter = 0;
    NodeIdArray enqueue_array;
    // transfer to concurrent queue
    while (!seq_queue.empty()) {
        node_id = seq_queue.front();
        seq_queue.pop();
        enqueue_array[enqueue_counter] = node_id;
        if (enqueue_counter >= BATCH_SIZE - 1) {
            ENQUEUE(enqueue_array);
            enqueue_array = NodeIdArray();
            enqueue_counter = 0;
        } else {
            enqueue_counter++;
        }
    }

    if (enqueue_counter > 0) {
        enqueue_array[enqueue_counter] = -1;
        ENQUEUE(enqueue_array);
    }
}

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
        NodeIdArray dequeue_array;
        NodeIdArray enqueue_array;
        NodeIdArray backup_dequeue_array;
        
        thread_id = omp_get_thread_num();
        #ifdef DEBUG
        nodes_revisited_local = 0;
        nodes_visited_local = 0;
        #endif

        bool do_backup = false;

        while (termination_detection.repeat([&]() { return DEQUEUE(dequeue_array); })) 
        {

            uint8_t enqueue_counter = 0;

search_neighbors:
            for (NodeID node_id : dequeue_array) {

                if (node_id == -1) {
                    break;
                }

                #ifdef DEBUG
                nodes_visited_local += 1;
                #endif

                Node node = parent_array[node_id];
                uint32_t depth = node.depth;
                uint32_t new_depth = depth + 1;

                for (NodeID neighbor_id : g.out_neigh(node_id)) {
                    Node neighbor = parent_array[neighbor_id];
                    uint32_t neighbor_depth = neighbor.depth;
                    while (new_depth < neighbor_depth) {
                        #ifdef DEBUG
                        if (neighbor_depth != MAX_DEPTH)
                        {
                            nodes_revisited_local += 1;
                        }
                        #endif
                        Node updated_node = {node_id, new_depth};
                        if (compare_and_swap(parent_array[neighbor_id], neighbor, updated_node)) {
                            enqueue_array[enqueue_counter] = neighbor_id;
                            if (enqueue_counter >= BATCH_SIZE - 1) {
                                ENQUEUE(enqueue_array);
                                enqueue_array = NodeIdArray();
                                enqueue_counter = 0;
                            } else {
                                enqueue_counter++;
                            }
                            break;
                        }
                        neighbor = parent_array[neighbor_id];
                        neighbor_depth = neighbor.depth;
                    }
                }
            }

            if (do_backup) { // Do we have a previous backup_dequeue_array that hasn't been handled yet?
                dequeue_array = backup_dequeue_array;
                do_backup = false;
                goto search_neighbors;
            }

            if (enqueue_counter <= 0) { continue; } // No leftover elements in enqueue_array

            if (SINGLE_DEQUEUE(backup_dequeue_array)) {
                auto deq_depth = parent_array[backup_dequeue_array[0]].depth;
                auto enq_depth = parent_array[enqueue_array[0]].depth;
                // If the dequeued array has greater depth, we search our enqueue array before it
                if (deq_depth >= enq_depth) { 
                    enqueue_array[enqueue_counter] = -1;
                    dequeue_array = enqueue_array;
                    enqueue_counter = 0;
                    do_backup = true;
                    goto search_neighbors;
                } 
                // If the enqueued array has greater depth, we search the dequeued array and continue appending to our enqueue array
                else {
                    dequeue_array = backup_dequeue_array;
                    goto search_neighbors;
                }
            } else {
                enqueue_array[enqueue_counter] = -1;
                dequeue_array = enqueue_array;
                enqueue_counter = 0;
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
    CLBFSApp cli(argc, argv, "Concurrent BFS");

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