# PrIM (Processing-In-Memory Benchmarks) Extended

This is an improved and extended version of the [PrIM benchmark suite](https://github.com/CMU-SAFARI/prim-benchmarks) originally developed for **UPMEM PIM** (near-memory computing / processing-in-memory) evaluation by Gómez-Luna et al.
The extension adds

* support for **NUMA**-aware **UPMEM**, **CXL** (Compute eXpress Link), **HBM** (high-bandwidth memory) and **DRAM** benchmarks,
* fine-granular timing and tracing support for **CPU**, **GPU** and **UPMEM**,
* a new **SEL-bitmap** benchmark, which scales much better than the original **SEL** implementation both on CPU and on UPMEM, and
* numerous bugfixes.

## Publications

Depending on how you use this repository, you may want to cite of one or more of the following publications.

### Timing and Tracing

**B. Friesel** and O. Spinczyk.
[Overhead Prediction for PIM-Enabled Applications with Performance-Aware Behaviour Models](https://ess.cs.uos.de/static/papers/Friesel-2025-CCMCC.pdf).
In 

```
@inproceedings{friesel2025ccmcc,
  author = {Friesel, Birte and Spinczyk, Olaf},
  title = {{Overhead Prediction for PIM-Enabled Applications with Performance-Aware Behaviour Models}},
  year = {2025},
  month = 10,
  publisher = ieee,
  pages={1--8},
  booktitle = {Proceedings of the 1st Cross-disciplinary Conference on Memory-Centric Computing},
  numpages = {7},
  location = {Dresden, Germany},
  series = {CCMCC '25},
  download-url = {https://ess.cs.uos.de/static/papers/Friesel-2025-CCMCC.pdf},
  doi={10.1109/CCMCC67628.2025.11380800}
}
```

### NUMA-Specific Benchmarks / HBM Support

**B. Friesel**, M. Lütke Dreimann, and O. Spinczyk.
[Performance Models for Task-based Scheduling with Disruptive Memory Technologies](https://ess.cs.uos.de/static/papers/Friesel-2024-DIMES.pdf).
In *Proceedings of the 2nd Workshop on Disruptive Memory Systems*, DIMES '24, 2024.
Association for Computing Machinery.
[DOI: 10.1145/3698783.3699376](http://dx.doi.org/10.1145/3698783.3699376])

```
@inproceedings{friesel2024dimes,
author = {Friesel, Birte and L{\"u}tke Dreimann, Marcel and Spinczyk, Olaf},
title = {Performance Models for Task-based Scheduling with Disruptive Memory Technologies},
year = {2024},
month = 11,
doi = {10.1145/3698783.3699376},
isbn = {979-8-4007-1303-3},
download-url = {https://ess.cs.uos.de/static/papers/Friesel-2024-DIMES.pdf},
pages = {1--8},
numpages = {8},
publisher = {Association for Computing Machinery},
address = {New York, NY, USA},
booktitle = {Proceedings of the 2nd Workshop on Disruptive Memory Systems},
location = {Austin, TX, USA},
series = {DIMES '24}
```

### Setup Cost and Data Transfer Overhead Benchmarks

**B. Friesel**, M. Lütke Dreimann, and O. Spinczyk.
[A Full-System Perspective on UPMEM Performance](https://ess.cs.uos.de/static/papers/Friesel-2023-DIMES.pdf).
In *Proceedings of the 1st Workshop on Disruptive Memory Systems*, DIMES '23, pages 1–7, 2023.
Association for Computing Machinery.
[DOI: 10.1145/3609308.3625266](http://dx.doi.org/10.1145/3609308.3625266)

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

### UPMEM Characterization / Benchmark Foundation

**Juan Gómez-Luna**, Izzat El Hajj, Ivan Fernandez, Christina Giannoula, Geraldo F. Oliveira, and Onur Mutlu, "[Benchmarking a New Paradigm: Experimental Analysis and Characterization of a Real Processing-in-Memory System](https://ieeexplore.ieee.org/abstract/document/9771457)". IEEE Access (2022).

```
@article{gomez2022pim,
  author={Gómez-Luna, Juan and Hajj, Izzat El and Fernandez, Ivan and Giannoula, Christina and Oliveira, Geraldo F. and Mutlu, Onur},
  journal={IEEE Access},
  title={Benchmarking a New Paradigm: Experimental Analysis and Characterization of a Real Processing-in-Memory System},
  year={2022},
  volume={10},
  pages={52565--52608},
  doi={10.1109/ACCESS.2022.3174101}
}
```

## Benchmark status

Almost all benchmarks and helper scripts have been extended significantly, typically with specific publications in mind.
We will provide detailed READMEs in the individual benchmark repositories once time permits.

## References

Up-to-date source code is available on the following mirrors:

* [ESS](https://ess.cs.uos.de/git/software/smaug/prim-benchmarks)
* [Finelrewind](https://git.finalrewind.org/derf/prim-benchmarks)
* [GitHub](https://github.com/derf/prim-benchmarks)

The original repository, including the original README, are available at <https://github.com/CMU-SAFARI/prim-benchmarks>.
