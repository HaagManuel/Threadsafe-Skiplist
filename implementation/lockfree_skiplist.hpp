#pragma once

#include <vector>
#include <limits>
#include <iostream>
#include <atomic>
#include <memory>
#include <mutex>

#include "implementation/spinlock.hpp"
#include "implementation/markable_reference.hpp"
#include "random_generator.hpp"

#define COUNTER_SIZE 100

template <class Key, class Value, bool do_count = false>
class LockFreeSkipList
{
private:
    struct Node;
    using Level = int;
    using MarkPtr = pointer::MarkableReference<Node>;
    using QueueLock = lock::Spinlock;
    // using QueueLock = std::mutex;
    struct Node {
        Node(Key k, Value val, Level lev) : 
         key(k), value(val), level(lev) {
             next = new std::atomic<MarkPtr>[level];
         };
        ~Node() {
            delete next;
        }
        std::atomic<Key> key;
        std::atomic<Value> value;
        std::atomic<MarkPtr> *next; //mark all pointers from node, that should be removed
        Level level;
    };
    
public:
    LockFreeSkipList(const double &probability, const Level &max_level = 20) : _p(probability), _max_level(max_level) {
        _head = new Node(std::numeric_limits<Key>::min(), Value(), _max_level);
        _tail = new Node(std::numeric_limits<Key>::max(), Value(), _max_level);
        for(int i = 0; i < _max_level; ++i) {
            _head -> next[i] = _tail;
        }
        for(int i = 0; i < _max_level; ++i) {
            _tail -> next[i] = _tail; //safety
        }
        _queue_locks = new QueueLock[_num_queues];
    };

    ~LockFreeSkipList() {
        for(auto &q : _queues) {
            while(!q.empty()) {
                auto ptr = q.front(); q.pop();
                delete ptr;
            }
        }
        delete _queue_locks;
    }

    std::pair<bool, Value> search(Key search_key) {
        std::vector<Node*> preds(_max_level);
        std::vector<Node*> succs(_max_level);
        get_update_nodes(preds, succs, search_key);
        Node *cur = succs[0];
        return {cur -> key == search_key, cur -> value};
    }

    void insert(Key insert_key, Value value) {
        std::vector<Node*> preds(_max_level);
        std::vector<Node*> succs(_max_level);
        Level random_level = random_gen::random_level(_p, _max_level);
        get_update_nodes(preds, succs, insert_key);
        Node* x = succs[0];
        if(x -> key == insert_key) {
            x -> value = value;
            return;
        }
        Node* new_node = new Node(insert_key, value, random_level);
        auto try_link_at = [&](int j) {
            MarkPtr expect = {succs[j], false};
            return preds[j] -> next[j].compare_exchange_strong(expect, {new_node, false});
        };
        while(true) {
            for(Level lev = 0; lev < random_level; ++lev) {
               new_node -> next[lev] = {succs[lev], false};
            }
            if(succs[0] -> key == insert_key) {
                delete new_node;
                return; //other thread inserted it
            }
            if(try_link_at(0)) {
                break;
            }
            get_update_nodes(preds, succs, insert_key);
        }
        for(Level lev = 1; lev < random_level; ++lev) {
            while(!try_link_at(lev)) {
                get_update_nodes(preds, succs, insert_key);
            }
        }
    }

    //returns if element was in list and is in the process of beeing removed
    bool remove(Key remove_key) {
        std::vector<Node*> preds(_max_level);
        std::vector<Node*> succs(_max_level);
        get_update_nodes(preds, succs, remove_key);
        Node *victim = succs[0];
        if(victim -> key != remove_key) {
            return false;
        }
        Level node_level = victim -> level;
        bool i_marked_last = false;
        //mark outgoing pointer of victim
        for(Level lv = node_level - 1; lv >= 0; --lv) {
            MarkPtr next_node = victim -> next[lv].load();
            while(!next_node.getMark()) {
                Node *ref = next_node.getRef();
                MarkPtr expect = {ref, false};
                bool marked_it = victim -> next[lv].compare_exchange_weak(expect, {ref, true});
                if(lv == 0 && marked_it) i_marked_last = true;
                next_node = victim -> next[lv].load();
            }
        }
        get_update_nodes(preds, succs, remove_key); //deleted marked nodes
        //ensure element is only pushed once into garbage queue
        if(i_marked_last) {
            size_t index = random_gen::random_index(_num_queues);
            _queue_locks[index].lock();
            _queues[index].push(victim);
            _queue_locks[index].unlock();
        }
        return true;
    }

    bool is_consistent() {
        Node* cur = _head;
        bool ok = true;
        while(cur -> key != _max_key) {
            ok &= cur -> key < cur -> next[0].load() -> key;
            cur = cur -> next[0].load().getRef();
        }
        return ok;
    }

    void print() {
        Node *cur = _head;
        while(cur -> key != _max_key) {
            std::cout << cur -> key << " ";
            cur = cur -> next[0];
        }
        std::cout << "\n";
    }

    std::vector<Key> get_keys() {
        Node *cur = _head -> next[0].load().getRef();
        std::vector<Key> v;
        while(cur -> key != _max_key) {
            v.push_back(cur -> key);
            cur = cur -> next[0].load().getRef();
        }
        return v;
    }

    void init_counter() {
        for(int i = 0; i < COUNTER_SIZE; ++i) {
            _counter1[i] = 0;
            _counter2[i] = 0;
        }
    }

    std::pair<size_t, size_t> collect_counter() {
        size_t num_finds = 0;
        size_t num_find_retries = 0;
        for(int i = 0; i < COUNTER_SIZE; ++i) {
            num_finds += _counter1[i];
            num_find_retries += _counter2[i];
        }
        return {num_finds, num_find_retries};
    }

private:
    //returns a vector predecessors and successors, waitfree
    void get_update_nodes(std::vector<Node*> &preds, std::vector<Node*> &succs,  Key search_key) {
        if constexpr(do_count) _counter1[random_gen::random_index(COUNTER_SIZE)]++; //special metric
        bool snip = false;
        MarkPtr pred, cur, succ;
        retry:
        pred = {_head, false};
        for(int i = _max_level - 1; i >= 0; --i) {
            cur = pred -> next[i];
            while(true) {
                succ = cur -> next[i];
                while(succ.getMark()) {
                    MarkPtr expect = {cur.getRef(), false};
                    // lazy removal of marked nodes
                    snip = (pred -> next[i].compare_exchange_strong(expect, {succ.getRef(), false}));
                    if(!snip) {
                        if constexpr(do_count) _counter2[random_gen::random_index(COUNTER_SIZE)]++; //special metric
                        goto retry;     
                    }
                    cur = pred -> next[i];
                    succ = cur -> next[i];
                }
                if(cur -> key < search_key) {
                    pred = cur;
                    cur = succ;
                }
                else {
                    break;
                }
            }
            preds[i] = pred.getRef();
            succs[i] = cur.getRef();
        }
        return;
    }

    const double _p;
    const Level _max_level;
    Node* _head;
    Node* _tail;

    size_t _num_queues = 12;
    std::vector<std::queue<Node*>> _queues{_num_queues};
    QueueLock *_queue_locks;

    std::atomic<size_t> _counter1[COUNTER_SIZE];
    std::atomic<size_t> _counter2[COUNTER_SIZE];

    const Key _min_key = std::numeric_limits<Key>::min();
    const Key _max_key = std::numeric_limits<Key>::max();
};
