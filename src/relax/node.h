#include <cstdint>

#define MAX_DEPTH 0xFFFFFFFF
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
    uint64_t *x_ptr = reinterpret_cast<uint64_t *>(&x);
    uint64_t old_val_int = *reinterpret_cast<uint64_t *>(&old_val);
    uint64_t new_val_int = *reinterpret_cast<uint64_t *>(&new_val);
    return __sync_bool_compare_and_swap(x_ptr, old_val_int, new_val_int);
}