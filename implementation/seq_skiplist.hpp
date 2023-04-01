#pragma once

#include <vector>
#include <limits>
#include <iostream>

#include "random_generator.hpp"

template <class Key, class Value>
class SeqSkipList
{
private:
    using Level = int;
    struct Node {
        Node(Key k, Value val, Level lev) : key(k), value(val), next(lev) {};
        Key key;
        Value value;
        std::vector<Node*> next; 
        Level get_level() { return next.size();}
    };
public:
    SeqSkipList(const double &probability, const Level &max_level = 20) : _p(probability), _max_level(max_level) {
        _head = new Node(_min_key, Value(), _max_level);
        _tail = new Node(_max_key, Value(), _max_level);
        for(int i = 0; i < _max_level; ++i) {
            _head -> next[i] = _tail;
        }
    };

    //checks if element with search_key exist, if so it returs the value of the searched element
    std::pair<bool, Value> search(Key &search_key) {
        Node *cur = _head;
        for(int i = _max_level - 1; i >= 0; --i) {
            while(cur -> next[i] -> key < search_key) {
                cur = cur -> next[i];
            }
        }
        cur = cur -> next[0];
        return {cur -> key == search_key, cur -> value};
    }

    void insert(Key &insert_key, Value &value) {
        auto update = get_update_nodes(insert_key);
        Node *x = update[0] -> next[0];
        if(x -> key == insert_key) {
            x -> value = value;
        }
        else {
            Level random_level = random_gen::random_level(_p, _max_level);
            Node *new_node = new Node(insert_key, value, random_level);
            for(int i = 0; i < random_level; ++i) {
                new_node -> next[i] = update[i] -> next[i];
                update[i] -> next[i] = new_node;
            }
        }
    }
    //returns if element was in list
    bool remove(Key &remove_key) {
        auto update = get_update_nodes(remove_key);
        Node *to_remove = update[0] -> next[0];
        if(to_remove -> key == remove_key) {
            for(int i = 0; i < to_remove -> get_level(); ++i) {
                update[i] -> next[i] = to_remove -> next[i];
            }
            delete to_remove;
            return true;
        }
        return false;
    }

    void print() {
        Node *cur = _head;
        while(cur -> next[0] != NULL) {
            cur = cur -> next[0];
            std::cout << cur -> key << " " << cur -> value << "\n";
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

    bool is_consistent() {
        Node *cur = _head;
        bool ok = true;
        while(cur -> key != _max_key) {
            ok &= cur -> key < cur -> next[0] -> key;
            cur = cur -> next[0];
        }
        return ok;
    }

private:
    //returns a vector of the rightmost node visited on each level
    std::vector<Node*> get_update_nodes(Key &search_key) {
        std::vector<Node*> update(_max_level);
        Node *cur = _head;
        for(int i = _max_level - 1; i >= 0; --i) {
            while(cur -> next[i] -> key < search_key) {
                cur = cur -> next[i];
            }
            update[i] = cur;
        }
        return update;
    }

    const double _p;
    const Level _max_level;
    Node *_head;
    Node *_tail;

    const Key _min_key = std::numeric_limits<Key>::min();
    const Key _max_key = std::numeric_limits<Key>::max();
};
