# PrIM (Processing-In-Memory Benchmarks) Extended

This is an improved and extended version of the PrIM benchmark suite originally developed for **UPMEM PIM** (near-memory computing / processing-in-memory) evaluation by Gómez-Luna et al.
The extension adds

* support for **NUMA**-aware **HBM** (high-bandwidth memory) and **DRAM** benchmarks,
* a new **COUNT** benchmark, and
* numerous bugfixes.

Pleace cite *friesel2024dimes* for NUMA-specific benchmarks, *friesel2023dimes* for setup cost and data transfer overhead benchmarks, and *gomez2022benchmarking* for everything else.

The benchmarks in this repository have been used in the following publications.

**B. Friesel**, M. Lütke Dreimann, and O. Spinczyk. [A Full-System Perspective on UPMEM Performance](https://ess.cs.uos.de/static/papers/Friesel-2023-DIMES.pdf). In *Proceedings of the 1st Workshop on Disruptive Memory Systems*, DIMES '23, pages 1–7, 2023. Association for Computing Machinery.
[DOI: 10.1145/3609308.3625266](http://dx.doi.org/10.1145/3609308.3625266)
|
[Presentation Slides](https://ess.cs.uos.de/static/papers/Friesel-2023-DIMES-Slides.pdf)

```
@inproceedings{friesel2023dimes,
author = {Friesel, Birte and L{\"u}tke Dreimann, Marcel and Spinczyk, Olaf},
title = {{A Full-System Perspective on UPMEM Performance}},
year = {2023},
month = 10,
isbn = {979-8-4007-0300-3},
publisher = {Association for Computing Machinery},
address = {New York, NY, USA},
doi = {10.1145/3609308.3625266},
booktitle = {Proceedings of the 1st Workshop on Disruptive Memory Systems},
pages = {1--7},
numpages = {7},
location = {Koblenz, Germany},
series = {DIMES '23}
}
```

**B. Friesel**, M. Lütke Dreimann, and O. Spinczyk. Performance Models for Task-based Scheduling with Disruptive Memory Technologies. In *Proceedings of the 2nd Workshop on Disruptive Memory Systems*, DIMES '24, 2024. Association for Computing Machinery. to appear.

```
@inproceedings{friesel2024dimes,
author = {Friesel, Birte and L{\"u}tke Dreimann, Marcel and Spinczyk, Olaf},
title = {Performance Models for Task-based Scheduling with Disruptive Memory Technologies},
year = {2024},
month = 11,
publisher = {Association for Computing Machinery},
address = {New York, NY, USA},
booktitle = {Proceedings of the 2nd Workshop on Disruptive Memory Systems},
location = {Austin, TX, USA},
series = {DIMES '24},
category = {ESS, SMAUG},
note = {to appear}
}
```

Up-to-date source code is available on the following mirrors:
* [GitHub](https://github.com/derf/prim-benchmarks)
* [ess.cs.uos.de](https://ess.cs.uos.de/git/software/smaug/prim-benchmarks)

The original README follows.

---

# PrIM (Processing-In-Memory Benchmarks)

PrIM is the first benchmark suite for a real-world processing-in-memory (PIM) architecture. 
PrIM is developed to evaluate, analyze, and characterize the first publicly-available real-world processing-in-memory (PIM) architecture, the [UPMEM](https://www.upmem.com/) PIM architecture. 
The UPMEM PIM architecture combines traditional DRAM memory arrays with general-purpose in-order cores, called DRAM Processing Units (DPUs), integrated in the same chip.

PrIM provides a common set of workloads to evaluate the UPMEM PIM architecture with and can be useful for programming, architecture and system researchers all alike to improve multiple aspects of future PIM hardware and software. 
The workloads have different characteristics, exhibiting heterogeneity in their memory access patterns, operations and data types, and communication patterns. 
This repository also contains baseline CPU and GPU implementations of PrIM benchmarks for comparison purposes. 

PrIM also includes a set of microbenchmarks can be used to assess various architecture limits such as compute throughput and memory bandwidth.

## Citation

Please cite the following papers if you find this repository useful.

The short paper version contains the key takeaways of our architecture characterization and workload suitability study:

Juan Gómez-Luna, Izzat El Hajj, Ivan Fernandez, Christina Giannoula, Geraldo F. Oliveira, and Onur Mutlu, "[Benchmarking Memory-centric Computing Systems: Analysis of Real Processing-in-Memory Hardware](https://ieeexplore.ieee.org/abstract/document/9651614)". 2021 12th International Green and Sustainable Computing Conference (IGSC). IEEE, 2021.

Bibtex entry for citation:
```
@inproceedings{gomez2021benchmarking,
  title={{Benchmarking Memory-centric Computing Systems: Analysis of Real Processing-in-Memory Hardware}},
  author={Juan Gómez-Luna and Izzat El Hajj and Ivan Fernandez and Christina Giannoula and Geraldo F. Oliveira and Onur Mutlu},
  booktitle={2021 12th International Green and Sustainable Computing Conference (IGSC)},
  year={2021},
  organization={IEEE}
}
```

The long paper version contains all details of our work: key observations, programming recommendations, and key takeaways:

Juan Gómez-Luna, Izzat El Hajj, Ivan Fernandez, Christina Giannoula, Geraldo F. Oliveira, and Onur Mutlu, "[Benchmarking a New Paradigm: Experimental Analysis and Characterization of a Real Processing-in-Memory System](https://ieeexplore.ieee.org/abstract/document/9771457)". IEEE Access (2022).

Juan Gómez-Luna, Izzat El Hajj, Ivan Fernandez, Christina Giannoula, Geraldo F. Oliveira, and Onur Mutlu, "[Benchmarking a New Paradigm: An Experimental Analysis of a Real Processing-in-Memory Architecture](https://arxiv.org/pdf/2105.03814.pdf)". arXiv:2105.03814 [cs.AR], 2021.

Bibtex entries for citation:
```
@article{gomez2022benchmarking,
  title={{Benchmarking a New Paradigm: Experimental Analysis and Characterization of a Real Processing-in-Memory System},
  author={Juan Gómez-Luna and Izzat El Hajj and Ivan Fernandez and Christina Giannoula and Geraldo F. Oliveira and Onur Mutlu},
  journal={IEEE Access},
  volume={10},
  pages={52565--52608},
  year={2022},
  publisher={IEEE}
}
```
```
@misc{gomezluna2021prim,
  title={{Benchmarking a New Paradigm: An Experimental Analysis of a Real Processing-in-Memory Architecture}}, 
  author={Juan Gómez-Luna and Izzat El Hajj and Ivan Fernandez and Christina Giannoula and Geraldo F. Oliveira and Onur Mutlu},
  year={2021},
  eprint={2105.03814},
  archivePrefix={arXiv},
  primaryClass={cs.AR}
}
```

## Repository Structure and Installation

We point out next the repository structure and some important folders and files. 
All benchmark folders have similar structure to the one shown for BFS. 
The microbenchmark folder contains eight different microbenchmarks, each with similar folder structure. 
The repository also includes `run_*.py` scripts to run strong and weak scaling experiments for PrIM benchmarks.

```
.
+-- LICENSE
+-- README.md
+-- run_strong_full.py
+-- run_strong_rank.py
+-- run_weak.py
+-- BFS/
|   +-- baselines/
|	|	+-- cpu/
|	|	+-- gpu/
|   +-- data/
|   +-- dpu/
|   +-- host/
|   +-- support/
|   +-- Makefile
+-- BS/
|   +-- ...
+-- GEMV/
|   +-- ...
+-- HST-L/
|   +-- ...
+-- HST-S/
|   +-- ...
+-- MLP/
|   +-- ...
+-- Microbenchmarks/
|   +-- Arithmetic-Throughput/
|   +-- CPU-DPU/
|   +-- MRAM-Latency/
|   +-- Operational-Intensity/
|   +-- Random-GUPS/
|   +-- STREAM/
|   +-- STRIDED/
|   +-- WRAM/
+-- NW/
|   +-- ...
+-- RED/
|   +-- ...
+-- SCAN-SSA/
|   +-- ...
+-- SCAN-RSS/
|   +-- ...
+-- SEL/
|   +-- ...
+-- SpMV/
|   +-- ...
+-- TRNS/
|   +-- ...
+-- TS/
|   +-- ...
+-- UNI/
|   +-- ...
+-- VA/
|   +-- ...
```

### Prerequisites

Running PrIM requires installing the [UPMEM SDK](https://sdk.upmem.com). 
PrIM benchmarks and microbenchmarks are designed to run on a server with real UPMEM modules, but they also run on the functional simulator include in the UPMEM SDK.

### Getting Started

Clone the repository:
```sh
git clone https://github.com/CMU-SAFARI/prim-benchmarks

cd prim-benchmarks
./set-root-dir.sh
```

## Running PrIM

### PrIM Benchmarks 

The repository includes scripts to run weak scaling and strong scaling experiments:
* `run_weak.py`: Weak scaling experiments for 16 PrIM benchmarks using 1 rank of UPMEM DPUs (1 to 64 DPUs).
* `run_strong_rank.py`: Strong scaling experiments for 16 PrIM benchmarks using 1 rank of UPMEM DPUs (1 to 64 DPUs).
* `run_strong_full.py`: Strong scaling experiments for 16 PrIM benchmarks using 4 to 32 ranks of UPMEM DPUs (256 to 2048 DPUs).

To run weak scaling experiments for BFS or SpMV, update the paths to input files in `run_weak.py`. 
The scripts save the results in a folder called `profile` inside each benchmark folder.

```sh
cd prim-benchmarks

# Weak scaling experiments for BFS
python3 run_weak.py BFS
```

Inside each PrIM benchmark folder, one can compile and run each benchmark with different input parameters. 
Choose a benchmark and compile. Every Makefile accepts several input parameters:
```sh
cd BFS

# Compile BFS for 32 DPUs and 16 tasklets (i.e., software threads) per DPU
NR_DPUS=32 NR_TASKLETS=16 make all
```

For help instructions:
```sh
./bin/host_code -h
```

Run the benchmark:
```sh
./bin/host_code -v 0 -f data/loc-gowalla_edges.txt
```

Several benchmark folders (HST-S, HST-L, RED, SCAN-SSA, SCAN-RSS) contain a script (`run.sh`) that compiles and runs the benchmark for the experiments in the appendix of the [paper](https://arxiv.org/pdf/2105.03814.pdf).

### Microbenchmarks 

Each microbenchmark folder contains a script (`run.sh`) that compiles and runs the microbenchmark for the experiments in the [paper](https://arxiv.org/pdf/2105.03814.pdf):

```sh
cd Microbenchmarks/Arithmetic-Throughput

./run.sh
```

### Getting Help

If you have any suggestions for improvement, please contact el1goluj at gmail dot com. 
If you find any bugs or have further questions or requests, please post an issue at the [issue page](https://github.com/CMU-SAFARI/prim-benchmarks/issues).


## Acknowledgments 

We thank UPMEM’s Fabrice Devaux, Rémy Cimadomo, Romaric Jodin, and Vincent Palatin for their valuable support. We acknowledge the support of SAFARI Research Group’s industrial partners, especially ASML, Facebook, Google, Huawei, Intel, Microsoft, VMware, and the Semiconductor Research Corporation. Izzat El Hajj acknowledges the support of the University Research Board of the American University of Beirut (URB-AUB-103951-25960). 
