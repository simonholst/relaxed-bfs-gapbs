#ifndef SUBQUEUES_H
#define SUBQUEUES_H

#include <vector>
#include <array>
#include <cstdint>
#include <boost/lockfree/queue.hpp>
#include <random>
#include <iostream>
#include <memory>
#include <queue>
#include <mutex>
#include <omp.h>
#include "boost/lockfree/queue.hpp"
#include "faa_array_queue.h"
#include "faa_array_queue_int.h"
#include "xoshiro.h"

using namespace std;

template <typename QueueType, typename ElementType, int SampleSize, int SubQueueCount>
class DCBOQueueBase {
protected:
    static thread_local XoshiroCpp::Xoshiro256Plus _generator;
    static thread_local uniform_int_distribution<int> _distribution;
};

template <typename QueueType, typename ElementType, int SampleSize, int SubQueueCount>
thread_local XoshiroCpp::Xoshiro256Plus DCBOQueueBase<QueueType, ElementType, SampleSize, SubQueueCount>::_generator{random_device{}()};

template <typename QueueType, typename ElementType, int SampleSize, int SubQueueCount>
thread_local uniform_int_distribution<int> DCBOQueueBase<QueueType, ElementType, SampleSize, SubQueueCount>::_distribution{0, SubQueueCount - 1};


template <typename QueueType, typename ElementType, int SampleSize, int SubQueueCount>
class DCBOQueue;

template <typename ElementType, int SampleSize, int SubQueueCount>
class DCBOQueue<boost::lockfree::queue<ElementType>, ElementType, SampleSize, SubQueueCount> : public DCBOQueueBase<boost::lockfree::queue<ElementType>, ElementType, SampleSize, SubQueueCount> {
private:
    typedef boost::lockfree::queue<ElementType> QueueType;
    typedef array<unique_ptr<QueueType>, SubQueueCount> SubQueues;
    SubQueues _sub_queues;

public:
    DCBOQueue() {
        for (int i = 0; i < SubQueueCount; ++i) {
            _sub_queues[i] = make_unique<boost::lockfree::queue<ElementType>>(false);
        }
    }

    void enqueue(const ElementType value) {
        auto min_index = this->optimal_enqueue_index(_sub_queues);
        _sub_queues[min_index]->push(value);
    }

    bool dequeue(ElementType& item) {
        auto min_index = this->optimal_dequeue_index(_sub_queues);
        if (_sub_queues[min_index]->pop(item)) {
            return true;
        }
        return this->double_collect(item);
    }

    bool double_collect(ElementType& item) {
        auto versions = array<uint16_t, SubQueueCount>();
        while (true) {
            for (int i = 0; i < SubQueueCount; ++i) {
                versions[i] = _sub_queues[i]->enqueue_version();
                if (_sub_queues[i]->pop(item)) {
                    return true;
                }
            }
            bool all_equal = true;
            for (int i = 0; i < SubQueueCount; ++i) {
                if (_sub_queues[i]->enqueue_version() != versions[i]) {
                    all_equal = false;
                    break;
                }
            }
            if (all_equal) {
                return false;
            }
        }
    }

        inline size_t optimal_enqueue_index(array<unique_ptr<QueueType>, SubQueueCount>& sub_queues) {
        size_t min_index = this->_distribution(this->_generator);
        size_t min_enqueue_count = sub_queues[min_index]->enqueue_count();
        for (size_t i = 1; i < SampleSize; ++i) {
            auto random_index = this->_distribution(this->_generator);
            auto enqueue_count = sub_queues[random_index]->enqueue_count();
            if (enqueue_count < min_enqueue_count) {
                min_enqueue_count = enqueue_count;
                min_index = random_index;
            }
        }
        return min_index;
    }

    inline size_t optimal_dequeue_index(array<unique_ptr<QueueType>, SubQueueCount>& sub_queues) {
        size_t min_index = this->_distribution(this->_generator);
        size_t min_dequeue_count = sub_queues[min_index]->dequeue_count();
        for (size_t i = 1; i < SampleSize; ++i) {
            auto random_index = this->_distribution(this->_generator);
            auto dequeue_count = sub_queues[random_index]->dequeue_count();
            if (dequeue_count < min_dequeue_count) {
                min_dequeue_count = dequeue_count;
                min_index = random_index;
            }
        }
        return min_index;
    }
};


template <typename ElementType, int SampleSize, int SubQueueCount>
class DCBOQueue<FAAArrayQueue<ElementType>, ElementType, SampleSize, SubQueueCount> : public DCBOQueueBase<FAAArrayQueue<ElementType>, ElementType, SampleSize, SubQueueCount> {
private:
    typedef FAAArrayQueue<ElementType> QueueType;
    typedef array<unique_ptr<QueueType>, SubQueueCount> SubQueues;
    SubQueues _sub_queues;

    inline bool faaaq_dequeue(size_t min_index, ElementType& item, int thread_id) {
        ElementType* elem = _sub_queues[min_index]->dequeue(thread_id);
        if (elem == nullptr) {
            return false;
        }
        item = *elem;
        delete elem;
        return true;
    }

    inline size_t faaaq_optimal_enqueue_index(array<unique_ptr<QueueType>, SubQueueCount>& sub_queues, const int thread_id) {
        auto min_index = this->_distribution(this->_generator);
        auto min_enqueue_count = sub_queues[min_index]->enqueue_count(thread_id);
        for (size_t i = 1; i < SampleSize; ++i) {
            auto random_index = this->_distribution(this->_generator);
            auto enqueue_count = sub_queues[random_index]->enqueue_count(thread_id);
            if (enqueue_count < min_enqueue_count) {
                min_enqueue_count = enqueue_count;
                min_index = random_index;
            }
        }
        return min_index;
    }

    inline size_t faaaq_optimal_dequeue_index(array<unique_ptr<QueueType>, SubQueueCount>& sub_queues, const int thread_id) {
        auto min_index = this->_distribution(this->_generator);
        auto min_dequeue_count = sub_queues[min_index]->dequeue_count(thread_id);
        for (size_t i = 1; i < SampleSize; ++i) {
            auto random_index = this->_distribution(this->_generator);
            auto dequeue_count = sub_queues[random_index]->dequeue_count(thread_id);
            if (dequeue_count < min_dequeue_count) {
                min_dequeue_count = dequeue_count;
                min_index = random_index;
            }
        }
        return min_index;
    }

public:
    DCBOQueue() {
        for (int i = 0; i < SubQueueCount; ++i) {
            _sub_queues[i] = make_unique<FAAArrayQueue<ElementType>>();
        }
    }

    void enqueue(const ElementType value, int thread_id) {
        auto min_index = this->faaaq_optimal_enqueue_index(_sub_queues, thread_id);
        _sub_queues[min_index]->enqueue(new ElementType(value), thread_id);
    }

    bool dequeue(ElementType& item, int thread_id) {
        auto min_index = this->faaaq_optimal_dequeue_index(_sub_queues, thread_id);
        if (this->faaaq_dequeue(min_index, item, thread_id)) {
            return true;
        }
        return this->double_collect(item, thread_id);
    }

    bool single_dequeue(ElementType& item, int thread_id) {
        auto min_index = this->faaaq_optimal_dequeue_index(_sub_queues, thread_id);
        return this->faaaq_dequeue(min_index, item, thread_id);
    }

    bool double_collect(ElementType& item, int thread_id) {
        auto versions = array<int, SubQueueCount>();
        while (true) {
            for (int i = 0; i < SubQueueCount; ++i) {
                versions[i] = _sub_queues[i]->enqueue_version(thread_id);
                if (this->faaaq_dequeue(i, item, thread_id)) {
                    return true;
                }
            }
            bool all_equal = true;
            for (int i = 0; i < SubQueueCount; ++i) {
                if (_sub_queues[i]->enqueue_version(thread_id) != versions[i]) {
                    all_equal = false;
                    break;
                }
            }
            if (all_equal) {
                return false;
            }
        }
    }
};

typedef FAAAQInt::FAAArrayQueue<int32_t> FAAArrayQueueInt;
template <typename ElementType, int SampleSize, int SubQueueCount>
class DCBOQueue<FAAAQInt::FAAArrayQueue<ElementType>, ElementType, SampleSize, SubQueueCount> : public DCBOQueueBase<FAAAQInt::FAAArrayQueue<ElementType>, ElementType, SampleSize, SubQueueCount> {
private:
    typedef FAAAQInt::FAAArrayQueue<ElementType> QueueType;
    typedef array<unique_ptr<QueueType>, SubQueueCount> SubQueues;
    SubQueues _sub_queues;

    inline size_t faaaq_optimal_enqueue_index(array<unique_ptr<QueueType>, SubQueueCount>& sub_queues, const int thread_id) {
        auto min_index = this->_distribution(this->_generator);
        auto min_enqueue_count = sub_queues[min_index]->enqueue_count(thread_id);
        for (size_t i = 1; i < SampleSize; ++i) {
            auto random_index = this->_distribution(this->_generator);
            auto enqueue_count = sub_queues[random_index]->enqueue_count(thread_id);
            if (enqueue_count < min_enqueue_count) {
                min_enqueue_count = enqueue_count;
                min_index = random_index;
            }
        }
        return min_index;
    }

    inline size_t faaaq_optimal_dequeue_index(array<unique_ptr<QueueType>, SubQueueCount>& sub_queues, const int thread_id) {
        auto min_index = this->_distribution(this->_generator);
        auto min_dequeue_count = sub_queues[min_index]->dequeue_count(thread_id);
        for (size_t i = 1; i < SampleSize; ++i) {
            auto random_index = this->_distribution(this->_generator);
            auto dequeue_count = sub_queues[random_index]->dequeue_count(thread_id);
            if (dequeue_count < min_dequeue_count) {
                min_dequeue_count = dequeue_count;
                min_index = random_index;
            }
        }
        return min_index;
    }

public:
    DCBOQueue() {
        for (int i = 0; i < SubQueueCount; ++i) {
            _sub_queues[i] = make_unique<FAAAQInt::FAAArrayQueue<int32_t>>();
        }
    }

    void enqueue(const int32_t value, int thread_id) {
        auto min_index = this->faaaq_optimal_enqueue_index(_sub_queues, thread_id);
        _sub_queues[min_index]->enqueue(value, thread_id);
    }

    bool dequeue(int32_t& item, int thread_id) {
        auto min_index = this->faaaq_optimal_dequeue_index(_sub_queues, thread_id);
        item = _sub_queues[min_index]->dequeue(thread_id);
        if (item != -1) {
            return true;
        }
        return this->double_collect(item, thread_id);
    }

    bool double_collect(int32_t& item, int thread_id) {
        auto versions = array<int, SubQueueCount>();
        while (true) {
            for (int i = 0; i < SubQueueCount; ++i) {
                versions[i] = _sub_queues[i]->enqueue_version(thread_id);
                item = _sub_queues[i]->dequeue(thread_id);
                if (item != -1) {
                    return true;
                }
            }
            bool all_equal = true;
            for (int i = 0; i < SubQueueCount; ++i) {
                if (_sub_queues[i]->enqueue_version(thread_id) != versions[i]) {
                    all_equal = false;
                    break;
                }
            }
            if (all_equal) {
                return false;
            }
        }
    }
};


template <typename T>
class SequentialQueue {
private:
    queue<T> _queue;
    atomic<uint64_t> _enqueue_count{0};
    atomic<uint64_t> _dequeue_count{0};
    mutable mutex _mutex;

public:
    void enqueue(const T& value) {
        unique_lock<mutex> lock(_mutex);
        _queue.push(value);
        lock.unlock();
        _enqueue_count.fetch_add(1, memory_order_relaxed);
    }

    bool dequeue(T& value) {
        unique_lock<mutex> lock(_mutex);
        if (_queue.empty()) {
            return false;
        }
        value = _queue.front();
        _queue.pop();
        lock.unlock();
        _enqueue_count.fetch_add(1, memory_order_relaxed);
        return true;
    }

    uint64_t enqueue_count() const {
        return _enqueue_count.load(memory_order_relaxed);
    }

    uint64_t dequeue_count() const {
        return _dequeue_count.load(memory_order_relaxed);
    }

    uint64_t enqueue_version() const {
        return _enqueue_count.load(memory_order_relaxed);
    }
};;

template <typename ElementType, int SampleSize, int SubQueueCount>
class DCBOQueue<SequentialQueue<ElementType>, ElementType, SampleSize, SubQueueCount> : public DCBOQueueBase<SequentialQueue<ElementType>, ElementType, SampleSize, SubQueueCount> {
private:
    typedef SequentialQueue<ElementType> QueueType;
    typedef array<unique_ptr<QueueType>, SubQueueCount> SubQueues;
    SubQueues _sub_queues;

public:
    DCBOQueue() {
        for (int i = 0; i < SubQueueCount; ++i) {
            _sub_queues[i] = make_unique<SequentialQueue<ElementType>>();
        }
    }

    void enqueue(const ElementType value) {
        size_t min_index = this->_distribution(this->_generator);
        size_t min_enqueue_count = _sub_queues[min_index]->enqueue_count();
        for (size_t i = 1; i < SampleSize; ++i) {
            auto random_index = this->_distribution(this->_generator);
            auto enqueue_count = _sub_queues[random_index]->enqueue_count();
            if (enqueue_count < min_enqueue_count) {
                min_enqueue_count = enqueue_count;
                min_index = random_index;
            }
        }
        _sub_queues[min_index]->enqueue(value);
    }

    bool dequeue(ElementType& item) {
        size_t min_index = this->_distribution(this->_generator);
        size_t min_dequeue_count = _sub_queues[min_index]->dequeue_count();
        for (size_t i = 1; i < SampleSize; ++i) {
            auto random_index = this->_distribution(this->_generator);
            auto dequeue_count = _sub_queues[random_index]->dequeue_count();
            if (dequeue_count < min_dequeue_count) {
                min_dequeue_count = dequeue_count;
                min_index = random_index;
            }
        }
        if (_sub_queues[min_index]->dequeue(item)) {
            return true;
        }
        return this->double_collect(item);
    }

    bool double_collect(ElementType& item) {
        auto versions = array<uint16_t, SubQueueCount>();
        while (true) {
            for (int i = 0; i < SubQueueCount; ++i) {
                versions[i] = _sub_queues[i]->enqueue_version();
                if (_sub_queues[i]->dequeue(item)) {
                    return true;
                }
            }
            bool all_equal = true;
            for (int i = 0; i < SubQueueCount; ++i) {
                if (_sub_queues[i]->enqueue_version() != versions[i]) {
                    all_equal = false;
                    break;
                }
            }
            if (all_equal) {
                return false;
            }
        }
    }
};

// int main() {


//     DCBOQueue<FAAArrayQueue<int>, int, 2, 8> q1;

//     // #pragma omp parallel
//     // {
//         for (int i = 0; i < 100; i++) {
//             q1.enqueue(i, omp_get_thread_num());
//         }
//     // }

//     // #pragma omp parallel
//     // {
//         int item;
//         for (int i = 0; i < 100; i++) {
//             q1.dequeue(item, omp_get_thread_num());
//             #pragma omp critical
//             cout << "Dequeued: " << item << endl;
//         }
//     // }

//     return 0;
// }

#endif // SUBQUEUES_H