# SEL-bitmap

This is an improved version of PrIM SEL as implemented be Goméz-Luna et al.

In contrast to the original, which copies all selected elements to a single
output array, SEL-bitmap uses a bitmap to indicate the selected elements. Its
CPU baseline scales well with multiple threads, and its UPMEM implementation
uses a single read operation rather than one per DPU.

## Variability

* `make NR_DPUS=`*1*..*2560* sets the number of DPUs (default: none, controlled by `NR_RANKS`)
* `make NR_RANKS=`*1*..*40* sets the number of ranks (default: 1)
* `bin/host_code -i` *n\_elements* seth the number of elements (default: 1²⁸)
* If compiled with `numa=1`, `bin/host_code -a A -b B -c C` set the NUMA node of input data (A), output bitmap (B), and CPU code (C), respectively
* Use `../util/limit_ranks_to_numa_node` to control the NUMA node of UPMEM  ranks

Note that `NR_DPUS` and `NR_RANKS` are mutually exclusive.
