#pragma once
#include <cstdint>

#define MAX_DEPTH 0xFFFFFFFF

#ifndef BATCH_SIZE
    #define BATCH_SIZE 8
#endif
typedef std::array<NodeID, BATCH_SIZE> NodeIdArray;

struct Node
{
    NodeID parent;
    uint32_t depth;

    Node() : parent(-1), depth(MAX_DEPTH) {}

    Node(NodeID parent, uint32_t depth) : parent(parent), depth(depth) {}
};

// Function to perform CAS on a 64-bit struct
bool compare_and_swap(Node &x, Node old_val, Node new_val)
{
    static_assert(sizeof(Node) == sizeof(uint64_t), "Node size must be 8 bytes");

    uint64_t old_val_int, new_val_int;
    std::memcpy(&old_val_int, &old_val, sizeof(uint64_t));
    std::memcpy(&new_val_int, &new_val, sizeof(uint64_t));

    uint64_t *x_ptr = reinterpret_cast<uint64_t *>(&x);
    return __sync_bool_compare_and_swap(x_ptr, old_val_int, new_val_int);
}