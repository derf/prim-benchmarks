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
#define T uint64_t
#endif

volatile unsigned long long total_count;

// Params ---------------------------------------------------------------------
typedef struct Params {
	char* dpu_type;
	unsigned long long input_size;
	int n_warmup;
	int n_reps;
	int n_threads;
#if NUMA
	struct bitmask* bitmask_in;
	struct bitmask* bitmask_out;
	int numa_node_cpu;
#endif
} Params;

struct Params p;

static T* A;
static T* C;
static unsigned long long pos;

bool pred(const T x)
{
	return (x % 2) == 0;
}

void create_test_file(unsigned int nr_elements)
{
	// srand(0);

	A = (T*)numa_bind_alloc(nr_elements * sizeof(T), p.bitmask_in);
	C = (T*)numa_bind_alloc(nr_elements * sizeof(T), p.bitmask_out);

	for (unsigned int i = 0; i < nr_elements; i++) {
		// A[i] = (unsigned int) (rand());
		A[i] = i + 1;
	}
}

/**
 * @brief compute output in the host
 */
static unsigned long long select_host(unsigned long long size, int t)
{
	pos = 0;
	C[pos] = A[pos];

	omp_set_num_threads(t);
#pragma omp parallel for
	for (unsigned long long my = 1; my < size; my++) {
		if (!pred(A[my])) {
			int p;
#pragma omp atomic update
			pos++;
			p = pos;
			C[p] = A[my];
		}
	}
	return pos;
}

void usage()
{
	fprintf(stderr,
	    "\nUsage:  ./program [options]"
	    "\n"
	    "\nGeneral options:"
	    "\n    -h        help"
	    "\n    -d <D>    DPU type (default=fsim)"
	    "\n    -t <T>    # of threads (default=4)"
	    "\n    -w <W>    # of untimed warmup iterations (default=1)"
	    "\n    -e <E>    # of timed repetition iterations (default=3)"
	    "\n"
	    "\nBenchmark-specific options:"
	    "\n    -i <I>    input size (default=8M elements)"
	    "\n");
}

void input_params(int argc, char** argv)
{
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
	while ((opt = getopt(argc, argv, "hi:w:e:t:A:B:C:")) >= 0) {
		switch (opt) {
		case 'h':
			usage();
			exit(0);
			break;
		case 'i':
			p.input_size = atoll(optarg);
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
			fprintf(stderr, "\nUnrecognized option: %c\n", opt);
			usage();
			exit(0);
		}
	}
	assert(p.n_threads > 0 && "Invalid # of ranks!");
}

int main(int argc, char** argv)
{

	input_params(argc, argv);

	const unsigned long long file_size = p.input_size;

#if NUMA
	numa_node_cpu = numa_cpu_bind(p.numa_node_cpu);
#endif

	create_test_file(file_size);

	for (int rep = 0; rep < p.n_warmup + p.n_reps; rep++) {
		perf_start();
		start(&timer, 0, 0);
		total_count = select_host(file_size, p.n_threads);
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
			printf("[::] SEL-CPU | n_threads=%d e_type=%s n_elements=%llu", nr_threads, XSTR(T), file_size);
#if NUMA
			printf(" numa_node_in=%d numa_node_out=%d numa_node_cpu=%d numa_distance_in_cpu=%d numa_distance_cpu_out=%d",
			    numa_node_in, numa_node_out, numa_node_cpu, numa_distance(numa_node_in, numa_node_cpu), numa_distance(numa_node_cpu, numa_node_out));
#endif
			printf(" |");
			perf_print();
#elif DFATOOL_TIMING
			printf("[::] SEL-CPU | n_threads=%d e_type=%s n_elements=%llu", nr_threads, XSTR(T), file_size);
#if NUMA
			printf(" numa_node_in=%d numa_node_out=%d numa_node_cpu=%d numa_distance_in_cpu=%d numa_distance_cpu_out=%d",
			    numa_node_in, numa_node_out, numa_node_cpu, numa_distance(numa_node_in, numa_node_cpu), numa_distance(numa_node_cpu, numa_node_out));
#endif
			printf(" | throughput_MBps=%f",
			    file_size * 2 * sizeof(T) / timer.time[0]);
			printf(" throughput_MOpps=%f latency_kernel_us=%f\n",
			    file_size / timer.time[0], timer.time[0]);
#endif
		}
	}

	numa_free(A, file_size * sizeof(T));
	numa_free(C, file_size * sizeof(T));

	return 0;
}
