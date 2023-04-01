#pragma once

#include <vector>
#include <limits>
#include <iostream>

#include "random_generator.hpp"

template <class Key, class Value>
class IndexableSeqSkipList
{
private:
    using Level = int;
    struct Node {
        Node(Key k, Value val, Level lev) : key(k), value(val), next(lev), length_next(lev, 0) {};
        Key key;
        Value value;
        std::vector<Node*> next; 
        std::vector<int> length_next; 
        Level get_level() { return next.size();}
    };
public:
    IndexableSeqSkipList(const double &probability, const Level &max_level = 20) : _p(probability), _max_level(max_level) {
        _head = new Node(std::numeric_limits<Key>::min(), Value(), _max_level);
        _tail = new Node(std::numeric_limits<Key>::max(), Value(), _max_level);
        for(int i = 0; i < _max_level; ++i) {
            _head -> next[i] = _tail;
            _head -> length_next[i] = 1;
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

    //checks if index in range, if so returs value at i
    std::pair<bool, Value> element_at(int search_index) {
        Node *cur = _head;
        int cur_index = -1;
        for(int i = _max_level - 1; i >= 0; --i) {
            while(cur -> next[i] != NULL && cur_index + cur -> length_next[i] <= search_index) {
                cur_index += cur -> length_next[i];
                cur = cur -> next[i];
            }
        }
        //checks if we are not on last dummy
        return {cur -> next[0] != NULL, cur -> value};
    }

    //checks if element exist, if so returs its rank
    std::pair<bool, Value> rank(Key &search_key) {
        Node *cur = _head;
        int cur_index = -1;
        for(int i = _max_level - 1; i >= 0; --i) {
            while(cur -> next[i] -> key <= search_key) {
                cur_index += cur -> length_next[i];
                cur = cur -> next[i];
            }
        }
        //checks if we are not on last dummy
        return {cur -> next[0] != NULL, cur_index};
    }

    void insert(Key &insert_key, Value &value) {
        auto [update, index] = get_update_nodes(insert_key);
        Node *x = update[0] -> next[0];
        if(x -> key == insert_key) {
            x -> value = value;
        }
        else {
            Level random_level = random_gen::random_level(_p, _max_level);
            Node *new_node = new Node(insert_key, value, random_level);
            int new_node_index = index[0] + 1;
            for(int i = 0; i < random_level; ++i) {
                new_node -> length_next[i] = (update[i] -> length_next[i]) - (new_node_index - index[i]) + 1;
                update[i] -> length_next[i] = new_node_index - index[i];

                new_node -> next[i] = update[i] -> next[i];
                update[i] -> next[i] = new_node;
            }
            for(int i = random_level; i < (int)update.size(); ++i) {
                update[i] -> length_next[i]++;
            }
        }
    }
    //returns if element was in list
    bool remove(Key &remove_key) {
        auto [update, index] = get_update_nodes(remove_key);
        Node *to_remove = update[0] -> next[0];
        if(to_remove -> key == remove_key) {
            Level remove_level = to_remove -> get_level();
            for(int i = 0; i < remove_level; ++i) {
                update[i] -> length_next[i] += to_remove -> length_next[i] - 1;

                update[i] -> next[i] = to_remove -> next[i];
            }
            for(int i = remove_level; i < (int) update.size(); ++i) {
                update[i] -> length_next[i]--;
            }
            delete to_remove;
            return true;
        }
        return false;
    }

    void print() {
        Node *cur = _head;
        while(cur -> next[0] != NULL) {
            std::cout << "key: " << cur -> key << " ";
            std::cout << "next keys + length: ";
            for(int i = 0; i < cur -> get_level(); i++) {
                std::cout << "(" << cur -> next[i] -> key << " " << cur -> length_next[i] << ") ";
            }
            std::cout << "\n";
            cur = cur -> next[0];
        }
    }

private:
    //returns a vector of the rightmost node visited on each level and it's index
    std::pair<std::vector<Node*>, std::vector<int>> get_update_nodes(Key &search_key) {
        std::vector<Node*> update(_max_level);
        std::vector<int> index(_max_level);
        Node *cur = _head;
        int cur_index = 0;
        for(int i = _max_level - 1; i >= 0; --i) {
            while(cur -> next[i] -> key < search_key) {
                cur_index += cur -> length_next[i];
                cur = cur -> next[i];
            }
            update[i] = cur;
            index[i] = cur_index;
        }
        return {update, index};
    }

    const double _p;
    const Level _max_level;
    Node *_head;
    Node *_tail;

};
