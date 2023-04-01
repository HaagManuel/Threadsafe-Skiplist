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

#include <parallel/algorithm>

#include "implementation/random_generator.hpp"
#include "implementation/seq_skiplist.hpp"
#include "implementation/indexable_seq_skiplist.hpp"


#include "implementation/lock_skiplist.hpp"
#include "implementation/lock_skiplist2.hpp"

#include "implementation/lockfree_skiplist.hpp"

#include "implementation/indexable_lock_skiplist.hpp"

#define V(x) std::string(#x "=") << (x) << " "


using int_type = int;

/* shuffling */
namespace shuffling {
    struct Permutation {
        void shuffle(std::vector<int_type> &v) { random_gen::shuffle<int_type>(v);}
    };

    struct WeakShuffle {
        void shuffle(std::vector<int_type> &v) { random_gen::sort_and_weak_shuffle<int>(v);}
    };
}
/* shuffling */

std::vector<std::vector<int_type>> distribute_work(std::vector<int> &v, int _num_threads) {
    std::vector<std::vector<int_type>> to_insert(_num_threads);
    for(uint i = 0; i < v.size(); ++i) {
        to_insert[i % _num_threads].push_back(v[i]);
    }
    return to_insert;
}

/* benchmark */
namespace benchmark {
    template<class Slist, class ShuffleType, class Benchmark> 
    class Runner {
     public:
        Runner(const double p, const int max_level, const int n, const int it, const int num_threads) 
        : _p(p), _max_level(max_level), _num_threads(num_threads), _n(n), _iterations(it) {}
        
        std::vector<double> run() {
            ShuffleType shf;
            Benchmark benchmark(_num_threads);
            std::vector<int_type> v(_n);
            std::iota(v.begin(), v.end(), 0);
            std::vector<double> times;
            for(int i = 0; i < _iterations; ++i) {
                shf.shuffle(v);
                Slist slist(_p, _max_level);
                double time = benchmark.run_with(slist, v);
                times.push_back(time);
            }
            return times;
        }

        std::vector<std::tuple<double, size_t, size_t, size_t>> run_special_metric() {
            ShuffleType shf;
            Benchmark benchmark(_num_threads);
            std::vector<int_type> v(_n);
            std::iota(v.begin(), v.end(), 0);
            std::vector<std::tuple<double, size_t, size_t, size_t>> all_data;
            for(int i = 0; i < _iterations; ++i) {
                shf.shuffle(v);
                Slist slist(_p, _max_level);
                slist.init_counter();
                auto data = benchmark.run_with_metric(slist, v);
                all_data.push_back(data);
            }
            return all_data;
        }
    private:
        const double _p;
        const int _max_level;
        const int _num_threads;
        const int _n;
        const int _iterations;
    };
    
    //threads do not share elements
    template<class Slist>
    struct BenchmarkDisjoint {
        BenchmarkDisjoint(int num_threads) : _num_threads(num_threads) {};

        double run_with(Slist &slist, std::vector<int_type> &v) {
            auto work_threads = distribute_work(v, _num_threads);
            std::vector<std::thread> threads;
            std::atomic<bool> complete = true;
            auto t1 = std::chrono::high_resolution_clock::now();
            //1:1:3
            for(int t = 0; t < _num_threads; ++t) {
                threads.emplace_back([&, t] {
                    for(auto &x : work_threads[t]) {
                        slist.insert(x, x);
                        slist.search(x);
                    }
                    for(auto &x : work_threads[t]) {
                        slist.search(x);
                    }
                    for(auto &x : work_threads[t]) {
                        slist.search(x);
                        slist.remove(x);
                    }
                });
            }
            for(auto &t : threads) {
                t.join();
            }
            threads.clear();
            auto t2 = std::chrono::high_resolution_clock::now();
            auto time = std::chrono::duration_cast<std::chrono::microseconds>(t2 - t1).count() / 1000.;
            return time;
        }

        std::tuple<double, size_t, size_t, size_t> run_with_metric(Slist &slist, std::vector<int_type> &v) {
            auto work_threads = distribute_work(v, _num_threads);
            std::vector<std::thread> threads;
            std::atomic<bool> complete = true;
            std::atomic<size_t> total_op{0};
            auto t1 = std::chrono::high_resolution_clock::now();
            //1:1:3
            for(int t = 0; t < _num_threads; ++t) {
                threads.emplace_back([&, t] {
                    for(auto &x : work_threads[t]) {
                        slist.insert(x, x);
                        slist.search(x);
                    }
                    for(auto &x : work_threads[t]) {
                        slist.search(x);
                    }
                    for(auto &x : work_threads[t]) {
                        slist.search(x);
                        slist.remove(x);
                    }
                    total_op += 5 * work_threads[t].size();
                });
            }
            for(auto &t : threads) {
                t.join();
            }
            threads.clear();
            auto t2 = std::chrono::high_resolution_clock::now();
            auto time = std::chrono::duration_cast<std::chrono::microseconds>(t2 - t1).count() / 1000.;
            auto [find, find_retry] = slist.collect_counter();
            return {time, find, find_retry, total_op.load()};
        }
        int _num_threads;
    };

    //threads have same elements
    template<class Slist>
    struct BenchmarkShared {
        BenchmarkShared(int num_threads) : _num_threads(num_threads) {};

        double run_with(Slist &slist, std::vector<int_type> &v) {
            std::vector<std::thread> threads;
            auto t1 = std::chrono::high_resolution_clock::now();
            for(int t = 0; t < _num_threads; ++t) {
                threads.emplace_back([&, t] {
                    for(auto &x : v) {
                        slist.insert(x, x);
                        slist.search(x);
                    }
                    for(auto &x : v) {
                        slist.search(x);
                    }
                    for(auto &x : v) {
                        slist.search(x);
                        slist.remove(x);
                    }
                });
            }
            for(auto &t : threads) {
                t.join();
            }
            threads.clear();
            auto t2 = std::chrono::high_resolution_clock::now();
            auto time = std::chrono::duration_cast<std::chrono::microseconds>(t2 - t1).count() / 1000.;
            return time;
        }

        std::tuple<double, size_t, size_t, size_t> run_with_metric(Slist &slist, std::vector<int_type> &v) {
            std::vector<std::thread> threads;
            std::atomic<size_t> total_op{0};
            auto t1 = std::chrono::high_resolution_clock::now();
            //1:1:3
            for(int t = 0; t < _num_threads; ++t) {
                threads.emplace_back([&, t] {
                    for(auto &x : v) {
                        slist.insert(x, x);
                        slist.search(x);
                    }
                    for(auto &x : v) {
                        slist.search(x);
                    }
                    for(auto &x : v) {
                        slist.search(x);
                        slist.remove(x);
                    }
                    total_op += 5 * v.size();
                });
            }
            for(auto &t : threads) {
                t.join();
            }
            threads.clear();
            auto t2 = std::chrono::high_resolution_clock::now();
            auto time = std::chrono::duration_cast<std::chrono::microseconds>(t2 - t1).count() / 1000.;
            auto [find, find_retry] = slist.collect_counter();
            return {time, find, find_retry, total_op.load()};
        }

        int _num_threads;
    };
}
/* benchmark */

/* printer */
namespace printer {
    template <class T>
    void print(std::ostream& out, const T& t, size_t w) {
        out.width(w);
        out       << t << " " << std::flush;
        std::cout.width(w);
        std::cout << t << " " << std::flush;
    }

    void print_headline(std::ostream& out) {
        print(out, "#it"    , 15);
        print(out, "threads", 15);
        print(out, "n", 15);
        print(out, "p", 15);
        print(out, "max_level", 15);
        print(out, "sorting", 15);
        print(out, "benchmark", 15);
        print(out, "variant", 15);
        print(out, "time", 15);
        out       << std::endl;
        std::cout << std::endl;
    }

    void print_timing(std::ostream& out,
                      int it, int t, int n, double p, int lv, std::string &srt, std::string &bench, std::string &var, double time) {
        print(out, it  , 15);
        print(out, t, 15);
        print(out, n, 15);
        print(out, p, 15);
        print(out, lv, 15);
        print(out, srt, 15);
        print(out, bench, 15);
        print(out, var, 15);
        print(out, time, 15);
        out       << std::endl;
        std::cout << std::endl;
    }

    template<class RunnerType> 
    void run_benchmark(std::ofstream &file,
     int it, std::vector<int> &ts, std::vector<double> &ps, std::vector<int> &max_levels, std::vector<int> &ns,
     std::string &sort, std::string &benchmark, std::string &variant) {
        for(auto &t : ts) {
            for(auto &p : ps) {
                for(auto &max_lv : max_levels) {
                    for(auto &n : ns) {
                        RunnerType runner(p, max_lv, n, it, t);
                        auto times = runner.run();
                        int it_nr = 1;
                        for(auto &time : times) {
                            print_timing(file, it_nr, t, n, p, max_lv, sort, benchmark, variant, time);
                            it_nr++;
                        }
                    }
                }
            }
        }
    }

    void print_headline2(std::ostream& out) {
        print(out, "#it"    , 12);
        print(out, "threads", 12);
        print(out, "n", 12);
        print(out, "p", 12);
        print(out, "max_level", 12);
        print(out, "sorting", 12);
        print(out, "benchmark", 12);
        print(out, "variant", 12);
        print(out, "time", 12);
        print(out, "num_find", 12);
        print(out, "num_find_retry", 12);
        print(out, "total_op", 12);
        out       << std::endl;
        std::cout << std::endl;
    }

    void print_timing2(std::ostream& out,
                      int it, int t, int n, double p, int lv, std::string &srt, std::string &bench, std::string &var, double time,
                       size_t nf, size_t nfr, size_t op) {
        print(out, it  , 12);
        print(out, t, 12);
        print(out, n, 12);
        print(out, p, 12);
        print(out, lv, 12);
        print(out, srt, 12);
        print(out, bench, 12);
        print(out, var, 12);
        print(out, time, 12);
        print(out, nf, 12);
        print(out, nfr, 12);
        print(out, op, 12);
        out       << std::endl;
        std::cout << std::endl;
    }

    template<class RunnerType> 
    void run_special_metric(std::ofstream &file,
     int it, std::vector<int> &ts, std::vector<double> &ps, std::vector<int> &max_levels, std::vector<int> &ns,
     std::string &sort, std::string &benchmark, std::string &variant) {
        for(auto &t : ts) {
            for(auto &p : ps) {
                for(auto &max_lv : max_levels) {
                    for(auto &n : ns) {
                        RunnerType runner(p, max_lv, n, it, t);
                        auto data = runner.run_special_metric();
                        int it_nr = 1;
                        for(auto &[time, finds, find_try, op] : data) {
                            print_timing2(file, it_nr, t, n, p, max_lv, sort, benchmark, variant, time, finds, find_try, op);
                            it_nr++;
                        }
                    }
                }
            }
        }
    }

    void print_headline3(std::ostream& out) {
        print(out, "#it"    , 12);
        print(out, "sec"    , 12);
        print(out, "threads", 12);
        print(out, "n", 12);
        print(out, "p", 12);
        print(out, "max_level", 12);
        print(out, "sorting", 12);
        print(out, "benchmark", 12);
        print(out, "variant", 12);
        print(out, "time", 12);
        out       << std::endl;
        std::cout << std::endl;
    }

    void print_timing3(std::ostream& out,
                      int it, int sec, int t, int n, double p, int lv, std::string &srt, std::string &bench, std::string &var, double time) {
        print(out, it  , 12);
        print(out, sec  , 12);
        print(out, t, 12);
        print(out, n, 12);
        print(out, p, 12);
        print(out, lv, 12);
        print(out, srt, 12);
        print(out, bench, 12);
        print(out, var, 12);
        print(out, time, 12);
        out       << std::endl;
        std::cout << std::endl;
    }

    template<class ShuffleType> 
    void run_index_benchmark(std::ofstream &file,
     int it, int n, int sections, std::vector<int> &ts, std::string &sort) {
        std::string benchmark = "disjoint";
        std::string variant = "indexable_slist";
        double p = 0.5;
        int max_level = 32;
        
        ShuffleType shf;
        std::atomic<size_t> errors = 0;
        for(auto &_num_threads : ts) {
            std::vector<int_type> v(n);
            std::iota(v.begin(), v.end(), 0);
            for(int i = 1; i <= it; ++i) {
                IndexableLockSkipList<int,int> slist(p, max_level);
                shf.shuffle(v);
                auto work_threads = distribute_work(v, _num_threads);
                std::vector<std::thread> threads;
                double total_time = 0;

                for(uint s = 1; s <= sections; ++s) {
                    auto t1 = std::chrono::high_resolution_clock::now();
                    for(uint t = 0; t < _num_threads; ++t) {
                        threads.emplace_back([&, t] {
                            uint size = work_threads[t].size();
                            uint batch = size / sections;
                            for(uint j = (s - 1) * batch; j < s * batch; ++j) {
                                int x = work_threads[t][j];
                                slist.insert(x, x);
                            }
                        });
                    }
                    for(auto &t : threads) {
                        t.join();
                    }
                    threads.clear();

                    slist.compute_indices();

                    for(int t = 0; t < _num_threads; ++t) {
                        threads.emplace_back([&, t] {
                            uint size = work_threads[t].size();
                            uint batch = size / sections;
                            for(uint j = (s - 1) * batch; j < s * batch; ++j) {
                                int x = work_threads[t][j];
                                auto[ok, rank] = slist.rank(x); //or element_at
                                if(!ok) errors++;
                            }
                        });
                    }
                    for(auto &t : threads) {
                        t.join();
                    }
                    threads.clear();
                    auto t2 = std::chrono::high_resolution_clock::now();
                    auto time = std::chrono::duration_cast<std::chrono::microseconds>(t2 - t1).count() / 1000.;
                    // print_timing3(file, i, s, _num_threads, n, p, max_level, sort, benchmark, variant, time);
                    total_time += time;
                    if(errors > 0) std::cout << "errors " << errors << "\n";
                }
                print_timing3(file, i, sections, _num_threads, n, p, max_level, sort, benchmark, variant, total_time);
            }
        }
    }

    struct SeqSorter {
        void sort(std::vector<int_type> &v) {
            std::sort(v.begin(), v.end());
        }
    };
    struct ParSorter {
        void sort(std::vector<int_type> &v) {
            __gnu_parallel::sort(v.begin(), v.end());
        }
    };

    template<class ShuffleType, class Sorter> 
    void run_vector_benchmark(std::ofstream &file,
     int it, int n, int sections, std::vector<int> &ts, std::string &sort, std::string &variant) {
        std::string benchmark = "disjoint";
        double p = 0.0;
        int max_level = 0;
        // std::vector<int_type> vec;
        // vec.reserve(n);
        ShuffleType shf;
        Sorter sorter;
        std::atomic<size_t> errors = 0;
        std::vector<int_type> v(n);
        std::iota(v.begin(), v.end(), 0);
        double total_time = 0;
        for(auto &_num_threads : ts) {
            // for(int i = 1; i <= it; ++i) {
            for(int i = it; i <= it; ++i) {
                // vec.clear();
                std::vector<int_type> vec;
                shf.shuffle(v);
                auto work_threads = distribute_work(v, _num_threads);
                std::vector<std::thread> threads;
                for(int s = 1; s <= sections; ++s) {
                    auto t1 = std::chrono::high_resolution_clock::now();
                    //insert sequentially
                    uint size = v.size();
                    uint batch = size / sections;
                    for(uint j = (s - 1) * batch; j < s * batch; ++j) {
                        int x = v[j];
                        vec.push_back(x);
                    }
                    sorter.sort(vec);

                    for(int t = 0; t < _num_threads; ++t) {
                        threads.emplace_back([&, t] {
                            uint size = work_threads[t].size();
                            uint batch = size / sections;
                            for(uint j = (s - 1) * batch; j < s * batch; ++j) {
                                int x = work_threads[t][j];
                                auto it = std::lower_bound(vec.begin(), vec.end(), x);
                                if(x != *it) errors++; //could compute rank by it -  vec.begin()
                            }
                        });
                    }
                    for(auto &t : threads) {
                        t.join();
                    }
                    threads.clear();
                    auto t2 = std::chrono::high_resolution_clock::now();
                    auto time = std::chrono::duration_cast<std::chrono::microseconds>(t2 - t1).count() / 1000.;
                    total_time += time;
                    // print_timing3(file, i, s, _num_threads, n, p, max_level, sort, benchmark, variant, time);
                }
                print_timing3(file, i, sections, _num_threads, n, p, max_level, sort, benchmark, variant, total_time);
            }
        }
    }



}
/* printer */

int main()
{
    // uncomment to run benmark

    // using SlistLock = LockSkipList<int_type, int_type>;
    // using SlistLock2 = LockFreeSkipList<int_type, int_type>;
    // using SlistLock3 = LockSkipList2<int_type, int_type>; //with shared ptr
    // using SlistLock4 = SeqSkipList<int_type, int_type>;

    // using Runner1 = benchmark::Runner<SlistLock, shuffling::Permutation, benchmark::BenchmarkDisjoint<SlistLock>>;
    // using Runner2 = benchmark::Runner<SlistLock, shuffling::WeakShuffle, benchmark::BenchmarkDisjoint<SlistLock>>;
    // using Runner3 = benchmark::Runner<SlistLock, shuffling::Permutation, benchmark::BenchmarkShared<SlistLock>>;
    // using Runner4 = benchmark::Runner<SlistLock, shuffling::WeakShuffle, benchmark::BenchmarkShared<SlistLock>>;

    // using Runner5 = benchmark::Runner<SlistLock2, shuffling::Permutation, benchmark::BenchmarkDisjoint<SlistLock2>>;
    // using Runner6 = benchmark::Runner<SlistLock2, shuffling::WeakShuffle, benchmark::BenchmarkDisjoint<SlistLock2>>;
    // using Runner7 = benchmark::Runner<SlistLock2, shuffling::Permutation, benchmark::BenchmarkShared<SlistLock2>>;
    // using Runner8 = benchmark::Runner<SlistLock2, shuffling::WeakShuffle, benchmark::BenchmarkShared<SlistLock2>>;

    // using Runner9  = benchmark::Runner<SlistLock3, shuffling::Permutation, benchmark::BenchmarkDisjoint<SlistLock3>>;
    // using Runner10 = benchmark::Runner<SlistLock3, shuffling::WeakShuffle, benchmark::BenchmarkDisjoint<SlistLock3>>;
    // using Runner11 = benchmark::Runner<SlistLock3, shuffling::Permutation, benchmark::BenchmarkShared<SlistLock3>>;
    // using Runner12 = benchmark::Runner<SlistLock3, shuffling::WeakShuffle, benchmark::BenchmarkShared<SlistLock3>>;

    // using Runner13 = benchmark::Runner<SlistLock4, shuffling::Permutation, benchmark::BenchmarkDisjoint<SlistLock4>>;
    // using Runner14 = benchmark::Runner<SlistLock4, shuffling::WeakShuffle, benchmark::BenchmarkDisjoint<SlistLock4>>;
    // using Runner15 = benchmark::Runner<SlistLock4, shuffling::Permutation, benchmark::BenchmarkShared<SlistLock4>>;
    // using Runner16 = benchmark::Runner<SlistLock4, shuffling::WeakShuffle, benchmark::BenchmarkShared<SlistLock4>>;


    std::string permuation = "permutation";
    std::string weak_shuffle = "weak_shuffle";

    std::string shared = "shared";
    std::string disjoint = "disjoint";
    
    std::string lock = "lock";
    std::string lockless = "lockless";
    std::string lock_shared_ptr = "lock_shared_ptr";
    std::string sequential = "sequential";

    std::vector<int> all_threads = {1,2,3,4,5,6,7,8,9,10,11,12}; 
    std::vector<int> half_threads = {1,2,3,4,5,6}; 
    std::vector<int> one_thread = {1}; 
    std::vector<int> default_thread = {6}; 
    std::vector<int> max_thread = {12}; 

    std::vector<int> default_max_lv = {32}; 

    std::vector<double> default_p = {0.5}; 
    

    int it;
    std::vector<double> ps;
    std::vector<int> max_levels, ns, ts;
    std::string filename;
    std::ofstream file;

    /* vary p */
    // filename = "vary_p.txt";
    // file = std::ofstream(filename);
    // printer::print_headline(file);
    // it = 5;
    // ps.clear();
    // for(double d = 0.05; d <= 0.80; d += 0.025) ps.push_back(d);
    // max_levels = default_max_lv;
    // ns = {1000000};
    // ts = default_thread;
    /* vary p */

    /* scaling */
    // filename = "scaling.txt";
    // file = std::ofstream(filename);
    // printer::print_headline(file);
    // it = 5;
    // ps = default_p;
    // max_levels = default_max_lv; 
    // ns = {100000, 1000000};
    // ts = all_threads;
    // ts = {9,10,11,12};
    /* scaling */

    /* vary n */
    // filename = "vary_n.txt";
    // file = std::ofstream(filename);
    // printer::print_headline(file);
    // it = 5;
    // ps = default_p;
    // max_levels = default_max_lv; 
    // ns.clear();
    // for(int i = 2; i <= 1 << 22; i *= 2) ns.push_back(i);
    // ts = {6,12};
    /* vary n */

    // printer::run_benchmark<Runner1>(file, it, ts, ps, max_levels, ns, permuation, disjoint, lock);
    // printer::run_benchmark<Runner2>(file, it, ts, ps, max_levels, ns, weak_shuffle, disjoint, lock);
    // printer::run_benchmark<Runner3>(file, it, ts, ps, max_levels, ns, permuation, shared, lock);
    // printer::run_benchmark<Runner4>(file, it, ts, ps, max_levels, ns, weak_shuffle, shared, lock);

    // printer::run_benchmark<Runner5>(file, it, ts, ps, max_levels, ns, permuation, disjoint, lockless);
    // printer::run_benchmark<Runner6>(file, it, ts, ps, max_levels, ns, weak_shuffle, disjoint, lockless);
    // printer::run_benchmark<Runner7>(file, it, ts, ps, max_levels, ns, permuation, shared, lockless);
    // printer::run_benchmark<Runner8>(file, it, ts, ps, max_levels, ns, weak_shuffle, shared, lockless);

    // printer::run_benchmark<Runner9>(file, it, ts, ps, max_levels, ns, permuation, disjoint, lock_shared_ptr);
    // printer::run_benchmark<Runner10>(file, it, ts, ps, max_levels, ns, weak_shuffle, disjoint, lock_shared_ptr);
    // printer::run_benchmark<Runner11>(file, it, ts, ps, max_levels, ns, permuation, shared, lock_shared_ptr);
    // printer::run_benchmark<Runner12>(file, it, ts, ps, max_levels, ns, weak_shuffle, shared, lock_shared_ptr);

    // printer::run_benchmark<Runner13>(file, it, one_thread, ps, max_levels, ns, permuation, disjoint, sequential);
    // printer::run_benchmark<Runner14>(file, it, one_thread, ps, max_levels, ns, weak_shuffle, disjoint, sequential);
    // printer::run_benchmark<Runner15>(file, it, one_thread, ps, max_levels, ns, permuation, shared, sequential);
    // printer::run_benchmark<Runner16>(file, it, one_thread, ps, max_levels, ns, weak_shuffle, shared, sequential);


    /* special metric */
    // using SSlistLock = LockSkipList<int_type, int_type, true>;
    // using SSlistLock2 = LockFreeSkipList<int_type, int_type, true>;

    // using SRunner1 = benchmark::Runner<SSlistLock, shuffling::Permutation, benchmark::BenchmarkDisjoint<SSlistLock>>;
    // using SRunner2 = benchmark::Runner<SSlistLock, shuffling::WeakShuffle, benchmark::BenchmarkDisjoint<SSlistLock>>;
    // using SRunner3 = benchmark::Runner<SSlistLock, shuffling::Permutation, benchmark::BenchmarkShared<SSlistLock>>;
    // using SRunner4 = benchmark::Runner<SSlistLock, shuffling::WeakShuffle, benchmark::BenchmarkShared<SSlistLock>>;

    // using SRunner5 = benchmark::Runner<SSlistLock2, shuffling::Permutation, benchmark::BenchmarkDisjoint<SSlistLock2>>;
    // using SRunner6 = benchmark::Runner<SSlistLock2, shuffling::WeakShuffle, benchmark::BenchmarkDisjoint<SSlistLock2>>;
    // using SRunner7 = benchmark::Runner<SSlistLock2, shuffling::Permutation, benchmark::BenchmarkShared<SSlistLock2>>;
    // using SRunner8 = benchmark::Runner<SSlistLock2, shuffling::WeakShuffle, benchmark::BenchmarkShared<SSlistLock2>>;

    /* counter */
    // filename = "counter.txt";
    // file = std::ofstream(filename);
    // printer::print_headline2(file);
    // it = 5;
    // ps = default_p;
    // max_levels = default_max_lv; 
    // ns = {1000000};
    // ts = {6,12};
    /* counter */
    // printer::run_special_metric<SRunner1>(file, it, ts, ps, max_levels, ns, permuation, disjoint, lock);
    // printer::run_special_metric<SRunner2>(file, it, ts, ps, max_levels, ns, weak_shuffle, disjoint, lock);
    // printer::run_special_metric<SRunner3>(file, it, ts, ps, max_levels, ns, permuation, shared, lock);
    // printer::run_special_metric<SRunner4>(file, it, ts, ps, max_levels, ns, weak_shuffle, shared, lock);

    // printer::run_special_metric<SRunner5>(file, it, ts, ps, max_levels, ns, permuation, disjoint, lockless);
    // printer::run_special_metric<SRunner6>(file, it, ts, ps, max_levels, ns, weak_shuffle, disjoint, lockless);
    // printer::run_special_metric<SRunner7>(file, it, ts, ps, max_levels, ns, permuation, shared, lockless);
    // printer::run_special_metric<SRunner8>(file, it, ts, ps, max_levels, ns, weak_shuffle, shared, lockless);
    /* special metric */
    

    /* rank */
    std::string seq_vec = "vector_seq";
    std::string par_vec = "vector_par";
    filename = "rank.txt";
    file = std::ofstream(filename);
    printer::print_headline3(file);
    it = 5;
    int n1 = 100000; 
    ts = all_threads;
    std::vector<uint> sec = {1, 10, 100, 1000};

    //1 thread, 6 threads, 12 threads

    //after that with fixed thread and varying section
    for(auto &sections : sec) {
        // printer::run_index_benchmark<shuffling::Permutation>(file, it, n1, sections, ts, permuation);
        // printer::run_index_benchmark<shuffling::WeakShuffle>(file, it, n1, sections, ts, weak_shuffle);

        for(int i = 1; i <= it; ++i) {
            printer::run_vector_benchmark<shuffling::Permutation, printer::SeqSorter>(file, i, n1, sections, ts, permuation, seq_vec);
        }
        for(int i = 1; i <= it; ++i) {
            printer::run_vector_benchmark<shuffling::WeakShuffle, printer::SeqSorter>(file, i, n1, sections, ts, weak_shuffle, seq_vec);
        }
        for(int i = 1; i <= it; ++i) {
            printer::run_vector_benchmark<shuffling::Permutation, printer::ParSorter>(file, i, n1, sections, ts, permuation, par_vec);
        }
        for(int i = 1; i <= it; ++i) {
            printer::run_vector_benchmark<shuffling::WeakShuffle, printer::ParSorter>(file, i, n1, sections, ts, weak_shuffle, par_vec);
        }
        // printer::run_vector_benchmark<shuffling::Permutation, printer::SeqSorter>(file, it, n1, sections, ts, permuation, seq_vec);
        // printer::run_vector_benchmark<shuffling::WeakShuffle, printer::SeqSorter>(file, it, n1, sections, ts, weak_shuffle, seq_vec);
        // printer::run_vector_benchmark<shuffling::Permutation, printer::ParSorter>(file, it, n1, sections, ts, permuation, par_vec);
        // printer::run_vector_benchmark<shuffling::WeakShuffle, printer::ParSorter>(file, it, n1, sections, ts, weak_shuffle, par_vec);
    }
    /* rank */
    return 0;
}
