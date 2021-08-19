
# A Cache for convolutional neural network accelerators
This repo contains a modified version of the dCache tool in Intel Pin version 3.11: https://software.intel.com/sites/landingpage/pintool/docs/97998/Pin/html/
The results discussed in this readme involve the implications of different cache configurations on gemm based cnn accelerator access patterns. The tool offers several knobs controllable through scripts or runnable with predefined configurations in darknet/

## Usage
### To use this tool independently
1) Download pin >= v3.11 from https://software.intel.com/content/www/us/en/develop/articles/pin-a-binary-instrumentation-tool-downloads.html
2) copy cache.H and dcache.cpp to pintools/source/tools/Memory/ 
3) run make build
4) modify make run to target required binary
5) analysis output will be printed to stdout

### To use this tool with darknet
1) run make build
2) modify configuration knobs used in launch scripts in darknet/run_*.sim

## Motivation
Studies have shown that one of the main hurdles to implementing convolutional neural networks on energy limited embedded systems is memory traffic to and from off-chip memory. One particular mathematical operation that dominates inference time
within a CNN as well as cause significant data movement is the convolution operation.
Generally CNNs implemented on embedded systems rely on heterogeneous computing facilities
such as embedded gpus or custom accelerators to increase performance while reducing energy
footprint. However, for more limited systems, these heterogeneous computing elements may be
too costly. An alternative to this approach is using general purpose processors with modified
caches to accelerate computations. One particularly interesting approach is to perform
convolutional neural network layer computations within the cache to reduce off chip traffic [1].
To better serve the approach in [1], maximizing data re-use, different prefetching techniques,
cache sizes, associativities, as well as replacement policies must be explored. In this project, I
aim to explore different cache configurations as well as cache-centric code optimization
techniques within the darknet convolutional neural network framework in order to determine
what configurations/ techniques maximize reuse and reduce off chp traffic.
With the explosion of convolutional neural networks in both data centers as well as the
edge, efficient hardware accelerators for cnn inference have gained a lot of traction. One
popular architecture in the literature for these accelerators is general matrix multipliers (gemm).
These gemm accelerators rely on a mathematical manipulation of the convolution operator that
changes the convolution operation into a generic matrix multiplication. This method is called
im2col which is described here [2]. This approach creates a significant amount of data
redundancy that puts unnecessary pressure on off chip memory in embedded environments.
More data redundancy means more transfers and thus higher latencies and power
consumption. To mitigate these negative effects a dedicated accelerator cache may be used.
The optimum configuration for this hypothetical cache is evaluated. One thing must be noted
though, the design of the cache as a process was done without considering a design for the
accelerator. In effect, this design space exploration process for the cache is accelerator
agnostic. The cache architectures explored where Column Associative caches, Hierarchical
Caches, and Prefetching Caches. Within these major architectures cache sizes, block sizes,
associativities, replacement policies were explored (when applicable).
## Testing Environment
The pin dcache tool was heavily modified and used as the platform for testing different caches.
Additionally, the darknet framework was used to evaluate several convolutional neural networks
along with different cache configurations to gain insight as to whether or not the network
architecture affects the optimal cache configuration. Since we are primarily interested in optimal
caches for gemm accelerators, the only functions in the darknet framework that should be
instrumented by pin is the gemm function. To achieve this, the filter pin tool was used to get the
address range of the assembly instructions of the gemm function. This address range was used
to restrict instrumentation to just the gemm function. It must be noted that pin is instrumenting
x86 instructions produced by the compiler that carry out gemm. While x86 instructions are in no
way similar to the operations carried out by gemm accelerators documented in the literature,
they can be used to estimate the expected memory addresses that will be demanded by a
generic accelerator. This assumption is both a blessing and a curse. With this assumption we
can attempt to find the optimum cache configuration without worrying too much about what
accelerator said cache will be hooked up to. Unfortunately, this approach makes any
conclusions drawn about the “best” cache configuration harder to validate given the inherent
inaccuracy in designing a cache for an incomplete system with widely varying memory access
patterns. To minimize this discrepancy between a potential gemm accelerator and x
instructions the compiler -g flag was used in order to prevent any x86 specific assembly
optimizations from happening as well as forcing the compiler to implement gemm in the most
naive way possible (assuming that the more naive implementation, the truer it is to gemma's
generic memory access patterns that is likely to be present in a gemm accelerator. Below is the
gemm function with a few minor modifications for prefetching and inline disabling.

```C

#define​ ​PREFETCH​ ​ // Used to enable and disable prefetching
//attribute forces function to be called from everywhere and not inlined
void​ ​__attribute__​((noinline)) ​gemm_nn​(​int​ ​M​, ​int​ ​N​, ​int​ ​K​, ​float​ ​ALPHA​,
                                                      ​float​ *​A​, ​int​ ​lda​,
                                                      ​float​ *​B​, ​int​ ​ldb​,
                                                      ​float​ *​C​, ​int​ ​ldc​)
{
    ​int​ i, j, k;
    ​ #pragma​ ​omp​ ​parallel​ ​for

        ​for​(i = ​ 0 ​; i < M; ++i)
    {
#ifdef​ ​PREFETCH
        ​if​(i < M - ​ 1 ​)
        {
            ​__builtin_prefetch​(&​C​[(i + ​ 1 ​) * ldc], ​ 0 ​, ​ 3 ​);
        }
#endif
        ​for​(k = ​ 0 ​; k < K; ++k)
        {
            ​register​ ​float​ A_PART = ALPHA * ​A​[i * lda + k];
#ifdef​ ​PREFETCH
            ​if​(k < K - ​ 1 ​)
            {
                ​__builtin_prefetch​(&​B​[(k + ​ 1 ​) * ldb], ​ 0 ​, ​ 3 ​);
            }
#endif
            ​for​(j = ​ 0 ​; j < N; ++j)
            {
                ​C​[i * ldc + j] += A_PART * ​B​[k * ldb + j];
            }
        }
    }
}

```

## Networks used
The following networks were used to test for optimal cache configurations
● Tiny-Yolo-v3: An object detection and localization network designed with low flops in
mind for embedded applications
● Tiny-darknet: An image recognition network similar to squeezenet. Designed with low
flops in mind for embedded applications
● Yolov3: The full sized version of tiny-yolo-v3 for object detection and localization, trades
much higher accuracy for greatly increased flops. Not as suitable for embedded
applications.
It must be noted that runtimes for the above networks was artificially constrained to 30 seconds
using the gnu timeout tool. This is due to the fact that runtimes would have been unmanageable

given the low compiler optimization applied to the framework as well as Pin’s instrumentation
overhead. However, 30 seconds proved sufficient for evaluated hit rates to converge for the
above networks.

## Features added to the dcache tool

The baseline dcache tool only provided data cache configuration for a single cache via the
command line. Associative caches were available with only one replacement policy, Round
Robin. The following features were added to allow for greater flexibility when evaluating optimal
caches.

● Hierarchy: A second level cache was added (enabled via a compiler directive) to allow for cache
hierarchy to be explored

● Support for small cache sizes: Given that silicon real estate is heavily constrained in low power embedded devices,
Small cache sizes needed to be supported (sizes less than 1kb)

● Column associativity: Column associativity was faithfully recreated from [3] (enabled via a compiler directive).
The algorithm for column associative cache replacement is presented below. Functions
b and f represent binary indexing and binary indexing with upper but flipping
respectively. To prevent incorrect aliasing between addresses that only differ in their
index bits (same tag) tag bits were extended by one bit

● LRU: LRU support was added in addition to Round Robin replacement. This was achieved
using staleness counters that tracked the staleness of cache lines in a set based on
when they were last updated.

● Prefetching detection: Within x86 there already exists an assembly command that prefetches requested data
into the cache while allowing the programmer to state that cache line’s expected
temporal locality. This instruction is accessible through

```c
__builtin_prefetch (void*, 0/1, 0-3);
```

The behavior of this particular instruction in terms of number of cycles taken to satisfy
the request depends on the state of the operating system and the cache. As a result, if it
is called too late and the requested data to prefetch is used immediately, then a miss will
occur if the data is not already present in the cache. This delay was not modelled by the
dcache tool, instead the request is serviced instantly, a significant deviation from x86’s
method of handling prefetch instructions. This instantaneous behavior can be used to
model a gemm specific prefetching scheme in hardware alongside the gemm
accelerator. The optimal placement of the prefetches in the original gemm code allows
for the exploration of optimal prefetching schemes for convolutional neural network
accelerators.

## Results
A series of tests were run with different cache configurations. The configuration of the cache for
each test is given as well as the networks run. Additionally, the goal of each test is given along
with the analysis of the results

### Test 1: Network differentiator
Goal
The purpose of this test was to determine whether the architecture of a convolutional neural
network has any effect on cache performance.
#### Configuration

● No Hierarchy (just one cache)

● Varied Replacement policy (LRU, RR)

● Varied Block Size (8, 32, 128 bytes)

● Varied Cache Size (256, 512, 1024 bytes)

● Varied Associativity (1-way → 8-way)

● Varied Networks (tiny darknet, tiny yolo v3, yolov3)

● Runtime for each network constrained to 30 seconds

![Test1 Results LRU](https://github.com/asultan123/CNN_Inference_cache_analysis_Intel_pin/blob/master/Results/image-001.jpg)
![Test1 Results RR](https://github.com/asultan123/CNN_Inference_cache_analysis_Intel_pin/blob/master/Results/image-002.jpg)

#### Analysis
Between networks there seems to be no significant difference in overall behavior at different
cache configurations. Cach performance scales dramatically with cache block size regardless of
the network as well as with cache size. If cache block size is too high the number of available
sets becomes too low which increases capacity misses. This is apparent at cache block size
128 with cache size 256.

### Test 2: Hierarchical Cache on tdark
Goal
The purpose of this test was to determine whether there are any merits to using a hierarchical
cache vs one monolithic cache. Hierarchical caches are useful if there is data sharing across
multiple accelerators however in this case we only have one. Other advantages of hierarchical
caches include being able to handle different operational speeds for variably sized caches.

#### Configuration

● 2 data Caches L1 and L

● RR Replacement policy

● Varied Block Size (8, 32, 128 bytes)

● Varied Cache Size (256, 512 bytes) L2 always double L

● Varied Associativity (1-way → 8-way)

● Network (tiny darknet)

● Runtime constrained to 30 seconds per configuration

![Test2 Results](https://github.com/asultan123/CNN_Inference_cache_analysis_Intel_pin/blob/master/Results/image-003.jpg)

#### Analysis
At lower cache sizes the hierarchical cache scheme provides good results for the overall area
provided, example configuration 256 - 512 - 32. Performance rivals that of a full 1Kb cache (see
linked google sheets) at a reduced area cost. Hierarchical configurations are generally useful if
multiple accelerators are sharing the available data. This is not the current case. In addition,
hierarchical schemes require duplicate access logic which may complicate the overall design.

### Test 3: Column Associativity on tdark
Goal
The purpose of this test was to determine whether there are any merits to using a Column
Associative cache vs a Set Associative Cache.

#### Configuration

● 1 data Cache L

● COL Replacement policy

● Varied Block Size (8, 32, 128 bytes)

● Varied Cache Size (256, 512, 1024 bytes)

● Varied Associativity (1-way → 8-way)

● Network (tiny darknet)

● Runtime constrained to 30 seconds per configuration

![Test3 Results](https://github.com/asultan123/CNN_Inference_cache_analysis_Intel_pin/blob/master/Results/image-004.jpg)

#### Analysis
Column associative caches performed well on low cache sizes but did not improve dramatically
over set associative caches with either LRU or RR replacement policies. Additionally, the rehash
logic necessary may incur significant access delays. Column associative caches performed
(overall) better than direct mapped caches but not better than set associative caches.

### Test 4: Prefetching on tdark
Goal
The purpose of this test was to determine whether there are any merits to applying prefetching
to gemm and whether there exists a particular prefetching scheme that improves cache
performance. (No optimizations regarding automatic prefetching have been considered due to
the compiler because optimization level is set to Og)

#### Configuration

● 1 data Cache L

● RR Replacement policy

● Varied Block Size (8, 32, 128 bytes)

● Varied Cache Size (256, 512, 1024 bytes)

● Fixed Associativity (4-way)

● Network (tiny darknet)

● Runtime constrained to 30 seconds per configuration

![Test4 Results](https://github.com/asultan123/CNN_Inference_cache_analysis_Intel_pin/blob/master/Results/image-005.jpg)

#### Analysis
Prefetching had no advantage over non prefetching despite the instant servicing of the prefetch
request. Additionally cache configuration size had no effect on prefetching effectiveness.

## Conclusion
Overall the optimal configuration depends primarily on cache block size and cache size. More
creative schemes need to be explored like splitting weight and input feature map data into
separate caches to prevent collision between them, however, this can be achieved by clever
compilers and may not necessarily have to be done in hardware

## References
[1] ​C. Eckert, X. Wang, J. Wang, A. Subramaniyan, R. Iyer, D. Sylvester, D. Blaaauw, and R. Das, “Neural Cache: Bit-Serial In-Cache Acceleration of Deep Neural Networks,” ​ 2018

[2] ​L. A. dos Santos, “Making faster,” ​ Making faster · Artificial Inteligence ​. [Online]. Available: https://leonardoaraujosantos.gitbooks.io/artificial-inteligence/content/making_faster.html.

[3] ​A. Agarwal and S. D. Pudar, “Column-associative caches,” ​ Proceedings of the 20th annual international symposium on Computer architecture - ISCA 93 ​, 1993.
