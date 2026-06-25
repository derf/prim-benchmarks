#include <assert.h>
#include <getopt.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <omp.h>

#if DFATOOL_TIMING
#include "../../include/timer.h"
Timer timer;
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
#include "../../../include/numa.h"
#else
#define numa_bind_alloc(size, bitmask) malloc(size)
#define numa_free(data, size) free(data)
#endif

#define XSTR(x) STR(x)
#define STR(x) #x

#ifndef T
#define T int64_t
#endif

volatile unsigned long total_count;
static unsigned long pos;

static T* A;
static T* C;

// Compute output in the host
static unsigned long unique_host(unsigned long size)
{
	pos = 0;
	C[pos] = A[pos];

#pragma omp parallel for
	for (unsigned long my = 1; my < size; my++) {
		if (A[my] != A[my - 1]) {
			int p;
#pragma omp atomic update
			pos++;
			p = pos;
			C[p] = A[my];
		}
	}

	return pos;
}

// Params
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
	    "\n    -t <T>    # of threads (default=8)"
	    "\n    -w <W>    # of untimed warmup iterations (default=1)"
	    "\n    -e <E>    # of timed repetition iterations (default=3)"
	    "\n"
	    "\nBenchmark-specific options:"
	    "\n    -i <I>    input size (default=8M elements)"
	    "\n");
}

struct Params input_params(int argc, char** argv)
{
	struct Params p;
	p.input_size = 16 << 20;
	p.n_warmup = 1;
	p.n_reps = 3;
	p.n_threads = 4;
#if NUMA
	p.bitmask_in = NULL;
	p.bitmask_out = NULL;
	p.numa_node_cpu = -1;
#endif

	int opt;
	while ((opt = getopt(argc, argv, "hd:i:w:e:t:A:B:C:")) >= 0) {
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
#endif // NUMA
		default:
			fprintf(stderr, "\nUnrecognized option!\n");
			usage();
			exit(0);
		}
	}
	assert(p.n_threads > 0 && "Invalid # of ranks!");

	return p;
}

// Main
int main(int argc, char** argv)
{

	struct Params p = input_params(argc, argv);

	omp_set_num_threads(p.n_threads);

	printf("Allocating arrays for A -> C: %.3f GiB per array\n",
	    (double)p.input_size * sizeof(T) / (1 << 30));

	A = (T*)numa_bind_alloc(p.input_size * sizeof(T), p.bitmask_in);
	C = (T*)numa_bind_alloc(p.input_size * sizeof(T), p.bitmask_out);

#if NUMA
	numa_node_cpu = numa_cpu_bind(p.numa_node_cpu);
#endif

#pragma omp parallel for
	for (unsigned long i = 0; i < p.input_size; i++) {
		A[i] = i % 2 == 0 ? i : i + 1;
	}

	for (int rep = 0; rep < p.n_warmup + p.n_reps; rep++) {
		perf_start();
		start(&timer, 0, 0);
		total_count = unique_host(p.input_size);
		stop(&timer, 0);
		perf_stop();

#if NUMA
		numa_node_in = numa_get_node_of_page(A, "A");
		numa_node_out = numa_get_node_of_page(C, "C");
#endif

		unsigned int nr_threads = 0;
#pragma omp parallel
#pragma omp atomic
		nr_threads++;

		if (rep >= p.n_warmup) {
#if WITH_PERF_LIB
			printf("[::] UNI-CPU | n_threads=%d e_type=%s n_elements=%lu",
			    nr_threads, XSTR(T), p.input_size);
#if NUMA
			printf(" numa_node_in=%d numa_node_out=%d numa_node_cpu=%d numa_distance_in_cpu=%d numa_distance_cpu_out=%d",
			    numa_node_in, numa_node_out, numa_node_cpu,
			    numa_distance(numa_node_in, numa_node_cpu),
			    numa_distance(numa_node_cpu, numa_node_out));
#endif
			printf(" |");
			perf_print();
#elif DFATOOL_TIMING
			printf("[::] UNI-CPU | n_threads=%d e_type=%s n_elements=%lu",
			    nr_threads, XSTR(T), p.input_size);
#if NUMA
			printf(" numa_node_in=%d numa_node_out=%d numa_node_cpu=%d numa_distance_in_cpu=%d numa_distance_cpu_out=%d",
			    numa_node_in, numa_node_out, numa_node_cpu,
			    numa_distance(numa_node_in, numa_node_cpu),
			    numa_distance(numa_node_cpu, numa_node_out));
#endif
			printf(" | throughput_MBps=%f throughput_MOpps=%f latency_us=%f\n",
			    p.input_size * sizeof(T) / timer.time[0],
			    p.input_size / timer.time[0],
			    timer.time[0]);
#endif
		}
	}

	numa_free(A, p.input_size * sizeof(T));
	numa_free(C, p.input_size * sizeof(T));
	return 0;
}
