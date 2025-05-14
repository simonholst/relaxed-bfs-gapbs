#ifndef BFS_HELPER_H
#define BFS_HELPER_H
#include <vector>
#include <iostream>
#include <atomic>

#include "queues/queues.h"
#include "node.h"
#include "../benchmark.h"
#include "../graph.h"
#include "../pvector.h"

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

#pragma once

#ifdef __SSE2__
#include <emmintrin.h>
#define PAUSE _mm_pause()
#else
#define PAUSE void(0)
#endif

namespace termination_detection {

// Taken from https://github.com/marvinwilliams/multiqueue_experiments

class TerminationDetection {
    int num_threads_;
    std::atomic_int idle_count_{0};
    std::atomic_int no_work_count_{0};

    bool should_terminate() {
        idle_count_.fetch_add(1, std::memory_order_relaxed);
        while (no_work_count_.load(std::memory_order_relaxed) >= num_threads_) {
            if (idle_count_.load(std::memory_order_relaxed) >= num_threads_) {
                return true;
            }
            PAUSE;
        }
        idle_count_.fetch_sub(1, std::memory_order_relaxed);
        return false;
    }

   public:
    explicit TerminationDetection(int num_threads) : num_threads_{num_threads} {
    }

    template <typename F>
    bool repeat(F&& f) {
        if (f()) {
            return true;
        }
        no_work_count_.fetch_add(1, std::memory_order_relaxed);
        while (!f()) {
            if (no_work_count_.load(std::memory_order_relaxed) >= num_threads_) {
                if (should_terminate()) {
                    return false;
                }
            }
        }
        no_work_count_.fetch_sub(1, std::memory_order_relaxed);
        return true;
    }
};

}  // namespace termination_detection

template <typename Q>
void SequentialStart(const Graph &g, pvector<Node> &parent_array, Q &queue, NodeID source_id, int thread_id, int nr_iterations)
{
    std::queue<NodeID> seq_queue;
    seq_queue.push(source_id);
    int counter = 0;
    NodeID node_id;
    while (!seq_queue.empty() && counter < nr_iterations)
    {
        node_id = seq_queue.front();
        seq_queue.pop();
        g.out_neigh(node_id);
        for (NodeID neighbor_id : g.out_neigh(node_id))
        {
            NodeID curr_parent = parent_array[neighbor_id].parent;
            if (curr_parent < 0)
            {
                uint32_t curr_depth = parent_array[node_id].depth;
                uint32_t new_depth = curr_depth + 1;
                Node updated_node = {node_id, new_depth};
                parent_array[neighbor_id] = updated_node;
                seq_queue.push(neighbor_id);
            }
        }
        counter++;
    }

    uint8_t producer_counter = 0;
    NodeIdArray producer_batch;
    // transfer to concurrent queue
    while (!seq_queue.empty())
    {
        node_id = seq_queue.front();
        seq_queue.pop();
        producer_batch[producer_counter] = node_id;
        if (producer_counter >= BATCH_SIZE - 1)
        {
            ENQUEUE(producer_batch);
            producer_batch = NodeIdArray();
            producer_counter = 0;
        }
        else
        {
            producer_counter++;
        }
    }

    if (producer_counter > 0)
    {
        producer_batch[producer_counter] = -1;
        ENQUEUE(producer_batch);
    }
}

#endif