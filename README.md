# Threadsafe Skiplist
This is my final project on [Threadsafe Skiplists](https://en.wikipedia.org/wiki/Skip_list) of the ["Efficient Parallel C++ 2021/2022"](https://ae.iti.kit.edu/4202.php) lab offered by the ITI Algorithm Engineering chair at the Karlsruher Institute of Technology (KIT).
The Skiplist is a probabilistic data structure that similar to red-black-trees allow dynamic insertion, deletion and search in expected $O(\log n)$ complexity.
However, there are much simpler to implement, since they do not need complex tree rotations and take advantage of randomization to probabilistically guarantee balance.

In addition, by keeping track of the length of the "skip-pointers", Skiplists can be made indexable.
This allows to efficiently search for the median or more generally the $k$-th element and to determine the rank of an element in sorted order.

Skiplists can be made concurrent, allowing parallel thread-safe access to the data structure.
There is a [lock-variant](https://link.springer.com/content/pdf/10.1007/978-3-540-72951-8_11.pdf) using a lazy-approach to update the Skiplist and a [lock-free variant](https://patentimages.storage.googleapis.com/3e/80/e8/7a7860dd318f16/US20100042584A1.pdf).
This repository contains implementations of a sequential (indexable) Skiplist as well as both variants of the thread-safe Skiplist. 

![plot1](/images/skiplist.png)


# Resources
[W. Pugh. Skip lists: a probabilistic alternative to balanced trees. ACM Transactions on Database Systems, 33(6):668–676, 1990.](https://15721.courses.cs.cmu.edu/spring2018/papers/08-oltpindexes1/pugh-skiplists-cacm1990.pdf)

[Y. Lev, M. Herlihy, V. Luchangco, and N. Shavit. A Simple Optimistic Skiplist Algorithm. Fourteenth Colloquium on structural information and
communication complexity (SIROCCO) 2007 pp. 124–138, June 5–8, 2007, Castiglioncello (LI), Italy.](https://link.springer.com/content/pdf/10.1007/978-3-540-72951-8_11.pdf)

[Herlihy, Y. Lev, and N. Shavit. A lock-free concurrent skiplist with wait-free search. Unpublished Manuscript, Sun Microsystems Laboratories,
Burlington, Massachusetts, 2007.](https://patentimages.storage.googleapis.com/3e/80/e8/7a7860dd318f16/US20100042584A1.pdf)



# How to build the project
```
mkdir release
cd release
cmake -DCMAKE_BUILD_TYPE=Release ..
make
```

# Run Tests
```
./release/test
```
# Run Benchmark
```
./release/time
```
