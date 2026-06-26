/*
 * JGL@SAFARI
 */

/**
 * CPU code with Thrust
 */
#include <assert.h>
#include <getopt.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <fstream>
#include <iostream>
#include <math.h>
#include <sys/time.h>

#include <thrust/copy.h>
#include <thrust/device_vector.h>
#include <thrust/host_vector.h>
#include <thrust/scan.h>
#include <thrust/system/omp/execution_policy.h>
#include <thrust/system/omp/vector.h>
#include <vector>

#include <omp.h>

#include "../../include/common.h"

#if DFATOOL_TIMING
#include "../../include/timer.h"
Timer timer;
#else
#define start(...)
#define stop(...)
#endif

#if WITH_PERF_LIB
extern "C" {
#include "../../../include/perf-lib.h"
};
#elif WITH_PERF_EXT
extern "C" {
#include "../../../include/perf-ext.h"
};
#else
#define perf_start(...)
#define perf_stop(...)
#endif

#if NUMA
#include <numa.h>
#include <numaif.h>

void* mp_pages[1];
int mp_status[1];
int mp_nodes[1];
int numa_node_in = -1;
int numa_node_cpu = -1;
#endif

#define XSTR(x) STR(x)
#define STR(x) #x

#define ANSI_COLOR_RED "\x1b[31m"
#define ANSI_COLOR_GREEN "\x1b[32m"
#define ANSI_COLOR_RESET "\x1b[0m"

// Pointer declaration
static T* A;

volatile T count;

/**
 * @brief creates input arrays
 * @param nr_elements how many elements in input arrays
 */
static void read_input(T* A, unsigned long nr_elements)
{
	// srand(0);
	for (unsigned long i = 0; i < nr_elements; i++) {
		// A[i] = (T) (rand()) % 2;
		A[i] = i;
	}
}

// Params ---------------------------------------------------------------------
typedef struct Params {
	unsigned long input_size;
	int n_warmup;
	int n_reps;
	int n_threads;
#if NUMA
	struct bitmask* bitmask_in;
	struct bitmask* bitmask_out;
	int numa_node_cpu;
#endif
} Params;

void usage()
{
	fprintf(stderr,
	    "\nUsage:  ./program [options]"
	    "\n"
	    "\nGeneral options:"
	    "\n    -h        help"
	    "\n    -w <W>    # of untimed warmup iterations (default=1)"
	    "\n    -e <E>    # of timed repetition iterations (default=3)"
	    "\n    -t <T>    # of threads (default=8)"
	    "\n"
	    "\nBenchmark-specific options:"
	    "\n    -i <I>    input size (default=2M elements)"
	    "\n");
}

struct Params input_params(int argc, char** argv)
{
	struct Params p;
	p.input_size = 2 << 20;
	p.n_warmup = 1;
	p.n_reps = 3;
	p.n_threads = 4;
#if NUMA
	p.bitmask_in = NULL;
	p.bitmask_out = NULL;
	p.numa_node_cpu = -1;
#endif

	int opt;
	while ((opt = getopt(argc, argv, "hi:w:e:t:A:B:C:")) >= 0) {
		switch (opt) {
		case 'h':
			usage();
			exit(0);
			break;
		case 'i':
			p.input_size = atol(optarg);
			break;
		case 'w':
			p.n_warmup = atoi(optarg);
			break;
		case 'e':
			p.n_reps = atoi(optarg);
			break;
		case 't':
			p.n_threads = atoi(optarg);
			break;
#if NUMA
		case 'A':
			p.bitmask_in = numa_parse_nodestring(optarg);
			break;
		case 'B':
			p.bitmask_out = numa_parse_nodestring(optarg);
			break;
		case 'C':
			p.numa_node_cpu = atoi(optarg);
			break;
#endif
		default:
			fprintf(stderr, "\nUnrecognized option!\n");
			usage();
			exit(0);
		}
	}
	assert(p.n_threads > 0 && "Invalid # of threads!");

	return p;
}

/**
 * @brief Main of the Host Application.
 */
int main(int argc, char** argv)
{

	struct Params p = input_params(argc, argv);

	assert(p.input_size % (p.n_threads) == 0 && "Input size!");

	// Input/output allocation

#if NUMA
	if (p.bitmask_in) {
		numa_set_membind(p.bitmask_in);
		numa_free_nodemask(p.bitmask_in);
	}
	A = (T*)numa_alloc(p.input_size * sizeof(T));
#else
	A = (T*)malloc(p.input_size * sizeof(T));
#endif

	// Create an input file with arbitrary data.
	read_input(A, p.input_size);

#if NUMA
	mp_pages[0] = A;
	if (move_pages(0, 1, mp_pages, NULL, mp_status, 0) == -1) {
		perror("move_pages(A)");
	} else if (mp_status[0] < 0) {
		printf("move_pages error: %d", mp_status[0]);
	} else {
		numa_node_in = mp_status[0];
	}

	numa_node_cpu = p.numa_node_cpu;
	if (numa_node_cpu != -1) {
		if (numa_run_on_node(numa_node_cpu) == -1) {
			perror("numa_run_on_node");
			numa_node_cpu = -1;
		}
	}
#endif

	// Loop over main kernel
	for (int rep = 0; rep < p.n_warmup + p.n_reps; rep++) {

		thrust::omp::vector<T> d_input(p.input_size);
		memcpy(thrust::raw_pointer_cast(&d_input[0]), A, p.input_size * sizeof(T));

		omp_set_num_threads(p.n_threads);

		unsigned int nr_threads = 0;
#pragma omp parallel
#pragma omp atomic
		nr_threads++;

		perf_start();
		start(&timer, 0, 0);
		count = thrust::reduce(thrust::omp::par, d_input.begin(), d_input.end());
		stop(&timer, 0);
		perf_stop();

		if (rep >= p.n_warmup) {
#if WITH_PERF_LIB
			printf("[::] RED-CPU | n_threads=%d e_type=%s n_elements=%lu",
			    nr_threads, XSTR(T), p.input_size);
#if NUMA
			printf(" numa_node_in=%d numa_node_cpu=%d numa_distance_in_cpu=%d",
			    numa_node_in, numa_node_cpu, numa_distance(numa_node_in, numa_node_cpu));
#endif
			printf(" |");
			perf_print();
#elif DFATOOL_TIMING
			printf("[::] RED-CPU | n_threads=%d e_type=%s n_elements=%lu"
#if NUMA
			       " numa_node_in=%d numa_node_cpu=%d numa_distance_in_cpu=%d"
#endif
			       " | throughput_MBps=%f",
			    nr_threads, XSTR(T), p.input_size,
#if NUMA
			    numa_node_in, numa_node_cpu, numa_distance(numa_node_in, numa_node_cpu),
#endif
			    p.input_size * sizeof(T) / timer.time[0]);
			printf(" throughput_MOpps=%f latency_us=%f\n",
			    p.input_size / timer.time[0],
			    timer.time[0]);
#endif // DFATOOL_TIMING
		}
	}

	// Deallocation
#if NUMA
	numa_free(A, p.input_size * sizeof(T));
#else
	free(A);
#endif

	return 0;
}
