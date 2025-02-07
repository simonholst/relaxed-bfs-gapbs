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
#include "queues.h"
#include <boost/lockfree/queue.hpp>
#include <chrono>
#include <omp.h>

using json = nlohmann::json;

#define MAX_FAILURES         1000

volatile uint64_t active_threads;
std::vector<uint64_t> source_node_vec;
std::vector<uint64_t> cas_fails_vec;
std::vector<uint64_t> edges_looked_at_vec;
std::vector<uint64_t> wrong_depth_count_vec;
std::vector<uint64_t> queue_pops_vec;

pvector<NodeID> ConcurrentBFS(const Graph &g, NodeID source_id, bool logging_enabled = false, bool structured_output = false)
{
    #ifdef DEBUG
    uint64_t cas_fails = 0;
    uint64_t edges_looked_at = 0;
    uint64_t wrong_depth_count = 0;
    uint64_t queue_pops = 0;
    PrintAligned("Source", source_id);
    source_node_vec.push_back(source_id);
    #endif

    bool is_active = false;
    uint64_t failures = 0;
    int thread_id = omp_get_thread_num();

    pvector<Node> node_to_parent_and_depth = pvector<Node>(g.num_nodes());
    QUEUE(NodeID);
    node_to_parent_and_depth[source_id] = {source_id, 0};
    ENQUEUE(source_id);
    NodeID node_id;
    active_threads = 0;

    #pragma omp parallel private(is_active, node_id, thread_id)
    {
        thread_id = omp_get_thread_num();
        while (failures < MAX_FAILURES || active_threads != 0) {
            while(DEQUEUE(node_id)) {

                if (!is_active) {
                    fetch_and_add(active_threads, 1);
                    is_active = true;
                    failures = 0;
                }
                #ifdef DEBUG
                fetch_and_add(queue_pops, 1);
                #endif
                Node node = node_to_parent_and_depth[node_id];
                uint32_t depth = node.depth;
                uint32_t new_depth = depth + 1;

                for (NodeID neighbor_id : g.out_neigh(node_id)) {
                    #ifdef DEBUG
                    fetch_and_add(edges_looked_at, 1);
                    #endif
                    Node neighbor = node_to_parent_and_depth[neighbor_id];
                    uint32_t neighbor_depth = neighbor.depth;
                    while (new_depth < neighbor_depth) {
                        #ifdef DEBUG
                        if (neighbor_depth != MAX_DEPTH) {
                            fetch_and_add(wrong_depth_count, 1);
                        }
                        #endif
                        // uint64_t updated_node =  new_depth | node_id;
                        Node updated_node = {node_id, new_depth};
                        if (compare_and_swap(node_to_parent_and_depth[neighbor_id], neighbor, updated_node)) {
                            ENQUEUE(neighbor_id);
                            break;
                        }
                        #ifdef DEBUG
                        fetch_and_add(cas_fails, 1);
                        #endif
                        neighbor = node_to_parent_and_depth[neighbor_id];
                        neighbor_depth = neighbor.depth;
                    }
                }
            }
            if (is_active)
            {
                fetch_and_sub(active_threads, 1);
                is_active = false;
            }
            failures += 1;
        }
    }

    pvector<NodeID> result(node_to_parent_and_depth.size());
    #pragma omp parallel for
    for (size_t i = 0; i < node_to_parent_and_depth.size(); i++) {
        result[i] = node_to_parent_and_depth[i].parent;
    }
    #ifdef DEBUG
    printf("-----\n");
    PrintAligned("CAS fails", cas_fails);
    PrintAligned("Edges looked at", edges_looked_at);
    PrintAligned("Wrong depth count", wrong_depth_count);
    PrintAligned("Queue pops", queue_pops);
    printf("-----\n");
    cas_fails_vec.push_back(cas_fails);
    edges_looked_at_vec.push_back(edges_looked_at);
    wrong_depth_count_vec.push_back(wrong_depth_count);
    queue_pops_vec.push_back(queue_pops);
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
    auto structured_output = BenchmarkKernelWithStructuredOutput(cli, g, BFSBound, PrintBFSStats, VerifierBound);

    if (cli.structured_output()) {
        auto runs = structured_output["run_details"];
        structured_output["queue"] = QUEUE_TYPE;
        for (size_t i = 0; i < source_node_vec.size(); i++) {
            auto run = runs[i];
            run["cas_fails"] = cas_fails_vec[i];
            run["edges_looked_at"] = edges_looked_at_vec[i];
            run["wrong_depth_count"] = wrong_depth_count_vec[i];
            run["queue_pops"] = queue_pops_vec[i];
            run["source"] = source_node_vec[i];
            runs[i] = run;
        }
        structured_output["run_details"] = runs;
        WriteJsonToFile(cli.output_name(), structured_output);
    }

    return 0;
}