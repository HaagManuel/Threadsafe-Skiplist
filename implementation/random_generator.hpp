#pragma once
#include <random>
#include <algorithm>

namespace random_gen {

    size_t random_index(const size_t upper) {
        static thread_local std::mt19937 generator;
        std::uniform_int_distribution<int> distribution(0, upper - 1);
        return distribution(generator);
    }
    
    //inclusive in both directions
    size_t random_range(const size_t lower, const size_t upper) {
        static thread_local std::mt19937 generator;
        std::uniform_int_distribution<int> distribution(lower, upper);
        return distribution(generator);
    }

    // Prob = 1/p, p in [0,1]
    bool prob_p(const double &p) {
        static thread_local std::minstd_rand gen(std::random_device{}());
        std::uniform_real_distribution<double> dist(0, 1);
        return dist(gen) <= p;
    }

    int random_level(const double &p, const int &max_level) {
        int level = 1;
        while(prob_p(p) && level < max_level) level++;
        return level;
    }
    
    template <class T>
    void shuffle(std::vector<T> &v) {
        std::random_device rd;
        std::mt19937 g(rd());
        std::shuffle(v.begin(), v.end(), g);
    }

    //n random swap with adjacent neighbor
    template <class T>
    void weak_shuffle(std::vector<T> &v) {
        if(v.size() < 3) { return;}
        for(uint i = 0; i < v.size(); ++i) {
            size_t index = random_index(v.size() - 1);
            std::swap(v[index], v[index + 1]);
        }
    }

    template <class T>
    void sort_and_weak_shuffle(std::vector<T> &v) {
        if(v.size() < 3) { return;}
        sort(v.begin(), v.end());
        for(uint i = 0; i < v.size(); ++i) {
            size_t index = random_index(v.size() - 1);
            std::swap(v[index], v[index + 1]);
        }
    }

    static uint64_t s[2] = { 0x41, 0x29837592 };
    static inline uint64_t rotl(const uint64_t x, int k) {
        return (x << k) | (x >> (64 - k));
    }

    uint64_t next(void) {
        const uint64_t s0 = s[0];
        uint64_t s1 = s[1];
        const uint64_t result = s0 + s1;
        s1 ^= s0;
        s[0] = rotl(s0, 55) ^ s1 ^ (s1 << 14); // a, b
        s[1] = rotl(s1, 36); // c
        return result;
    }

    bool fast_prob_p() {
        return next() & 1;
    }

    int fast_random_level(const int &max_level) {
        int level = 1;
        while(fast_prob_p() && level < max_level) level++;
        return level;
    }
}