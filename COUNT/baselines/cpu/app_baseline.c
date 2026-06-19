/**
 * @file app.c
 * @brief Template for a Host Application Source File.
 *
 */
#include <assert.h>
#include <getopt.h>
#include <omp.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#if DFATOOL_TIMING
#include "../../include/timer.h"
#else
#define start(...)
#define stop(...)
#endif

#if WITH_PERF_LIB
#include "../../../include/perf-lib.h"
#elif WITH_PERF_EXT
#include "../../../include/perf-ext.h"
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

#ifndef T
#define T uint64_t
#endif

volatile unsigned long total_count;

typedef struct Params {
	unsigned long input_size;
	int n_warmup;
	int n_reps;
	int n_threads;
#if NUMA
	struct bitmask* bitmask_in;
	int numa_node_cpu;
#endif
} Params;

struct Params p;

static T* A;

bool pred(const T x)
{
	return (x % 2) == 0;
}

void fill_column(unsigned long nr_elements)
{
	// srand(0);

#if NUMA
	if (p.bitmask_in != NULL) {
		numa_set_membind(p.bitmask_in);
		numa_free_nodemask(p.bitmask_in);
	}
	A = (T*)numa_alloc(nr_elements * sizeof(T));
#else
	A = (T*)malloc(nr_elements * sizeof(T));
#endif

#if NUMA
	struct bitmask* bitmask_all = numa_allocate_nodemask();
	numa_bitmask_setall(bitmask_all);
	numa_set_membind(bitmask_all);
	numa_free_nodemask(bitmask_all);
#endif

	for (unsigned long i = 0; i < nr_elements; i++) {
		// A[i] = (unsigned int) (rand());
		A[i] = i + 1;
	}

#if NUMA
	if (p.bitmask_in != NULL) {
		mp_pages[0] = A;
		if (move_pages(0, 1, mp_pages, NULL, mp_status, 0) == -1) {
			perror("move_pages(A)");
		} else if (mp_status[0] < 0) {
			printf("move_pages error: %d", mp_status[0]);
		} else {
			numa_node_in = mp_status[0];
		}
	}

	numa_node_cpu = p.numa_node_cpu;
	if (numa_node_cpu != -1) {
		if (numa_run_on_node(numa_node_cpu) == -1) {
			perror("numa_run_on_node");
			numa_node_cpu = -1;
		}
	}
#endif
}

static unsigned long count_host(unsigned long size, int t)
{
	unsigned long count = 0;

	omp_set_num_threads(t);
#pragma omp parallel for reduction(+ : count)
	for (unsigned long my = 0; my < size; my++) {
		if (!pred(A[my])) {
			count++;
		}
	}
	return count;
}

void usage()
{
	fprintf(stderr,
	    "\nUsage:  ./program [options]"
	    "\n"
	    "\nGeneral options:"
	    "\n    -h        help"
	    "\n    -t <T>    # of threads (default=4)"
	    "\n    -w <W>    # of untimed warmup iterations (default=1)"
	    "\n    -e <E>    # of timed repetition iterations (default=3)"
	    "\n"
	    "\nBenchmark-specific options:"
	    "\n    -i <I>    input size (default=2^28 elements)"
	    "\n");
}

void input_params(int argc, char** argv)
{
	p.input_size = 1 << 28;
	p.n_warmup = 1;
	p.n_reps = 3;
	p.n_threads = 4;
#if NUMA
	p.bitmask_in = NULL;
	p.numa_node_cpu = -1;
#endif

	int opt;
	while ((opt = getopt(argc, argv, "hi:w:e:t:A:C:")) >= 0) {
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
	assert(p.n_threads > 0);
}

/**
 * @brief Main of the Host Application.
 */
int main(int argc, char** argv)
{

	input_params(argc, argv);

	const unsigned long input_size = p.input_size;

	// Create a column with arbitrary data
	fill_column(input_size);

#if DFATOOL_TIMING
	Timer timer;
#endif

#if NOP_SYNC
	for (int rep = 0; rep < 200000; rep++) {
		asm volatile("nop" ::);
	}
#endif

	for (int rep = 0; rep < p.n_warmup + p.n_reps; rep++) {
		if (rep >= p.n_warmup) {
			perf_start();
			start(&timer, 0, 0);
		}
		total_count = count_host(input_size, p.n_threads);
		if (rep >= p.n_warmup) {
			stop(&timer, 0);
			perf_stop();
		}

		unsigned int nr_threads = 0;
#pragma omp parallel
#pragma omp atomic
		nr_threads++;

		if (rep >= p.n_warmup) {
#if WITH_PERF_LIB
			printf("[::] COUNT-CPU | n_threads=%d e_type=%s n_elements=%lu"
#if NUMA
			       " numa_node_in=%d numa_node_cpu=%d numa_distance_in_cpu=%d"
#endif
			       " |",
			    nr_threads, XSTR(T), input_size
#if NUMA
			    ,
			    numa_node_in, numa_node_cpu,
			    numa_distance(numa_node_in, numa_node_cpu)
#endif
			);
			perf_print();
#elif DFATOOL_TIMING
			printf("[::] COUNT-CPU | n_threads=%d e_type=%s n_elements=%lu"
#if NUMA
			       " numa_node_in=%d numa_node_cpu=%d numa_distance_in_cpu=%d"
#endif
			       " | throughput_MBps=%f",
			    nr_threads, XSTR(T), input_size,
#if NUMA
			    numa_node_in, numa_node_cpu,
			    numa_distance(numa_node_in, numa_node_cpu),
#endif
			    input_size * 2 * sizeof(T) / timer.time[0]);
			printf(" throughput_MOpps=%f",
			    input_size / timer.time[0]);
			printf(" latency_us=%f\n",
			    timer.time[0]);
#endif // DFATOOL_TIMING
		}
	}

#if NOP_SYNC
	for (int rep = 0; rep < 200000; rep++) {
		asm volatile("nop" ::);
	}
#endif

#if NUMA
	numa_free(A, input_size * sizeof(T));
#else
	free(A);
#endif
	return 0;
}
