#ifndef SUBQUEUES_H
#define SUBQUEUES_H

#include <vector>
#include <cstdint>
#include <boost/lockfree/queue.hpp>
#include <random>
#include <iostream>
#include <memory>

std::vector<int32_t> generate_samples(int32_t nr_queues, int32_t nr_samples) {
    static thread_local std::mt19937 generator(std::random_device{}());
    std::uniform_int_distribution<int32_t> distribution(0, nr_queues - 1);
    std::vector<int32_t> random_numbers(nr_samples);
    for (int32_t &num : random_numbers) {
        num = distribution(generator);
    }
    return random_numbers;
}

bool double_collect(std::vector<std::unique_ptr<boost::lockfree::queue<int32_t>>> &sub_queues, int32_t &item) {
    auto versions = std::vector<int32_t>(sub_queues.size());
    while (true) {
        for (int i = 0; i < sub_queues.size(); i++) {
            versions[i] = sub_queues[i]->enqueue_version();
            auto dequeued = sub_queues[i]->pop(item);
            if (dequeued) {
                return true;
            }
        }
        bool all_equal = true;
        for (int i = 0; i < sub_queues.size(); i++) {
            if (sub_queues[i]->enqueue_version() != versions[i]) {
                all_equal = false;
                break;
            }
        }
        if (all_equal) {
            return false;
        }
    }
}

void enqueue(std::vector<std::unique_ptr<boost::lockfree::queue<int32_t>>> &sub_queues, int32_t d, int32_t item) {
    auto sample_indices = generate_samples(sub_queues.size(), d);
    auto min_index = sample_indices[0];
    auto min_value = sub_queues[min_index]->enqueue_count();
    for (auto index : sample_indices) {
        auto temp = sub_queues[index]->enqueue_count();
        if (temp < min_value) {
            min_value = temp;
            min_index = index;
        }
    }
    sub_queues[min_index]->push(item);
}

bool dequeue(std::vector<std::unique_ptr<boost::lockfree::queue<int32_t>>> &sub_queues, int32_t d, int32_t &item) {
    auto sample_indices = generate_samples(sub_queues.size(), d);
    auto min_index = sample_indices[0];
    auto min_value = sub_queues[min_index]->dequeue_count();
    for (auto index : sample_indices) {
        auto temp = sub_queues[index]->dequeue_count();
        if (temp < min_value) {
            min_value = temp;
            min_index = index;
        }
    }
    auto success = sub_queues[min_index]->pop(item);
    if (success) {
        return true;
    }
    else {
        return double_collect(sub_queues, item);
    }
}

int main() {
    int n_items = 100;
    auto subqueues = std::vector<std::unique_ptr<boost::lockfree::queue<int32_t>>>();
    for (int i = 0; i < 5; i++) {
        subqueues.emplace_back(std::make_unique<boost::lockfree::queue<int32_t>>(128));
    }

    for (int i = 0; i < n_items; i++) {
        enqueue(subqueues, 3, i);
    }

    for (int i = 0; i < subqueues.size(); i++) {
        std::cout << "Queue " << i << " has " << subqueues[i]->enqueue_count() << " elements" << std::endl;
    }

    for (int i = 0; i < n_items; i++) {
        int32_t item;
        dequeue(subqueues, 3, item);
        std::cout << "Dequeued: " << item << std::endl;
    }

    return 0;
}

#endif // SUBQUEUES_H