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
// typedef NodeID NodeIdArray[BATCH_SIZE];
using NodeIdArray = NodeID[BATCH_SIZE];

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
    QUEUE(NodeID);
    parent_array[source_id] = {source_id, 0};

    NodeID* source = new NodeIdArray;
    source[0] = source_id;
    source[1] = -1;
    ENQUEUE(source);
    
    termination_detection::TerminationDetection termination_detection(omp_get_max_threads());

    #ifdef DEBUG
    #pragma omp parallel private(thread_id, nodes_revisited_local, nodes_visited_local)
    #endif
    #ifndef DEBUG
    #pragma omp parallel private(thread_id)
    #endif
    {
        NodeID* dequeue_array;
        NodeID* enqueue_array;
        
        thread_id = omp_get_thread_num();
        #ifdef DEBUG
        nodes_revisited_local = 0;
        nodes_visited_local = 0;
        #endif

        while (termination_detection.repeat([&]() {
            return DEQUEUE(dequeue_array);
        })) {
            enqueue_array = new NodeIdArray;
            uint8_t enqueue_counter = 0;

            for (int i = 0; i < BATCH_SIZE; i++) {
                NodeID node_id = dequeue_array[i];
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
                                enqueue_array = new NodeIdArray;
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

            if (enqueue_counter > 0) {
                enqueue_array[enqueue_counter] = -1;
                ENQUEUE(enqueue_array);
            }

            delete dequeue_array;
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