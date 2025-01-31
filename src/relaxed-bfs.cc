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

__thread int thread_id;

using namespace std;

// BFS verifier does a serial BFS from same source and asserts:
// - parent[source] = source
// - parent[v] = u  =>  depth[v] = depth[u] + 1 (except for source)
// - parent[v] = u  => there is edge from u to v
// - all vertices reachable from source have a parent
bool BFSVerifier(const Graph &g, NodeID source,
                 const pvector<NodeID> &parent)
{
    pvector<int> depth(g.num_nodes(), -1);
    depth[source] = 0;
    vector<NodeID> to_visit;
    to_visit.reserve(g.num_nodes());
    to_visit.push_back(source);
    for (auto it = to_visit.begin(); it != to_visit.end(); it++)
    {
        NodeID u = *it;
        for (NodeID v : g.out_neigh(u))
        {
            if (depth[v] == -1)
            {
                depth[v] = depth[u] + 1;
                to_visit.push_back(v);
            }
        }
    }
    for (NodeID u : g.vertices())
    {
        if ((depth[u] != -1) && (parent[u] != -1))
        {
            if (u == source)
            {
                if (!((parent[u] == u) && (depth[u] == 0)))
                {
                    cout << "Source wrong" << endl;
                    return false;
                }
                continue;
            }
            bool parent_found = false;
            for (NodeID v : g.in_neigh(u))
            {
                if (v == parent[u])
                {
                    if (depth[v] != depth[u] - 1)
                    {
                        cout << "Wrong depths for " << u << " & " << v << endl;
                        return false;
                    }
                    parent_found = true;
                    break;
                }
            }
            if (!parent_found)
            {
                cout << "Couldn't find edge from " << parent[u] << " to " << u << endl;
                return false;
            }
        }
        else if (depth[u] != parent[u])
        {
            cout << "Reachability mismatch" << endl;
            return false;
        }
    }
    return true;
}

void PrintBFSStats(const Graph &g, const pvector<NodeID> &bfs_tree)
{
    int64_t tree_size = 0;
    int64_t n_edges = 0;
    for (NodeID n : g.vertices())
    {
        if (bfs_tree[n] >= 0)
        {
            n_edges += g.out_degree(n);
            tree_size++;
        }
    }
    cout << "BFS Tree has " << tree_size << " nodes and ";
    cout << n_edges << " edges" << endl;
}

void RelaxedBFS(const Graph &g, NodeID source, bool logging_enabled = false) {
    // TODO: Implement

    // list of distances, initialised with [∞,∞,...,∞]
    vector<int64_t> distances(g.num_nodes(), numeric_limits<int64_t>::max());
    
    // TODO: add queue structure here
    distances[source] = 0;

    #pragma omp parallel num_threads(1) 
    {
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

    RelaxedBFS(g, sp.PickNext(), cli.logging_en());
    // auto BFSBound = [&sp, &cli](const Graph &g)
    // {
    //     return RelaxedBFS(g, sp.PickNext(), cli.logging_en());
    // };

    // SourcePicker<Graph> vsp(g, cli.start_vertex());
    // auto VerifierBound = [&vsp] (const Graph &g, const pvector<NodeID> &parent) {
    //     return BFSVerifier(g, vsp.PickNext(), parent);
    // };

    // BenchmarkKernel(cli, g, BFSBound, PrintBFSStats, VerifierBound);
    return 0;

}