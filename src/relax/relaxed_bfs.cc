#include <iostream>
#include <vector>

#include "benchmark.h"
#include "bitmap.h"
#include "builder.h"
#include "command_line.h"
#include "graph.h"
#include "platform_atomics.h"
#include "pvector.h"
#include "sliding_queue.h"
#include "timer.h"
#include <boost/lockfree/queue.hpp>
#include "bfs_helper.h"

__thread int thread_id;

pvector<NodeID> InitParent(const Graph &g) {
  pvector<NodeID> parent(g.num_nodes());
  #pragma omp parallel for
  for (NodeID n=0; n < g.num_nodes(); n++)
    parent[n] = -1;
  return parent;
}

pvector<NodeID> RelaxedBFS(const Graph &g, NodeID source, bool logging_enabled = false) {
    pvector<NodeID> parent = InitParent(g);
    boost::lockfree::queue<NodeID> queue(false);
    queue.push(source);
    parent[source] = source;
	while (true)
	{
        NodeID node;
		while (queue.pop(node))
		{
            g.out_neigh(node);
            for (NodeID v : g.out_neigh(node))
            {
                NodeID curr_val = parent[v];
                if (curr_val < 0)
                {
                    if (compare_and_swap(parent[v], curr_val, node))
                    {
                        queue.push(v);
                    }
                }
            }

		}
        return parent;
	}
}

int main(int argc, char* argv[])
{
    CLApp cli(argc, argv, "relaxed breadth-first search");

    if (!cli.ParseArgs()) {
        printf("Exiting");
        return -1;
    }
    Builder b(cli);
    Graph g = b.MakeGraph();

    // Pick start-vertex in BFS graph traversal
    SourcePicker<Graph> sp(g, cli.start_vertex());

    // RelaxedBFS(g, sp.PickNext(), cli.logging_en());
    auto BFSBound = [&sp, &cli](const Graph &g)
    {
        return RelaxedBFS(g, sp.PickNext(), cli.logging_en());
    };

    SourcePicker<Graph> vsp(g, cli.start_vertex());
    auto VerifierBound = [&vsp] (const Graph &g, const pvector<NodeID> &parent) {
        return BFSVerifier(g, vsp.PickNext(), parent);
    };

    BenchmarkKernel(cli, g, BFSBound, PrintBFSStats, VerifierBound);
    return 0;
}