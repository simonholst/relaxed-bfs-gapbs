#ifndef _QUEUES_H_
#define _QUEUES_H_

#ifdef MS
    #include <boost/lockfree/queue.hpp>
    #define ENQUEUE(val) queue.push(val)
    #define DEQUEUE(val) queue.pop(val)
    #define QUEUE(type) boost::lockfree::queue<type> queue(false)
    #define QUEUE_TYPE "Boost Lockfree Queue"
#endif

#ifdef FAA
    #include "faa_array_queue.h"

    template <typename T>
    inline bool dequeue(FAAArrayQueue<T>& queue, T& val, int tid) {
        T* elem = queue.dequeue(tid);
        if (elem == nullptr) {
            return false;
        }
        val = *elem;
        delete elem;
        return true;
    }

    #define ENQUEUE(val) queue.enqueue(new NodeID(val), thread_id)
    #define DEQUEUE(val) dequeue(queue, val, thread_id)
    #define QUEUE(type) FAAArrayQueue<type> queue
    #define QUEUE_TYPE "FAA Array Queue"
#endif

#ifdef FAA_INT
    #include "faa_array_queue_int.h"

    using namespace FAAAQInt;

    inline bool dequeue(FAAArrayQueue<int32_t>& queue, int32_t& val, int tid) {
        val = queue.dequeue(tid);
        return val != -1;
    }

    #define ENQUEUE(val) queue.enqueue(val, thread_id)
    #define DEQUEUE(val) dequeue(queue, val, thread_id)
    #define QUEUE(type) FAAArrayQueue<int32_t> queue
    #define QUEUE_TYPE "FAA Array Queue Int"
#endif

#ifdef DCBO_MS
    #include "subqueues.h"

    #ifndef D
        #define D 2
    #endif
    #ifndef NUM_SUBQUEUES
        #define NUM_SUBQUEUES 64
    #endif

    #define ENQUEUE(val) queue.enqueue(val)
    #define DEQUEUE(val) queue.dequeue(val)
    #define QUEUE(type) DCBOQueue<boost::lockfree::queue<type>, type, D, NUM_SUBQUEUES> queue
    #define QUEUE_TYPE "d-CBO MS"
#endif

#ifdef DCBO_FAA
    #include "subqueues.h"

    #ifndef D
        #define D 2
    #endif
    #ifndef NUM_SUBQUEUES
        #define NUM_SUBQUEUES 64
    #endif

    #define ENQUEUE(val) queue.enqueue(val, thread_id)
    #define DEQUEUE(val) queue.dequeue(val, thread_id)
    #define QUEUE(type) DCBOQueue<FAAArrayQueue<type>, type, D, NUM_SUBQUEUES> queue
    #define QUEUE_TYPE "d-CBO FAA"
#endif

#ifdef DCBO_FAA_INT
    #include "subqueues.h"

    #ifndef D
        #define D 2
    #endif
    #ifndef NUM_SUBQUEUES
        #define NUM_SUBQUEUES 64
    #endif

    #define ENQUEUE(val) queue.enqueue(val, thread_id)
    #define DEQUEUE(val) queue.dequeue(val, thread_id)
    #define QUEUE(type) DCBOQueue<FAAArrayQueueInt, int32_t, D, NUM_SUBQUEUES> queue
    #define QUEUE_TYPE "d-CBO FAA"
#endif

#ifndef QUEUE
    #include <boost/lockfree/queue.hpp>
    #define ENQUEUE(val) queue.push(val)
    #define DEQUEUE(val) queue.pop(val)
    #define QUEUE(type) boost::lockfree::queue<type> queue(false)
    #define QUEUE_TYPE "Unspecified. Defaulting to: Boost Lockfree Queue"
#endif

#endif /* _QUEUES_H_ */