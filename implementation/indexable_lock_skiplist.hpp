#pragma once

#include <vector>
#include <limits>
#include <iostream>
#include <atomic>
#include <queue>

#include "implementation/spinlock.hpp"
#include "random_generator.hpp"


//must call method that updates all changed lengths
template <class Key, class Value>
class IndexableLockSkipList
{
private:
    using Level = int;
    using Length = int;
    using Lock = lock::Spinlock;
    using QueueLock = lock::Spinlock;

    struct Node {
        Node(Key k, Value val, Level lev) : 
         key(k), value(val), level(lev), lock(),
         beeing_deleted(false), fully_linked(false) {
            next = new std::atomic<Node*>[level];
            length = new std::atomic<Length>[level];
         };
        ~Node() {
            delete next;
            delete length;
        }
        std::atomic<Key> key;
        std::atomic<Value> value;
        std::atomic<Node*> *next;
        std::atomic<Length> *length;
        Level level;
        Lock lock;
        std::atomic<bool> beeing_deleted; //some threads currently deletes this node
        std::atomic<bool> fully_linked; //all pointers are set
    };

public:
    IndexableLockSkipList(const double &probability, const Level &max_level = 20) : _p(probability), _max_level(max_level) {
        _head = new Node(_min_key, Value(), _max_level);
        _tail = new Node(_max_key, Value(), _max_level);
        for(int i = 0; i < _max_level; ++i) {
            _head -> next[i] = _tail;
            _head -> length[i] = 1;
        }
        for(int i = 0; i < _max_level; ++i) {
            _tail -> next[i] = _tail; //safety
            _tail -> length[i] = 0;
        }
        _queue_locks = new QueueLock[_num_queues];
    };

    ~IndexableLockSkipList() {
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
        return {cur -> key == search_key && !(cur -> beeing_deleted) && cur -> fully_linked, cur -> value};
    }

    void insert(Key insert_key, Value value) {
        std::vector<Node*> preds(_max_level);
        std::vector<Node*> succs(_max_level);
        Level random_level = random_gen::random_level(_p, _max_level);
        get_update_nodes(preds, succs, insert_key);
        Node *x = succs[0];
        if(x -> key == insert_key) {
            x -> value = value;
            return;
        }
        Node *new_node = new Node(insert_key, value, random_level);
        for(Level i = 0; i < random_level; ++i) {
            new_node -> next[i] = succs[i];
        }
        new_node -> lock.lock(); // -> if node is not fully linked, other threads could already access this node
        auto validate = [&](int j) {
            return !(preds[j] -> beeing_deleted) && !(succs[j] -> beeing_deleted) && preds[j] -> next[j] == succs[j];
        };
        auto try_insert_at = [&](int j) {
            if(validate(j)) {
                preds[j] -> lock.lock();
                if(validate(j)) {
                    new_node -> next[j] = succs[j]; //update succs
                    preds[j] -> next[j] = new_node;
                    preds[j] -> lock.unlock();
                    return true;
                    }
                preds[j] -> lock.unlock();
            }
            get_update_nodes(preds, succs, insert_key);
            return false;
        };
        //thread that gets 0 level gets all levels
        while(true) {
            if(succs[0] -> key == insert_key) {
                delete new_node;
                return; //element was inserted by other thread
            }
            if(try_insert_at(0)) {
                break;
            }
        }
        for(Level i = 1; i < random_level; ++i) {
            while(!try_insert_at(i)) {}
        }
        new_node -> fully_linked = true;
        new_node -> lock.unlock(); //test
        return;
    }

    bool remove(Key remove_key) {
        std::vector<Node*> preds(_max_level);
        std::vector<Node*> succs(_max_level);
        get_update_nodes(preds, succs, remove_key);
        Node *victim = succs[0];
        if(victim -> key != remove_key || !(victim -> fully_linked)) {
            return false;
        }
        if(victim -> beeing_deleted) {
            return true;
        }
        //one thread set marked flag and continues removing it
        bool expect = false;
        if(!victim -> beeing_deleted.compare_exchange_strong(expect, true)) {
            return true;
        }
        Level node_level = victim -> level;        
        auto validate = [&](int j) {
            return !(preds[j] -> beeing_deleted) && preds[j] -> next[j] == victim;
        };
        victim -> lock.lock();
        for(Level i = node_level - 1; i >= 0; --i) {
            while(true) {
                if(validate(i)) {
                    preds[i] -> lock.lock();
                    if(validate(i)) {
                        preds[i] -> next[i] = victim -> next[i].load();
                        preds[i] -> lock.unlock();
                        break;
                    }
                    preds[i] -> lock.unlock();
                }
                get_update_nodes(preds, succs, remove_key);
            }
        }
        victim -> lock.unlock();
        //garbage collection
        size_t index = random_gen::random_index(_num_queues);
        _queue_locks[index].lock();
        _queues[index].push(victim);
        _queue_locks[index].unlock();
        return true;
    }

    //O(#pointers) = O(n * 1/p), sequential
    void compute_indices() {
        std::vector<Node*> preds(_max_level, _head);
        std::vector<Node*> succs(_max_level);
        std::vector<Length> index(_max_level, -1); //index of node last node at this level = preds nodes
        Node *cur = _head -> next[0].load();
        Length cur_index = 0;
        while(preds[0] -> key != _max_key) {
            for(int i = 0; i < cur -> level; i++) {
                preds[i] -> length[i] = cur_index - index[i];
                preds[i] = cur;
                index[i] = cur_index;
            }
            cur = cur -> next[0];
            cur_index++;
        }
    }

        //checks if index in range, if so returs value at i
    std::pair<bool, Value> element_at(int search_index) {
        Node *cur = _head;
        int cur_index = -1;
        for(int i = _max_level - 1; i >= 0; --i) {
            while(cur -> key != _max_key && cur_index + cur -> length[i] <= search_index) {
                cur_index += cur -> length[i];
                cur = cur -> next[i];
            }
        }
        //checks if we are not on last dummy
        return {cur -> key != _max_key && cur_index == search_index, cur -> value};
    }

    //checks if element exist, if so returs its rank
    std::pair<bool, Value> rank(Key search_key) {
        Node *cur = _head;
        int cur_index = -1;
        for(int i = _max_level - 1; i >= 0; --i) {
            while(cur -> next[i].load() -> key <= search_key) {
                cur_index += cur -> length[i];
                cur = cur -> next[i];
            }
        }
        return {cur -> key == search_key, cur_index};
    }

    //holds only for permuation from 0,..., n ! 
    bool is_consistent() {
        compute_indices();
        Node *cur = _head;
        bool ok = true;
        while(cur -> key != _max_key) {
            for(int i = 0; i < cur -> level; i++) {
                Key k1 = cur -> key, k2 = cur -> next[i].load() -> key;
                Length l = cur -> length[i];
                k1 = k1 == _min_key ? -1 : k1;
                if(!(k1 < k2)) std::cout << "not all < \n";
                ok &= k1 < k2;
                if(!(k2 == _max_key || k1 + l == k2)) {
                    std::cout << "length wrong " << k1 << " " << l << " " << k2 << "\n";
                }
                ok &= k2 == _max_key || k1 + l == k2; 
            }
            cur = cur -> next[0];
        }
        return ok;
    }

    void print() {
        Node *cur = _head;
        while(cur -> key != _max_key) {
            std::cout << "key: " << cur -> key << " ";
            std::cout << "next keys + length: ";
            for(int i = 0; i < cur -> level; i++) {
                std::cout << "(" << cur -> next[i].load() -> key << " " << cur -> length[i] << ") ";
            }
            std::cout << "\n";
            cur = cur -> next[0].load();
        }
    }

    std::vector<Key> get_keys() {
        Node *cur = _head -> next[0];
        std::vector<Key> v;
        while(cur -> key != _max_key) {
            v.push_back(cur -> key);
            cur = cur -> next[0];
        }
        return v;
    }

private:
    //returns a vector predecessors and successors, waitfree
    void get_update_nodes(std::vector<Node*> &preds, std::vector<Node*> &succs, Key search_key) {
        Node *pred = _head;
        Node *succ;
        for(int i = _max_level - 1; i >= 0; --i) {
            succ = pred -> next[i];
            // 2. condition for tail
            while(succ -> key < search_key && succ -> key != succ -> next[i].load() -> key)  {
                pred = succ;
                succ = succ -> next[i];
            }
            preds[i] = pred;
            succs[i] = succ;
        }
        return;
    }

    const double _p;
    const Level _max_level;
    Node *_head;
    Node *_tail;

    size_t _num_queues = 12;
    std::vector<std::queue<Node*>> _queues{_num_queues};
    QueueLock *_queue_locks;

    const Key _min_key = std::numeric_limits<Key>::min();
    const Key _max_key = std::numeric_limits<Key>::max();
};
