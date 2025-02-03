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
    bool is_active = false; // TODO: add as private in omp pragma
    uint64_t failures = 0;
    uint64_t cas_fails = 0;

    // Maps a NodeID n to a single uint64_t that contains both n's parent and the depth of n
    // - The parent's NodeID is the 32 least significant bits
    // - The depth is the 32 most significant bits
    pvector<uint64_t> node_to_parent_and_depth = InitNodeParentDepth(g);
    boost::lockfree::queue<uint64_t> queue(false);
    uint64_t source = static_cast<uint64_t>(source_id);
    node_to_parent_and_depth[source] = source;
    printf("Source: %llu\n", source);
    queue.push(source);
    uint64_t node;
    active_threads = 0;

    #pragma omp parallel private(is_active, node)
    {
        while (failures < MAX_FAILURES || active_threads != 0) {
            while(queue.pop(node)) {
                if (!is_active) {
                    __sync_fetch_and_add(&active_threads, 1);
                    is_active = true;
                    failures = 0;
                }
                uint64_t parent_id = getParentId(node);
                uint64_t depth = getDepth(node);
                uint64_t new_depth = incDepth(depth);

                for (NodeID neighbor_id : g.out_neigh(node)) {
                    uint64_t neighbor = node_to_parent_and_depth[neighbor_id];
                    uint64_t neighbor_depth = getDepth(neighbor);
                    while (new_depth < neighbor_depth) {
                        uint64_t updated_node =  new_depth | parent_id;
                        if (compare_and_swap(node_to_parent_and_depth[neighbor_id], neighbor, updated_node)) {
                            queue.push(new_depth | neighbor_id); 
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