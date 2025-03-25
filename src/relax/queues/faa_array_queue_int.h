/******************************************************************************
 * Copyright (c) 2014-2016, Pedro Ramalhete, Andreia Correia
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in the
 *       documentation and/or other materials provided with the distribution.
 *     * Neither the name of Concurrency Freaks nor the
 *       names of its contributors may be used to endorse or promote products
 *       derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL <COPYRIGHT HOLDER> BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 ******************************************************************************
 */

#ifndef _FAA_ARRAY_QUEUE_INT_HP_H_
#define _FAA_ARRAY_QUEUE_INT_HP_H_

#include <atomic>
#include <stdexcept>
#include <vector>
#include <iostream>


namespace FAAAQInt {

template<typename T>
class HazardPointers {

private:
    static const int      HP_MAX_THREADS = 256;
    static const int      HP_MAX_HPS = 4;     // This is named 'K' in the HP paper
    static const int      CLPAD = 128/sizeof(std::atomic<T*>);
    static const int      HP_THRESHOLD_R = 0; // This is named 'R' in the HP paper
    static const int      MAX_RETIRED = HP_MAX_THREADS*HP_MAX_HPS; // Maximum number of retired objects per thread

    const int             maxHPs;
    const int             maxThreads;

    std::atomic<T*>*      hp[HP_MAX_THREADS];
    // It's not nice that we have a lot of empty vectors, but we need padding to avoid false sharing
    std::vector<T*>       retiredList[HP_MAX_THREADS*CLPAD];

public:
    HazardPointers(int maxHPs=HP_MAX_HPS, int maxThreads=HP_MAX_THREADS) : maxHPs{maxHPs}, maxThreads{maxThreads} {
        for (int ithread = 0; ithread < HP_MAX_THREADS; ithread++) {
            hp[ithread] = new std::atomic<T*>[CLPAD*2]; // We allocate four cache lines to allow for many hps and without false sharing
            for (int ihp = 0; ihp < HP_MAX_HPS; ihp++) {
                hp[ithread][ihp].store(nullptr, std::memory_order_relaxed);
            }
        }
    }

    ~HazardPointers() {
        for (int ithread = 0; ithread < HP_MAX_THREADS; ithread++) {
            delete[] hp[ithread];
            // Clear the current retired nodes
            for (unsigned iret = 0; iret < retiredList[ithread*CLPAD].size(); iret++) {
                delete retiredList[ithread*CLPAD][iret];
            }
        }
    }


    /**
     * Progress Condition: wait-free bounded (by maxHPs)
     */
    void clear(const int tid) {
        for (int ihp = 0; ihp < maxHPs; ihp++) {
            hp[tid][ihp].store(nullptr, std::memory_order_release);
        }
    }


    /**
     * Progress Condition: wait-free population oblivious
     */
    void clearOne(int ihp, const int tid) {
        hp[tid][ihp].store(nullptr, std::memory_order_release);
    }


    /**
     * Progress Condition: lock-free
     */
    T* protect(int index, const std::atomic<T*>& atom, const int tid) {
        T* n = nullptr;
        T* ret;
		while ((ret = atom.load()) != n) {
			hp[tid][index].store(ret);
			n = ret;
		}
		return ret;
    }

	
    /**
     * This returns the same value that is passed as ptr, which is sometimes useful
     * Progress Condition: wait-free population oblivious
     */
    T* protectPtr(int index, T* ptr, const int tid) {
        hp[tid][index].store(ptr);
        return ptr;
    }



    /**
     * This returns the same value that is passed as ptr, which is sometimes useful
     * Progress Condition: wait-free population oblivious
     */
    T* protectRelease(int index, T* ptr, const int tid) {
        hp[tid][index].store(ptr, std::memory_order_release);
        return ptr;
    }


    /**
     * Progress Condition: wait-free bounded (by the number of threads squared)
     */
    void retire(T* ptr, const int tid) {
        retiredList[tid*CLPAD].push_back(ptr);
        if (retiredList[tid*CLPAD].size() < HP_THRESHOLD_R) return;
        for (unsigned iret = 0; iret < retiredList[tid*CLPAD].size();) {
            auto obj = retiredList[tid*CLPAD][iret];
            bool canDelete = true;
            for (int tid = 0; tid < maxThreads && canDelete; tid++) {
                for (int ihp = maxHPs-1; ihp >= 0; ihp--) {
                    if (hp[tid][ihp].load() == obj) {
                        canDelete = false;
                        break;
                    }
                }
            }
            if (canDelete) {
                retiredList[tid*CLPAD].erase(retiredList[tid*CLPAD].begin() + iret);
                delete obj;
                continue;
            }
            iret++;
        }
    }
};


/**
 * <h1> Fetch-And-Add Array Queue </h1>
 *
 * Each node has one array but we don't search for a vacant entry. Instead, we
 * use FAA to obtain an index in the array, for enqueueing or dequeuing.
 *
 * There are some similarities between this queue and the basic queue in YMC:
 * http://chaoran.me/assets/pdf/wfq-ppopp16.pdf
 * but it's not the same because the queue in listing 1 is obstruction-free, while
 * our algorithm is lock-free.
 * In FAAArrayQueue eventually a new node will be inserted (using Michael-Scott's
 * algorithm) and it will have an item pre-filled in the first position, which means
 * that at most, after BUFFER_SIZE steps, one item will be enqueued (and it can then
 * be dequeued). This kind of progress is lock-free.
 *
 * Each entry in the array may contain one of three possible values:
 * - A valid item that has been enqueued;
 * - nullptr, which means no item has yet been enqueued in that position;
 * - taken, a special value that means there was an item but it has been dequeued;
 *
 * Enqueue algorithm: FAA + CAS(null,item)
 * Dequeue algorithm: FAA + CAS(item,taken)
 * Consistency: Linearizable
 * enqueue() progress: lock-free
 * dequeue() progress: lock-free
 * Memory Reclamation: Hazard Pointers (lock-free)
 * Uncontended enqueue: 1 FAA + 1 CAS + 1 HP
 * Uncontended dequeue: 1 FAA + 1 CAS + 1 HP
 *
 *
 * <p>
 * Lock-Free Linked List as described in Maged Michael and Michael Scott's paper:
 * {@link http://www.cs.rochester.edu/~scott/papers/1996_PODC_queues.pdf}
 * <a href="http://www.cs.rochester.edu/~scott/papers/1996_PODC_queues.pdf">
 * Simple, Fast, and Practical Non-Blocking and Blocking Concurrent Queue Algorithms</a>
 * <p>
 * The paper on Hazard Pointers is named "Hazard Pointers: Safe Memory
 * Reclamation for Lock-Free objects" and it is available here:
 * http://web.cecs.pdx.edu/~walpole/class/cs510/papers/11.pdf
 *
 * @author Pedro Ramalhete
 * @author Andreia Correia
 */
template<typename T>
class FAAArrayQueue {
    static const long BUFFER_SIZE = 1024;  // 1024

private:
    struct Node {
        std::atomic<int>   deqidx;
        std::atomic<int32_t> items[BUFFER_SIZE];
        std::atomic<int>   enqidx;
        std::atomic<Node*> next;
        int                node_idx;

        // Start with the first entry pre-filled and enqidx at 1
        Node(int32_t item, int node_idx) : deqidx{0}, enqidx{1}, next{nullptr}, node_idx{node_idx} {
            items[0].store(item, std::memory_order_relaxed);
            for (long i = 1; i < BUFFER_SIZE; i++) {
            items[i].store(-1, std::memory_order_relaxed);
            }
        }

        bool casNext(Node *cmp, Node *val) {
            return next.compare_exchange_strong(cmp, val);
        }
    };

    bool casTail(Node *cmp, Node *val) {
		return tail.compare_exchange_strong(cmp, val);
	}

    bool casHead(Node *cmp, Node *val) {
        return head.compare_exchange_strong(cmp, val);
    }

    // Pointers to head and tail of the list
    alignas(128) std::atomic<Node*> head;
    alignas(128) std::atomic<Node*> tail;

    static const int MAX_THREADS = 256;
    const int maxThreads;

    int32_t taken = -2;  // Muuuahahah !

    // We need just one hazard pointer
    HazardPointers<Node> hp {1, maxThreads};
    const int kHpTail = 0;
    const int kHpHead = 0;


public:
    FAAArrayQueue(int maxThreads=MAX_THREADS) : maxThreads{maxThreads} {
        Node* sentinelNode = new Node(-1, 0);
        sentinelNode->enqidx.store(0, std::memory_order_relaxed);
        head.store(sentinelNode, std::memory_order_relaxed);
        tail.store(sentinelNode, std::memory_order_relaxed);
    }


    ~FAAArrayQueue() {
        while (dequeue(0) != -1);      // Drain the queue
        delete head.load();            // Delete the last node
    }


    std::string className() { return "FAAArrayQueue"; }


    void enqueue(int32_t item, const int tid) {
        if (item == -1) throw std::invalid_argument("item can not be null value (-1)");
        while (true) {
            Node* ltail = hp.protect(kHpTail, tail, tid);
            const int idx = ltail->enqidx.fetch_add(1);
            if (idx > BUFFER_SIZE-1) { // This node is full
                if (ltail != tail.load()) continue;
                Node* lnext = ltail->next.load();
                if (lnext == nullptr) {
                    Node* newNode = new Node(item, ltail->node_idx + 1);
                    if (ltail->casNext(nullptr, newNode)) {
                        casTail(ltail, newNode);
                        hp.clear(tid);
                        return;
                    }
                    delete newNode;
                } else {
                    casTail(ltail, lnext);
                }
                continue;
            }
            int32_t itemnull = -1;
            if (ltail->items[idx].compare_exchange_strong(itemnull, item)) {
                hp.clear(tid);
                return;
            }
        }
    }


    int32_t dequeue(const int tid) {
        while (true) {
            Node* lhead = hp.protect(kHpHead, head, tid);
            if (lhead->deqidx.load() >= lhead->enqidx.load() && lhead->next.load() == nullptr) break;
            const int idx = lhead->deqidx.fetch_add(1);
            if (idx > BUFFER_SIZE-1) { // This node has been drained, check if there is another one
                Node* lnext = lhead->next.load();
                if (lnext == nullptr) break;  // No more nodes in the queue
                if (casHead(lhead, lnext)) hp.retire(lhead, tid);
                continue;
            }
            int32_t item = lhead->items[idx].exchange(taken);
            if (item == -1) continue;
            hp.clear(tid);
            return item;
        }
        hp.clear(tid);
        return -1;
    }

    int enqueue_count(const int tid) {
        Node* ltail = hp.protect(kHpTail, tail, tid);
        int idx = ltail->enqidx.load();
        if (idx > BUFFER_SIZE - 1) {
            idx = BUFFER_SIZE;
        }
        auto res = idx + BUFFER_SIZE * ltail->node_idx;
        hp.clear(tid);
        return res;
    }

    int dequeue_count(const int tid) {
        Node* lhead = hp.protect(kHpHead, head, tid);
        int idx = lhead->deqidx.load();
        if (idx > BUFFER_SIZE - 1) {
            idx = BUFFER_SIZE;
        }
        auto res = idx + BUFFER_SIZE * lhead->node_idx;
        hp.clear(tid);
        return res;
    }

    int enqueue_version(const int tid) {
        Node* ltail = hp.protect(kHpTail, tail, tid);
        int idx = ltail->enqidx.load();
        if (idx > BUFFER_SIZE - 1) {
            idx = BUFFER_SIZE;
        }
        auto res = idx + BUFFER_SIZE * ltail->node_idx;
        hp.clear(tid);
        return res;
    }
};

}

#endif /* _FAA_ARRAY_QUEUE_INT_HP_H_ */