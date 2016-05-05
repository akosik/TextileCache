# Benchmarking a Multithreaded Cache
##### Alec Kosik and Alex Pan


###Getting started
####How to clone this repository:
`git clone --recursive https://github.com/akosik/TextileCache.git`

Leaving out the recursive flag will make git ignore the submodule jsmn and stunt client compilation.

####How to run things:
Compiled and tested on Linux (done by adding -gnu=c99 flag to compile on Linux)
Make file will work on Linux, no need for any changes if testing on either operating system
* `./server` will run server side.
* `python distribution.py x | ./set_client x` will run a testing client for set requests.
* `python distribution.py x | ./get_client x` will run a testing client for get requests.
* `python distribution.py x | ./delete_client x` will run a testing client for delete requests.

Replace `x` with the number of requests the client should make.
Note: You will need to have python's SciPy library installed for distribution.py to work.


###Improvements on our cache design
The primary change we made was to multithread our cache. There are locks for general cache information: capacity, number of keys in cache, and amount of memory used. And there are locks for each individual entry in the cache. We use `pthread_rwlock_t` for finer control on when we actually need to give a thread exclusive access to a resource versus when multiple threads can be looking up a resouce.  


###Experimental Design:
Our experimental design and setup was essentially the same as when benchmarking the unthreaded cache. The only difference was that we did not use `gyoza` since it was having some strange behavior. Our server was run from `mandu` and we simultaneously ran clients from the other four machines for 25,000 iterations each.


###Results
####Profiling
We ended up being unable to profile our code using `gprof` on the linux machines in polytopia. To use gprof, we had to add the `-pg` flag when compiling our code, which caused an `EINTR` error code for `Interrupted system call`. Compiling without the `-pg` flags gives us no errors.
I attempted to get around this by adding a `continue` statement, which allowed our code to run properly, but when looking at gprof's actual output, there were entries that didn't make any sense. For example, it recorded 92% `work`, 4% `cache_set`, and 4% `establish_udp_client` when I tried testing the `set_client` (everything else was marked as 0% ie negligable). The first two make sense, since every thread has the infinite while-loop and we expect `cache_set` to take a significant portion of the time (but not as much as an infinite while loop!). However, since gprof was run only on the server, the fact that we are seeing calls to `establish_udp_client` make no sense. We were unable to come up with a fix for this, so with our current experimental setup (ie using the linux machines in polytopia) we are unable to profile our cache.

####Threaded vs Unthreaded Speeds:
##### Table of results from unthreaded cache: 
| SERVICE | GET | SET | DELETE | GET* |
| --- | --- | --- | --- | --- |
| Average response time (msec) | 0.283 | 0.685 | 0.5486 | 0.438 |
| Total Load (requests/sec) | 17700 | 7300 | 9110 | 11400 |
| Failure rate (%) | 4.96 | N/A | N/A | 8.41 |
(All values are 3 sig. figs.)

##### Table of results from threaded cache: 
| SERVICE | GET | SET | DELETE |
| --- | --- | --- | --- |
| Average response time (msec) | 0.215 | 0.653 | 0.512 |
| Total Load (requests/sec) | 18500 | 6140 | 7740 |
| Failure rate (%) | 3.45 | N/A | N/A |
(All values are 3 sig. figs.)

##### Interpretation:
Comparing unthreaded `GET*` and threaded `GET`, we see the speed is 51.9% of what it was before, and the load is actually 4.5% higher while the failure rate is 30.4% lower. We suspect that the threaded `GET` is actually even faster, since some clients might finish setting and start their timing cycles before other clients have finished setting values into the cache.

The threaded `SET` is 4.7% faster, but with a load factor that is 15.9% smaller. Similarly, threaded `DELETE` is 6.7% faster, but has a load factor that is 17.7% smaller. The smaller magnitude of improvements in speed was probably due to the number of resources that the cache had to lock during the `SET` and `DELETE` operations because of cache-resizing. Each of these operations had to use locks for capacity, number of keys in cache, and amount of memory used. As an improvement, we would probably remove cache-resizing and instead, just initalize the cache to be at maxmem. This would eliminate the number of locks we need and significantly improve the speed of our cache.

It is also possible that the number of requests we are using affected our results. For these benchmarks we ran everything at 25,000 iterations. We would expect to see a higher average response time for a higher number of requests, since that would lead to an increasingly large backlog of requests. For `GET` we originally ran our tests at 100,000 iterations, so a portion of the speedup could come from the smaller number of iterations. Conversely, for `SET` and `DELETE`, we ran 2.5x the number of iterations, since originally both results for the unthreaded cache were taken from 10,000 requests. This increase in the number of iterations could have contributed to a smaller speedup than what we would have liked to see. 

Like last time, we were unable to find maximum load while running on the polytopia machines.
