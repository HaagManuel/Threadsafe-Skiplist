#include <iostream>
#include <fstream>
#include <random>
#include <chrono>
#include <limits>
#include <vector>
#include <string>
#include <thread>
#include <atomic>

#include <memory>

#include "implementation/random_generator.hpp"
#include "implementation/seq_skiplist.hpp"
#include "implementation/indexable_seq_skiplist.hpp"

#include "implementation/indexable_lock_skiplist.hpp"

#include "implementation/lock_skiplist.hpp"
#include "implementation/lock_skiplist2.hpp"

#include "implementation/lockfree_skiplist.hpp"


#define V(x) std::string(#x "=") << (x) << " "

void test_random_generator(const double p, const int max_level, const int n) {
    std::cout << "random levels" << "\n";
    for(int i = 0; i < n; i++) {
        std::cout << random_gen::random_level(p, max_level) << " ";
    }
    std::cout << "\n random numbers" << "\n";
    for(int i = 0; i < n; i++) {
        std::cout << random_gen::prob_p(p) << " ";
    }    
    std::cout << "\n";
}

void test_seq_skiplist_with(const double p, const int max_level, std::vector<int> &v) {
    SeqSkipList<int,int> slist(p, max_level);
    for(auto &x : v) {
        slist.insert(x, x);
        auto[is_in, value] = slist.search(x);
        if(!is_in || value != x) {
            std::cout << "failed insert " << x << "\n";
            return;
        }
    }
    for(auto &x : v) {
        bool success = slist.remove(x);
        auto[is_in, value] = slist.search(x);
        if(!success || is_in) {
            std::cout << "failed removing " << x << "\n";
            return;
        }
    }
    std::cout << "test ok \n";
}

void test_seq_skiplist(const double p, const int max_level, const int n, const int it) {
    std::vector<int> v(n);
    std::iota(v.begin(), v.end(), 0);
    for(int i = 0; i < it; ++i) {
        test_seq_skiplist_with(p, max_level, v);
        random_gen::shuffle<int>(v);
    }
}

void test_index_seq_skiplist_with(const double p, const int max_level, std::vector<int> &v) {
    IndexableSeqSkipList<int,int> slist(p, max_level);
    for(auto &x : v) {
        slist.insert(x, x);
        auto[is_in, value] = slist.search(x);
        if(!is_in || value != x) {
            std::cout << "failed insert " << x << "\n";
            return;
        }
    }

    for(auto &x : v) {
        auto[found, value] = slist.element_at(x); //permuation
        if(!found || value != x) {
            std::cout << "failed element at " << x << "\n";
            return;
        }
    }

    for(auto &x : v) {
        auto[found, rank] = slist.rank(x); //permuation
        if(!found || rank != x) {
            std::cout << "failed rank " << x << "\n";
            return;
        }
    }
    for(auto &x : v) {
        bool success = slist.remove(x);
        auto[is_in, value] = slist.search(x);
        if(!success || is_in) {
            std::cout << "failed removing " << x << "\n";
            return;
        }
    }
    std::cout << "test ok \n";
}

void test_index_seq_skiplist(const double p, const int max_level, const int n, const int it) {
    std::vector<int> v(n);
    std::iota(v.begin(), v.end(), 0);
    for(int i = 0; i < it; ++i) {
        test_index_seq_skiplist_with(p, max_level, v);
        random_gen::shuffle<int>(v);
    }
}

bool test_index_par_skiplist_with(const double p, const int max_level, const int num_threads, std::vector<int> &v) {
    std::vector<std::vector<int>> work_thread(num_threads);
    for(uint i = 0; i < v.size(); ++i) {
            work_thread[i % num_threads].push_back(v[i]);
    }
    IndexableLockSkipList<int,int> slist(p, max_level);
    std::vector<std::thread> threads;
    std::atomic<bool> complete = true;
    for(int t = 0; t < num_threads; ++t) {
        threads.emplace_back([&, t] {
            for(auto &x : work_thread[t]) {
                slist.insert(x, x);
                auto[is_in, value] = slist.search(x);
                if(!is_in || value != x) {
                    std::cout << "not found " << x << "\n";
                    complete = false;
                }
            }
        });
    }
    for(auto &t : threads) {
        t.join();
    }
    threads.clear();    
    slist.compute_indices();
    for(int t = 0; t < num_threads; ++t) {
        threads.emplace_back([&, t] {
            for(auto &x : work_thread[t]) {
                auto[ok1, val] = slist.element_at(x);
                auto[ok2, rank] = slist.rank(x);
                if(!ok1 || !ok2 || val != x || rank != x) {
                    if(!ok1) std::cout << "index wrong " << x << " " << val << "\n";
                    if(!ok2) std::cout << "rank wrong " << x << " " << rank << "\n";
                    complete = false;
                }
            }
        });
    }
    for(auto &t : threads) {
        t.join();
    }
    threads.clear();
    if(!complete) {
        slist.print();
    }
    return complete.load();
}

void test_index_par_skiplist(const double p, const int max_level, const int n, const int it, const int num_threads) {
    std::vector<int> v(n);
    std::iota(v.begin(), v.end(), 0);
    bool all_ok = true;
    for(int i = 0; i < it; ++i) {
        random_gen::shuffle<int>(v);
        bool ok = test_index_par_skiplist_with(p, max_level, num_threads, v);
        all_ok &= ok;
        std::cout << "test " << i + 1 << " -> " << ok << "\n";
    }
    std::cout << "all test -> " << all_ok << "\n";
}

template<class Slist>
class ParTest {
 public:
    ParTest(const double p, const int max_level, const int n, const int it, const int num_threads) 
    : _p(p), _max_level(max_level), _num_threads(num_threads), _n(n), _iterations(it) {}


    std::pair<bool, int> repeated_search(Slist &slist, int x) {
        int upper = 10;
        bool is_in = false;
        int value = 0;
        for(int i = 0; i < !is_in && i < upper; ++i) {
            std::tie(is_in, value) = slist.search(x);
        }
        return {is_in, value};
    }

    //all elements have same elements
    bool test_same_elements(std::vector<int> &v) {
        std::vector<std::thread> threads;
        std::atomic<bool> correct = true;
        Slist slist = Slist(_p, _max_level);
        for(int t = 0; t < _num_threads; ++t) {
            threads.emplace_back([&, t] {
                for(auto &x : v) {
                    slist.insert(x, x);
                }
            });
        }
        for(auto &t : threads) {
            t.join();
        }
        threads.clear();
        if(!slist.is_consistent()) correct = false;
        // slist.print();
        auto keys = slist.get_keys();
        if(keys.size() != v.size()) {
            correct = false;
            std::cout << "sizes mismatch " << keys.size() << " " << v.size() << "\n";
        }
        std::vector<int> w(v); //v is shuffled
        std::sort(w.begin(), w.end());
        for(uint i = 0; i < w.size(); ++i) {
            if(w[i] != keys[i]) {
                correct = false;
                std::cout << "mismatch " << i << " " << w[i] << " " << keys[i] << "\n";
            }
        }

        for(int t = 0; t < _num_threads; ++t) {
            threads.emplace_back([&, t] {
                for(auto &x : v) {
                    auto[is_in, value] = slist.search(x);
                    if(!is_in || value != x) correct = false;
                }
            });
        }
        for(auto &t : threads) {
            t.join();
        }
        threads.clear();

        for(int t = 0; t < _num_threads; ++t) {
            threads.emplace_back([&, t] {
                for(uint i = 0; i < v.size(); ++i) {
                    int x = v[i];
                    slist.remove(x);
                    auto[is_in, value] = slist.search(x);
                    if(is_in) correct = false;
                }
            });
        }
        for(auto &t : threads) {
            t.join();
        }
        threads.clear();
        return correct.load();
    }

    bool test_duplicates(std::vector<std::vector<int>> &to_insert) {
        std::vector<std::thread> threads;
        std::atomic<bool> correct = true;
        Slist slist = Slist(_p, _max_level);
        for(int t = 0; t < _num_threads; ++t) {
            threads.emplace_back([&, t] {
                //skiplist has some elements to exploit parallel access
                for(auto &x : to_insert[t]) {
                    slist.insert(x,x);
                }
                for(uint j = 0; j < to_insert[t].size() / 2; ++j) {
                    int x = to_insert[t][j];
                    int bound = 5;
                    //reinsert should only change value
                    for(int i = 0; i < bound; ++i) {
                        slist.insert(x, x);
                    }
                    auto [found, value] = slist.search(x);
                    if(!found || value != x) {
                        std::cout << found << " " << x << " " << value << " should be found \n";
                        correct.store(false);
                    }
                    bool remove_ok = slist.remove(x);
                    if(!remove_ok) {
                        std::cout << x << " remove failed \n";
                        correct.store(false);
                    }

                    for(int i = 0; i < bound; ++i) {
                        remove_ok = slist.remove(x);
                        if(remove_ok) {
                            std::cout << x << " remove should not succeed \n";
                            correct.store(false);
                        }
                    }
                }
                //removed
                for(uint j = 0; j < to_insert[t].size() / 2; ++j) {
                    int x = to_insert[t][j];
                    auto [found, value] = slist.search(x);
                    if(found) {
                        std::cout << found << " " << x << " " << value << " should not be found \n";
                        correct.store(false);
                    }
                }
                //not removed
                for(uint j = to_insert[t].size() / 2; j < to_insert[t].size(); ++j) {
                    int x = to_insert[t][j];
                    auto [found, value] = slist.search(x);
                    if(!found || value != x) {
                        std::cout << x << " " << value << " should be found 2 \n";
                        correct.store(false);
                    }
                }
            });
        }
        for(auto &t : threads) {
            t.join();
        }
        threads.clear();
        return correct.load();
    }

    bool test_mixed(std::vector<std::vector<int>> &to_insert, Slist &slist) {
        std::vector<std::thread> threads;
        std::atomic<bool> correct = true;
        for(int t = 0; t < _num_threads; ++t) {
            threads.emplace_back([&, t] {
                bool thread_correct = true;
                uint rate = 5;
                for(uint i = 0; i < to_insert[t].size(); ++i) {
                    int x = to_insert[t][i];
                    slist.insert(x, x);
                    auto[is_in, value] = slist.search(x);
                    thread_correct &= is_in && value == x;
                    if(i % rate == rate - 1) {
                        bool rem_ok = slist.remove(x);;
                        if(!rem_ok) std::cout << "removed failed " << x << "\n";
                        thread_correct &= rem_ok;
                        std::tie(is_in, value) = slist.search(x);
                        thread_correct &= !is_in;
                    }
                }
                correct.store(correct.load() & thread_correct);
            });
        }
        for(auto &t : threads) {
            t.join();
        }
        threads.clear();
        return correct.load();
    }

    bool test_insert(std::vector<std::vector<int>> &to_insert, Slist &slist) {
        std::vector<std::thread> threads;
        std::atomic<bool> complete = true;
        for(int t = 0; t < _num_threads; ++t) {
            threads.emplace_back([&, t] {
                for(auto &x : to_insert[t]) {
                    slist.insert(x, x);
                    auto[is_in, value] = slist.search(x);
                    if(!is_in || value != x) {
                        complete = false;
                    }
                }
            });
        }
        for(auto &t : threads) {
            t.join();
        }
        threads.clear();
        return complete.load();
    }

    bool test_search(std::vector<std::vector<int>> &to_insert, Slist &slist) {
        std::vector<std::thread> threads;
        std::atomic<bool> complete = true;
        for(int t = 0; t < _num_threads; ++t) {
            threads.emplace_back([&, t] {
                for(auto &x : to_insert[t]) {
                    auto[is_in, value] = slist.search(x);
                    if(!is_in || value != x) {
                        complete = false;
                    }
                }
            });
        }
        for(auto &t : threads) {
            t.join();
        }
        threads.clear();
        return complete.load();
    }

    bool test_remove(std::vector<std::vector<int>> &to_insert, Slist &slist) {
        std::atomic<bool> all_removed = true;
        std::vector<std::thread> threads;
        for(int t = 0; t < _num_threads; ++t) {
            threads.emplace_back([&, t] {
            for(uint i = 0; i < to_insert[t].size() / 2; ++i) {
                int x = to_insert[t][i];
                bool success = slist.remove(x);
                if(!success) {
                    all_removed = false;
                }
            }
            });
        }
        for(auto &t : threads) {
            t.join();
        }
        threads.clear();
        return all_removed.load();
    }

    bool test_remove_check(std::vector<std::vector<int>> &to_insert, Slist &slist) {
        std::atomic<bool> remove_check = true;
        std::vector<std::thread> threads;
        for(int t = 0; t < _num_threads; ++t) {
            threads.emplace_back([&, t] {
                uint i = 0;
                for(auto &x : to_insert[t]) {
                    auto[is_in, value] = slist.search(x);
                    if(i < to_insert[t].size() / 2) {
                        if(is_in) {
                            // std::cout << "should have been removed " << x << "\n";
                            remove_check.store(false);
                        }
                    }
                    else {
                        if(!is_in || value != x) {
                            // std::cout << "should not have been removed " << x << "\n";
                            remove_check.store(false);
                        }
                    }
                    i++;
                }
            });
        }
        for (auto& t : threads) {
            t.join();
        }
        threads.clear();
        return remove_check.load();
    }


    std::vector<std::vector<int>> distribute_work(std::vector<int> &v) {
        std::vector<std::vector<int>> to_insert(_num_threads);
        for(uint i = 0; i < v.size(); ++i) {
            to_insert[i % _num_threads].push_back(v[i]);
        }
        return to_insert;
    }

    bool test_par_skiplist_with(std::vector<int> &v) {
        Slist slist = Slist(_p, _max_level);
        Slist slist2 = Slist(_p, _max_level);
        auto to_insert = distribute_work(v);
        bool same = true;
        bool duplicates = true;
        bool mixed = true;
        bool insert = true;
        bool search = true;
        bool remove = true;
        bool remove_check = true;
        bool consistent = true;
        same &= test_same_elements(v);
        duplicates &= test_duplicates(to_insert);
        mixed &= test_mixed(to_insert, slist2);
        insert &= test_insert(to_insert, slist);
        consistent &= slist.is_consistent();
        search &= test_search(to_insert, slist);
        remove &= test_remove(to_insert, slist);
        remove_check &= test_remove_check(to_insert, slist);
        bool ok = same && duplicates && insert && search && consistent && remove && remove_check;
        std::cout << V(same) << V(duplicates) << V(mixed) << V(insert) << V(search) << V(consistent) << V(remove) << V(remove_check) << "-> " << ok  << "\n";
        return ok;
    }

    void test_par_skiplist() {
        std::vector<int> v(_n);
        std::iota(v.begin(), v.end(), 0);
        bool ok = true;
        auto t1 = std::chrono::high_resolution_clock::now();
        for(int i = 0; i < _iterations; ++i) {
            std::cout << "test " << i + 1 << ": ";
            ok &= test_par_skiplist_with(v);
            random_gen::shuffle<int>(v);
            // random_gen::sort_and_weak_shuffle<int>(v);
            
        }
        auto t2 = std::chrono::high_resolution_clock::now();
        auto time = std::chrono::duration_cast<std::chrono::microseconds>(t2 - t1).count() / 1000.;
        std::cout << "all tests -> " << ok << "\n";
        std::cout << "time: " << time << "\n";
    }

    const double _p;
    const int _max_level;
    const int _num_threads;
    const int _n;
    const int _iterations;
};


int main()
{
    const double p = 0.5;
    const int max_level = 32;
    const int n = 100000;
    const int it = 5;
    const int num_threads = 6;

    //sequential
    ParTest<SeqSkipList<int,int>> tester0(p, max_level, n, it, 1); //one level at the time -> works

    //lock based
    ParTest<LockSkipList<int,int>> tester1(p, max_level, n, it, num_threads); //one level at the time -> works
    ParTest<LockSkipList2<int,int>> tester11(p, max_level, n, it, num_threads); //one level at the time with shared ptr -> works

    //lock free
    ParTest<LockFreeSkipList<int,int>> tester2(p, max_level, n, it, num_threads); // -> works
    
    //indeaxable
    ParTest<IndexableLockSkipList<int,int>> tester3(p, max_level, n, it, num_threads);

    test_index_seq_skiplist(p,  max_level, n, it);

    tester0.test_par_skiplist();
    tester1.test_par_skiplist();
    tester11.test_par_skiplist();
    tester2.test_par_skiplist();
    tester3.test_par_skiplist();
    test_index_par_skiplist(p, max_level, n, it, num_threads);

    return 0;
}
