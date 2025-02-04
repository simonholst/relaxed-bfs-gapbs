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
#include "bfs_helper.h"
#include <boost/lockfree/queue.hpp>
#include <chrono>
#include <omp.h>

#define MAX_DEPTH            0xFFFFFFFF00000000
#define MOST_SIGNIFICANT_32  0xFFFFFFFF00000000
#define LEAST_SIGNIFICANT_32 0x00000000FFFFFFFF
#define INC_DEPTH            0x0000000100000000
#define MAX_FAILURES         1000


volatile uint64_t active_threads;

pvector<uint64_t> InitNodeParentDepth(const Graph &g) {
    pvector<uint64_t> parent(g.num_nodes());
    #pragma omp parallel for
    for (int64_t n = 0; n < g.num_nodes(); n++)
        parent[n] = 0xFFFFFFFF00000000 + n;
    return parent;
}

// parent id is the 32 least significant bits
inline uint64_t getParentId(uint64_t node) {
    return node & LEAST_SIGNIFICANT_32;
}

inline uint64_t getDepth(NodeID node) {
    return static_cast<uint64_t>(node) << 32;
}

// depth is the 32 most significant bits 
inline uint64_t getDepth(uint64_t node){
    return node & MOST_SIGNIFICANT_32;
}

inline uint64_t incDepth(uint64_t node){
    return node + INC_DEPTH;
} 

pvector<NodeID> ConcurrentBFS(const Graph &g, NodeID source_id, bool logging_enabled = false)
{
    bool is_active = false;
    uint64_t failures = 0;
    uint64_t cas_fails = 0;
    uint64_t edges_looked_at = 0;
    uint64_t redundant_re_depth = 0;
    uint64_t queue_pops = 0;
    double total_time = 0;
    double total_time_pop = 0;

    // Maps a NodeID n to a single uint64_t that contains both n's parent and the depth of n
    // - The parent's NodeID is the 32 least significant bits
    // - The depth is the 32 most significant bits
    pvector<uint64_t> node_to_parent_and_depth = InitNodeParentDepth(g);
    boost::lockfree::queue<NodeID> queue(false);
    uint64_t source = static_cast<uint64_t>(source_id);
    node_to_parent_and_depth[source_id] = source;
    printf("Source: %llu\n", source);
    queue.push(source);
    NodeID node_id;
    active_threads = 0;
    std::chrono::duration<double> total_elapsed(0);
    std::chrono::duration<double> total_elapsed_pop(0);

    #pragma omp parallel private(is_active, node_id, total_elapsed, total_elapsed_pop)
    {
        while (failures < MAX_FAILURES || active_threads != 0) {
            while(true) {
                auto start = std::chrono::high_resolution_clock::now();
                bool suc = queue.pop(node_id);
                auto end = std::chrono::high_resolution_clock::now();
                total_elapsed_pop += end - start;
                if (!suc) {
                    break;
                }
                if (!is_active) {
                    __sync_fetch_and_add(&active_threads, 1);
                    is_active = true;
                    failures = 0;
                }
                __sync_fetch_and_add(&queue_pops, 1);
                uint64_t node = node_to_parent_and_depth[node_id];
                uint64_t depth = getDepth(node);
                uint64_t new_depth = incDepth(depth);

                for (NodeID neighbor_id : g.out_neigh(node_id)) {
                    __sync_fetch_and_add(&edges_looked_at, 1);
                    uint64_t neighbor = node_to_parent_and_depth[neighbor_id];
                    uint64_t neighbor_depth = getDepth(neighbor);
                    while (new_depth < neighbor_depth) {
                        if (neighbor_depth != MAX_DEPTH) {
                            __sync_fetch_and_add(&redundant_re_depth, 1);
                        }
                        uint64_t updated_node =  new_depth | node_id;
                        if (compare_and_swap(node_to_parent_and_depth[neighbor_id], neighbor, updated_node)) {
                            auto start = std::chrono::high_resolution_clock::now();
                            queue.push(neighbor_id); 
                            auto end = std::chrono::high_resolution_clock::now();
                            total_elapsed += end - start;
                            break;
                        }
                        __sync_fetch_and_add(&cas_fails, 1);
                        neighbor = node_to_parent_and_depth[neighbor_id];
                        neighbor_depth = getDepth(neighbor);
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
        #pragma omp atomic
        total_time += total_elapsed.count();

        #pragma omp atomic
        total_time_pop += total_elapsed_pop.count();
    }

    pvector<NodeID> result(node_to_parent_and_depth.size());
    #pragma omp parallel for
    for (size_t i = 0; i < node_to_parent_and_depth.size(); i++) {
        if (getDepth(node_to_parent_and_depth[i]) == MAX_DEPTH) {
            result[i] = -1;
        } else {
            result[i] = static_cast<NodeID>(node_to_parent_and_depth[i]);
        }
    }
    printf("CAS fails: %llu\n", cas_fails);
    printf("Failures: %llu\n", failures);
    printf("Edges looked at: %llu\n", edges_looked_at);
    printf("Redundant re-depths: %llu\n", redundant_re_depth);
    printf("Queue pops: %llu\n", queue_pops);
    printf("Total time: %f\n", total_time / omp_get_num_threads());
    printf("Total time pop: %f\n", total_time_pop / omp_get_num_threads());
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