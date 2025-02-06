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
#include "bfs_helper.h"
#include "node.h"
#include <boost/lockfree/queue.hpp>
#include <chrono>
#include <omp.h>

#define MAX_DEPTH            0xFFFFFFFF
#define MAX_FAILURES         1000

volatile uint64_t active_threads;

pvector<Node> InitNodeParentDepth(const Graph &g) {
    pvector<Node> parent(g.num_nodes());
    #pragma omp parallel for
    for (int64_t n = 0; n < g.num_nodes(); n++)
        parent[n].depth = 0xFFFFFFFF;
    return parent;
} 

pvector<NodeID> ConcurrentBFS(const Graph &g, NodeID source_id, bool logging_enabled = false)
{
    #ifdef DEBUG
    uint64_t cas_fails = 0;
    uint64_t edges_looked_at = 0;
    uint64_t wrong_depth_count = 0;
    uint64_t queue_pops = 0;
    printf("Source: %u\n", source_id);
    #endif

    bool is_active = false;
    uint64_t failures = 0;

    pvector<Node> node_to_parent_and_depth = InitNodeParentDepth(g);
    boost::lockfree::queue<NodeID> queue(false);
    node_to_parent_and_depth[source_id] = {source_id, 0};
    queue.push(source_id);
    NodeID node_id;
    active_threads = 0;

    #pragma omp parallel private(is_active, node_id)
    {
        while (failures < MAX_FAILURES || active_threads != 0) {
            while(queue.pop(node_id)) {

                if (!is_active) {
                    __sync_fetch_and_add(&active_threads, 1);
                    is_active = true;
                    failures = 0;
                }
                #ifdef DEBUG
                __sync_fetch_and_add(&queue_pops, 1);
                #endif
                Node node = node_to_parent_and_depth[node_id];
                uint32_t depth = node.depth;
                uint32_t new_depth = depth + 1;

                for (NodeID neighbor_id : g.out_neigh(node_id)) {
                    #ifdef DEBUG
                    __sync_fetch_and_add(&edges_looked_at, 1);
                    #endif
                    Node neighbor = node_to_parent_and_depth[neighbor_id];
                    uint32_t neighbor_depth = neighbor.depth;
                    while (new_depth < neighbor_depth) {
                        #ifdef DEBUG
                        if (neighbor_depth != MAX_DEPTH) {
                            __sync_fetch_and_add(&wrong_depth_count, 1);
                        }
                        #endif
                        // uint64_t updated_node =  new_depth | node_id;
                        Node updated_node = {node_id, new_depth};
                        if (compare_and_swap(node_to_parent_and_depth[neighbor_id], neighbor, updated_node)) {
                            queue.push(neighbor_id);
                            break;
                        }
                        #ifdef DEBUG
                        __sync_fetch_and_add(&cas_fails, 1);
                        #endif
                        neighbor = node_to_parent_and_depth[neighbor_id];
                        neighbor_depth = neighbor.depth;
                    }
                }
            }
            if (is_active)
            {
                __sync_fetch_and_sub(&active_threads, 1);
                is_active = false;
            }
            failures += 1;
        }
    }

    pvector<NodeID> result(node_to_parent_and_depth.size());
    #pragma omp parallel for
    for (size_t i = 0; i < node_to_parent_and_depth.size(); i++) {
        if (node_to_parent_and_depth[i].depth == MAX_DEPTH) {
            result[i] = -1;
        } else {
            result[i] = static_cast<NodeID>(node_to_parent_and_depth[i].parent);
        }
    }
    #ifdef DEBUG
    printf("-----\n");
    PrintAligned("CAS fails", cas_fails);
    PrintAligned("Edges looked at", edges_looked_at);
    PrintAligned("Wrong depth count", wrong_depth_count);
    PrintAligned("Queue pops", queue_pops);
    printf("-----\n");
    #endif
    return result;
}

int main(int argc, char *argv[]) {
    CLApp cli(argc, argv, "concurrent breadth-first search");

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

    BenchmarkKernel(cli, g, BFSBound, PrintBFSStats, VerifierBound);
    return 0;
}