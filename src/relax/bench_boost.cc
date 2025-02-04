#include <boost/lockfree/queue.hpp>
#include <chrono>

int main() {
    boost::lockfree::queue<int32_t> queue(false);
    int32_t loops = 0;
    int32_t value;

    auto start = std::chrono::high_resolution_clock::now();

    #pragma omp parallel
    {
        while (loops < 2500) {
            for (int i = 0; i < 100; i++) {
                queue.push(i);
            }
            while(queue.pop(value)) {}
            __sync_fetch_and_add(&loops, 1);
        }
    }


    auto end = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> elapsed = end - start;
    printf("Elapsed time: %f\n", elapsed.count());
}