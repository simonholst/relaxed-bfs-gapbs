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

__thread int thread_id;

pvector<NodeID> InitParent(const Graph &g) {
    pvector<NodeID> parent(g.num_nodes());
    #pragma omp parallel for
    for (NodeID n = 0; n < g.num_nodes(); n++)
        parent[n] = -1;
    return parent;
}

pvector<NodeID> InitDepth(const Graph &g) {
    pvector<int32_t> depth(g.num_nodes());
    #pragma omp parallel for
    for (int64_t n = 0; n < g.num_nodes(); n++)
        depth[n] = 0;
    return depth;
}

pvector<NodeID> ConcurrentBFS(const Graph &g, NodeID source, bool logging_enabled = false)
{
    pvector<NodeID> parent = InitParent(g);
    pvector<int32_t> depth = InitDepth(g);
    boost::lockfree::queue<NodeID> queue(false);
    queue.push(source);
    parent[source] = source;
	while (true)
	{
        NodeID node;
		while (queue.pop(node))
		{
            g.out_neigh(node);
            // uint64_t *neighbors;
			// uint64_t size = get_neighbors(g, current, &neighbors);
			// uint64_t current_distance = g->distances[current];

            int32_t parent_depth = depth[node];
            int32_t new_depth = parent_depth + 1; 

            for (NodeID v : g.out_neigh(node)) {
                int32_t curr_parent = parent[v];
                if (curr_parent < 0) {
                    if (compare_and_swap(parent[v], curr_parent, node)) {
                        queue.push(v);
                    }
                    else {
                        int32_t current_depth = depth[v];
                        if (current_depth > new_depth) {
                            // assign new parent and depth
                        }
                    }
                }
                else {
                    int32_t current_depth = depth[v];
                    if (current_depth > new_depth) {
                        // assign new parent and depth
                    }
                    queue.push(v);
                }
            }

		}
        return parent;
	}
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