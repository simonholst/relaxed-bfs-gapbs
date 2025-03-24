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

std::vector<uint64_t> nodes_visited_vec;
std::vector<uint64_t> source_node_vec;

pvector<NodeID> InitParent(const Graph &g) {
  pvector<NodeID> parent(g.num_nodes());
  for (NodeID n=0; n < g.num_nodes(); n++)
    parent[n] = -1;
  return parent;
}

#ifdef DEBUG    
uint64_t max_degree_node = 0;
#endif

pvector<NodeID> SequentialBFS(const Graph &g, NodeID source, bool logging_enabled = false) {
    #ifdef DEBUG
    uint64_t nodes_visited = 0;
    source_node_vec.push_back(source);
    #endif

    pvector<NodeID> parent = InitParent(g);
    queue<NodeID> queue;
    queue.push(source);
    parent[source] = source;
    while (!queue.empty())
    {
        #ifdef DEBUG
        nodes_visited++;
        uint64_t degree = 0;
        #endif
        NodeID node = queue.front();
        queue.pop();
        for (NodeID v : g.out_neigh(node))
        {
            #ifdef DEBUG
            degree++;
            #endif
            NodeID curr_val = parent[v];
            if (curr_val < 0)
            {
                parent[v] = node;
                queue.push(v);
            }
        }
        #ifdef DEBUG
        if (degree > max_degree_node)
        {
            max_degree_node = degree;
        }
        #endif

    }
    #ifdef DEBUG
    nodes_visited_vec.push_back(nodes_visited);
    #endif
    return parent;
}

int main(int argc, char* argv[])
{
    CLBFSApp cli(argc, argv, "Sequential BFS");

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
        return SequentialBFS(g, sp.PickNext(), cli.logging_en());
    };

    SourcePicker<Graph> vsp(g, cli.start_vertex());
    auto VerifierBound = [&vsp] (const Graph &g, const pvector<NodeID> &parent) {
        return BFSVerifier(g, vsp.PickNext(), parent);
    };

    auto structured_output = BenchmarkKernelWithStructuredOutput(cli, g, BFSBound, PrintBFSStats, VerifierBound);

    if (cli.structured_output()) {
        auto runs = structured_output["run_details"];
        #ifdef DEBUG
        structured_output["max_degree"] = max_degree_node;
        #endif
        structured_output["queue"] = "std::queue";
        for (size_t i = 0; i < source_node_vec.size(); i++) {
            auto run = runs[i];
            run["nodes_visited"] = nodes_visited_vec[i];
            run["nodes_revisited"] = 0;
            run["source"] = source_node_vec[i];
            runs[i] = run;
        }
        structured_output["run_details"] = runs;
        WriteJsonToFile(cli.output_name(), structured_output);
    }
    return 0;
}