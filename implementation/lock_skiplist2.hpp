#pragma once

#include <vector>
#include <limits>
#include <iostream>
#include <atomic>
#include <queue>

#include "implementation/spinlock.hpp"
#include "random_generator.hpp"


//link each level individually, shared ptr for garbage collection -> very slow
template <class Key, class Value>
class LockSkipList2
{
private:
    struct Node;
    using NodePtr = std::shared_ptr<Node>; //access with std::atomic_ functions 
    using Level = int;
    using Lock = lock::Spinlock;
    // using Lock = std::mutex; //slower
    struct Node {
        Node(Key k, Value val, Level lev) : 
         key(k), value(val), level(lev), lock(),
         beeing_deleted(false), fully_linked(false) {
             next = new NodePtr[level];
         };
        ~Node() {
            // delete next;
        }
        std::atomic<Key> key;
        std::atomic<Value> value;
        NodePtr *next;
        Level level;
        Lock lock;
        std::atomic<bool> beeing_deleted; //some threads currently deletes this node
        std::atomic<bool> fully_linked;   //all pointers are set
    };
public:
    LockSkipList2(const double &probability, const Level &max_level = 20) : _p(probability), _max_level(max_level) {
        _head = std::make_shared<Node>(_min_key, Value(), _max_level);
        _tail = std::make_shared<Node>(_max_key, Value(), _max_level);

        for(int i = 0; i < _max_level; ++i) {
            _head -> next[i] = _tail;
        }
        for(int i = 0; i < _max_level; ++i) {
            _tail -> next[i] = _tail; //safety
        }
    };

    std::pair<bool, Value> search(Key search_key) {
        std::vector<NodePtr> preds(_max_level);
        std::vector<NodePtr> succs(_max_level);
        get_update_nodes(preds, succs, search_key);
        NodePtr cur = succs[0];
        return {cur -> key == search_key && !(cur -> beeing_deleted) && cur -> fully_linked, cur -> value};
    }

    void insert(Key insert_key, Value value) {
        std::vector<NodePtr> preds(_max_level);
        std::vector<NodePtr> succs(_max_level);
        Level random_level = random_gen::random_level(_p, _max_level);
        get_update_nodes(preds, succs, insert_key);
        NodePtr x = succs[0];
        if(x -> key == insert_key) {
            x -> value = value;
            return;
        }
        NodePtr new_node = std::make_shared<Node>(insert_key, value, random_level);
        for(Level i = 0; i < random_level; ++i) {
            new_node -> next[i] = succs[i]; //node is not shared yet
        }
        new_node -> lock.lock(); // -> if node is not fully linked, other threads could already access this node
        auto validate = [&](int j) {
            return !(preds[j] -> beeing_deleted) && !(succs[j] -> beeing_deleted) && preds[j] -> next[j] == succs[j];
        };
        auto try_insert_at = [&](int j) {
            if(validate(j)) {
                preds[j] -> lock.lock();
                if(validate(j)) {
                    // new_node -> next[j] = succs[j]; //update succs
                     std::atomic_store(&(new_node -> next[j]), succs[j]);
                    // preds[j] -> next[j] = new_node;
                    std::atomic_store(&(preds[j] -> next[j]), new_node);
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
        std::vector<NodePtr> preds(_max_level);
        std::vector<NodePtr> succs(_max_level);
        get_update_nodes(preds, succs, remove_key);
        NodePtr victim = succs[0];
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
                        // preds[i] -> next[i] = victim -> next[i].load();
                        std::atomic_store(&(preds[i] -> next[i]), victim -> next[i]);
                        preds[i] -> lock.unlock();
                        break;
                    }
                    preds[i] -> lock.unlock();
                }
                get_update_nodes(preds, succs, remove_key);
            }
        }
        victim -> lock.unlock();
        return true;
    }

    bool is_consistent() {
        NodePtr cur = _head;
        bool ok = true;
        while(cur -> key != _max_key) {
            ok &= cur -> key < cur -> next[0] -> key;
            cur = cur -> next[0];
        }
        return ok;
    }

    void print() {
        NodePtr cur = _head;
        while(cur -> key != _max_key) {
            std::cout << cur -> key << " ";
            cur = cur -> next[0];
        }
        std::cout << "\n";
    }

    std::vector<Key> get_keys() {
        NodePtr cur = _head -> next[0];
        std::vector<Key> v;
        while(cur -> key != _max_key) {
            v.push_back(cur -> key);
            cur = cur -> next[0];
        }
        return v;
    }

private:
    //returns a vector predecessors and successors, waitfree
    void get_update_nodes(std::vector<NodePtr> &preds, std::vector<NodePtr> &succs,  Key search_key) {
        // NodePtr pred = _head;
        NodePtr pred = std::atomic_load(&_head);
        NodePtr succ;
        for(int i = _max_level - 1; i >= 0; --i) {
            // succ = pred -> next[i];
            succ = std::atomic_load(&(pred -> next[i]));
            //2. condition for tail
            // while(succ -> key < search_key && succ -> key != succ -> next[i].load() -> key)  {
            while(succ -> key < search_key && succ -> key != std::atomic_load(&(succ -> next[i])) -> key)  {
                pred = succ;
                // succ = succ -> next[i];
                succ = std::atomic_load(&(succ -> next[i]));
            }
            preds[i] = pred;
            succs[i] = succ;
        }
        return;
    }

    const double _p;
    const Level _max_level;
    NodePtr _head;
    NodePtr _tail;

    const Key _min_key = std::numeric_limits<Key>::min();
    const Key _max_key = std::numeric_limits<Key>::max();
};
