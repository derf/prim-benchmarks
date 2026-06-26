/*
 * Copyright (c) 2016 University of Cordoba and University of Illinois
 * All rights reserved.
 *
 * Developed by:    IMPACT Research Group
 *                  University of Cordoba and University of Illinois
 *                  http://impact.crhc.illinois.edu/
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * with the Software without restriction, including without limitation the
 * rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
 * sell copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 *      > Redistributions of source code must retain the above copyright notice,
 *        this list of conditions and the following disclaimers.
 *      > Redistributions in binary form must reproduce the above copyright
 *        notice, this list of conditions and the following disclaimers in the
 *        documentation and/or other materials provided with the distribution.
 *      > Neither the names of IMPACT Research Group, University of Cordoba,
 *        University of Illinois nor the names of its contributors may be used
 *        to endorse or promote products derived from this Software without
 *        specific prior written permission.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * CONTRIBUTORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS WITH
 * THE SOFTWARE.
 *
 */

#include "kernel.h"
#include "support/common.h"
#include "support/setup.h"
#include "support/verify.h"

#if DFATOOL_TIMING
#include "support/timer.h"
#else
#include <string>
struct Timer {
	inline void start(std::string name) { (void)name; }
	inline void stop(std::string name) { (void)name; }
};
#endif

#if WITH_PERF_LIB
extern "C" {
#include "../../../include/perf-lib.h"
};
#else
#define perf_start(...)
#define perf_stop(...)
#endif

#include <assert.h>
#include <string.h>
#include <thread>
#include <unistd.h>

#define XSTR(x) STR(x)
#define STR(x) #x

#if NUMA
#include "../../../include/numa.h"
#else
#define numa_bind_alloc(size, bitmask) malloc(size)
#define numa_free(data, size) free(data)
#endif

// Params ---------------------------------------------------------------------
struct Params {

	int n_threads;
	int n_warmup;
	int n_reps;
	int M_;
	int m;
	int N_;
	int n;
#if NUMA
	struct bitmask* bitmask_in;
	int numa_node_cpu;
#endif

	Params(int argc, char** argv)
	{
		n_threads = 4;
		n_warmup = 1;
		n_reps = 3;
		M_ = 128;
		m = 16;
		N_ = 128;
		n = 8;
#if NUMA
		bitmask_in = NULL;
		numa_node_cpu = -1;
#endif
		int opt;
		while ((opt = getopt(argc, argv, "ht:w:e:m:n:o:p:A:C:")) >= 0) {
			switch (opt) {
			case 'h':
				usage();
				exit(0);
				break;
			case 't':
				n_threads = atoi(optarg);
				break;
			case 'w':
				n_warmup = atoi(optarg);
				break;
			case 'e':
				n_reps = atoi(optarg);
				break;
			case 'm':
				m = atoi(optarg);
				break;
			case 'n':
				n = atoi(optarg);
				break;
			case 'o':
				M_ = atoi(optarg);
				break;
			case 'p':
				N_ = atoi(optarg);
				break;
#if NUMA
			case 'A':
				bitmask_in = numa_parse_nodestring(optarg);
				break;
			case 'C':
				numa_node_cpu = atoi(optarg);
				break;
#endif // NUMA
			default:
				fprintf(stderr, "\nUnrecognized option!\n");
				usage();
				exit(0);
			}
		}
	}

	void usage()
	{
		fprintf(stderr,
		    "\nUsage:  ./trns [options]"
		    "\n"
		    "\nGeneral options:"
		    "\n    -h        help"
		    "\n    -t <T>    # of host threads (default=4)"
		    "\n    -w <W>    # of untimed warmup iterations (default=5)"
		    "\n    -r <R>    # of timed repetition iterations (default=50)"
		    "\n"
		    "\nData-partitioning-specific options:"
		    "\n    TRNS only supports CPU-only or GPU-only execution"
		    "\n"
		    "\nBenchmark-specific options:"
		    "\n    -m <I>    m (default=16 elements)"
		    "\n    -n <I>    n (default=8 elements)"
		    "\n    -o <I>    M_ (default=128 elements)"
		    "\n    -p <I>    N_ (default=128 elements)"
		    "\n");
	}
};

// Input Data -----------------------------------------------------------------
void read_input(T* x_vector, const Params& p)
{
	unsigned long in_size = p.M_ * p.m * p.N_ * p.n;
	srand(5432);
	for (unsigned long i = 0; i < in_size; i++) {
		x_vector[i] = ((T)(rand() % 100) / 100);
	}
}

// Main ------------------------------------------------------------------------------------------
int main(int argc, char** argv)
{

	const Params p(argc, argv);
	Timer timer;

	// Allocate
	int M_ = p.M_;
	int m = p.m;
	int N_ = p.N_;
	int n = p.n;
	unsigned long in_size = M_ * m * N_ * n;
	unsigned long finished_size = M_ * m * N_;

#if NUMA
	numa_node_cpu = numa_cpu_bind(p.numa_node_cpu);
#endif

	T* h_in_backup = (T*)malloc(in_size * sizeof(T));
	ALLOC_ERR(h_in_backup);

	T* h_in_out = (T*)numa_bind_alloc(in_size * sizeof(T), p.bitmask_in);
	T* h_local = h_in_out;

	std::atomic_int* h_finished = (std::atomic_int*)numa_bind_alloc(sizeof(std::atomic_int) * finished_size, p.bitmask_in);
	std::atomic_int* h_head = (std::atomic_int*)numa_bind_alloc(N_ * sizeof(std::atomic_int), p.bitmask_in);

	ALLOC_ERR(h_in_out, h_finished, h_head);

	// Initialize
	read_input(h_in_out, p);
	memset((void*)h_finished, 0, sizeof(std::atomic_int) * finished_size);
	for (int i = 0; i < N_; i++)
		h_head[i].store(0);

	memcpy(h_in_backup, h_in_out, in_size * sizeof(T)); // Backup for reuse across iterations

#if NUMA
	numa_node_in = numa_get_node_of_page(h_in_out, "h_in_out");
#endif

	// Loop over main kernel
	for (int rep = 0; rep < p.n_warmup + p.n_reps; rep++) {

		// Reset
		memset((void*)h_finished, 0, sizeof(std::atomic_int) * finished_size);
		for (int i = 0; i < N_; i++)
			h_head[i].store(0);

		timer.start("Step 1");
		perf_start();
		std::thread main_thread_1(run_cpu_threads_100, h_local, h_finished, h_head, M_ * m, N_, n, p.n_threads); // M_ * m * N_);
		main_thread_1.join();
		perf_stop();
		timer.stop("Step 1");

#if WITH_PERF_LIB
		printf("[::] TRNS-CPU-kernel1 | n_threads=%d e_type=%s n_elements=%lu",
		    p.n_threads, XSTR(T), in_size);
#if NUMA
		printf(" numa_node_inout=%d numa_node_cpu=%d numa_distance_inout_cpu=%d",
		    numa_node_in, numa_node_cpu, numa_distance(numa_node_in, numa_node_cpu));
#endif
		printf(" |");
		perf_print();
#endif

#if ASPECTC
		printf("[::] run_cpu_threads_100 @ %s:%d | arg1=%d arg2=%d arg3=%d n_threads=%d | latency_us=%f\n",
		    __FILE__, __LINE__,
		    M_ * m, N_, n, p.n_threads,
		    timer.get("Step 1"));
#endif

		for (int i = 0; i < N_; i++)
			h_head[i].store(0);

		timer.start("Step 2");
		perf_start();
		std::thread main_thread_2(run_cpu_threads_010, h_local, h_head, m, n, M_ * N_, p.n_threads);
		main_thread_2.join();
		perf_stop();
		timer.stop("Step 2");

#if WITH_PERF_LIB
		printf("[::] TRNS-CPU-kernel2 | n_threads=%d e_type=%s n_elements=%lu",
		    p.n_threads, XSTR(T), in_size);
#if NUMA
		printf(" numa_node_inout=%d numa_node_cpu=%d numa_distance_inout_cpu=%d",
		    numa_node_in, numa_node_cpu, numa_distance(numa_node_in, numa_node_cpu));
#endif
		printf(" |");
		perf_print();
#endif

#if ASPECTC
		printf("[::] run_cpu_threads_010 @ %s:%d | arg1=%d arg2=%d arg3=%d n_threads=%d | latency_us=%f\n",
		    __FILE__, __LINE__,
		    m, n, M_ * N_, p.n_threads,
		    timer.get("Step 2"));
#endif

		memset((void*)h_finished, 0, sizeof(std::atomic_int) * finished_size);
		for (int i = 0; i < N_; i++)
			h_head[i].store(0);

		timer.start("Step 3");
		perf_start();
		for (int i = 0; i < N_; i++) {
#if ASPECTC
			timer.start("Step 3");
#endif
			std::thread main_thread_3(run_cpu_threads_100, h_local + i * M_ * n * m, h_finished + i * M_ * n, h_head + i, M_, n, m, p.n_threads); // M_ * n);
			main_thread_3.join();
#if ASPECTC
			timer.stop("Step 3");
			printf("[::] run_cpu_threads_100 @ %s:%d | arg1=%d arg2=%d arg3=%d n_threads=%d | latency_us=%f\n",
			    __FILE__, __LINE__,
			    M_ * m, N_, n, p.n_threads,
			    timer.get("Step 3"));
#endif
		}
		perf_stop();
		timer.stop("Step 3");
		// end timer

#if WITH_PERF_LIB
		printf("[::] TRNS-CPU-kernel3 | n_threads=%d e_type=%s n_elements=%lu",
		    p.n_threads, XSTR(T), in_size);
#if NUMA
		printf(" numa_node_inout=%d numa_node_cpu=%d numa_distance_inout_cpu=%d",
		    numa_node_in, numa_node_cpu, numa_distance(numa_node_in, numa_node_cpu));
#endif
		printf(" |");
		perf_print();
#endif

#if DFATOOL_TIMING
		if (rep >= p.n_warmup) {
			printf("[::] TRNS-CPU | n_threads=%d e_type=%s n_elements=%lu",
			    p.n_threads, XSTR(T), in_size);
#if NUMA
			printf(" numa_node_inout=%d numa_node_cpu=%d numa_distance_inout_cpu=%d",
			    numa_node_in, numa_node_cpu, numa_distance(numa_node_in, numa_node_cpu));
#endif
			printf(" | throughput_MBps=%f",
			    in_size * sizeof(T) / (timer.get("Step 1") + timer.get("Step 2") + timer.get("Step 3")));
			printf(" throughput_MOpps=%f",
			    in_size / (timer.get("Step 1") + timer.get("Step 2") + timer.get("Step 3")));
			printf(" latency_step1_us=%f latency_step2_us=%f latency_step3_us=%f latency_us=%f\n",
			    timer.get("Step 1"), timer.get("Step 2"), timer.get("Step 3"),
			    timer.get("Step 1") + timer.get("Step 2") + timer.get("Step 3"));
		}
#endif
	}

	// Free memory
#if NUMA
	numa_free(h_in_out, in_size * sizeof(T));
	numa_free(h_finished, sizeof(std::atomic_int) * finished_size);
	numa_free(h_head, N_ * sizeof(std::atomic_int));
	numa_free(h_in_backup, in_size * sizeof(T));
#else
	free(h_in_out);
	free(h_finished);
	free(h_head);
	free(h_in_backup);
#endif

	return 0;
}
