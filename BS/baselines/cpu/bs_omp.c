#include <assert.h>
#include <getopt.h>
#include <omp.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
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

#define DTYPE uint64_t

typedef struct Params {
	unsigned long input_size;
	unsigned long n_queries;
	int n_warmup;
	int n_reps;
	int n_threads;
#if NUMA
	struct bitmask* bitmask_in;
	int numa_node_cpu;
#endif
} Params;

struct Params p;

volatile DTYPE result_host = -1;

void input_params(int argc, char** argv)
{
	p.input_size = 262144;
	p.n_queries = 16777216;
	p.n_warmup = 1;
	p.n_reps = 3;
	p.n_threads = 4;
#if NUMA
	p.bitmask_in = NULL;
	p.numa_node_cpu = -1;
#endif

	int opt;
	while ((opt = getopt(argc, argv, "i:q:w:e:t:A:C:")) >= 0) {
		switch (opt) {
		case 'i':
			p.input_size = atol(optarg);
			break;
		case 'q':
			p.n_queries = atol(optarg);
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
			exit(0);
		}
	}
	assert(p.n_threads > 0);
}

void create_test_file(DTYPE* input, uint64_t nr_elements, DTYPE* queries,
    uint64_t n_queries)
{

	srand(time(NULL));

	input[0] = 1;
	for (uint64_t i = 1; i < nr_elements; i++) {
		input[i] = input[i - 1] + (rand() % 10) + 1;
	}

	for (uint64_t i = 0; i < n_queries; i++) {
		queries[i] = input[rand() % (nr_elements - 2)];
	}
}

uint64_t binarySearch(DTYPE* input, uint64_t input_size, DTYPE* queries,
    unsigned n_queries)
{

	uint64_t found = -1;
	uint64_t q, r, l, m;

#pragma omp parallel for private(q, r, l, m)
	for (q = 0; q < n_queries; q++) {
		l = 0;
		r = input_size;
		while (l <= r) {
			m = l + (r - l) / 2;

			// Check if x is present at mid
			if (input[m] == queries[q]) {
				found += m;
				break;
			}
			// If x greater, ignore left half
			if (input[m] < queries[q])
				l = m + 1;

			// If x is smaller, ignore right half
			else
				r = m - 1;
		}
	}

	return found;
}

int main(int argc, char** argv)
{

	input_params(argc, argv);

#if NUMA
	numa_node_cpu = numa_cpu_bind(p.numa_node_cpu);
#endif

	omp_set_num_threads(p.n_threads);

	DTYPE* input = numa_bind_alloc(p.input_size * sizeof(DTYPE), p.bitmask_in);
	DTYPE* queries = numa_bind_alloc(p.n_queries * sizeof(DTYPE), p.bitmask_in);

	// Create an input file with arbitrary data.
	create_test_file(input, p.input_size, queries, p.n_queries);

#if NUMA
	numa_node_in = numa_get_node_of_page(input, "input");
#endif

#if NOP_SYNC
	for (int rep = 0; rep < 200000; rep++) {
		asm volatile("nop" ::);
	}
#endif

	for (int rep = 0; rep < p.n_warmup + p.n_reps; rep++) {
		perf_start();
		start(&timer, 0, 0);
		result_host = binarySearch(input, p.input_size - 1, queries, p.n_queries);
		stop(&timer, 0);
		perf_stop();

#if NOP_SYNC
		for (int rep = 0; rep < 200000; rep++) {
			asm volatile("nop" ::);
		}
#endif

		int status = (result_host);
		unsigned int nr_threads = 0;
#pragma omp parallel
#pragma omp atomic
		nr_threads++;

		if (status) {
			if (rep >= p.n_warmup) {
#if WITH_PERF_LIB
				printf("[::] binarySearch | n_threads=%d e_type=%s n_elements=%lu"
				       " |",
				    nr_threads, "uint64_t", p.input_size);
#if NUMA
				printf(" numa_node_in=%d numa_node_cpu=%d numa_distance_in_cpu=%d", numa_node_in, numa_node_cpu, numa_distance(numa_node_in, numa_node_cpu));
#endif
				printf(" |");
				perf_print();
#elif DFATOOL_TIMING
				printf("[::] BS-CPU | n_threads=%d e_type=%s n_elements=%lu",
				    nr_threads, "uint64_t", p.input_size);
#if NUMA
				printf(" numa_node_in=%d numa_node_cpu=%d numa_distance_in_cpu=%d", numa_node_in, numa_node_cpu, numa_distance(numa_node_in, numa_node_cpu));
#endif
				printf(" | throughput_MBps=%f",
				    p.n_queries * sizeof(DTYPE) / timer.time[0]);
				printf(" throughput_MOpps=%f latency_us=%f\n",
				    p.n_queries / timer.time[0], timer.time[0]);
#endif // DFATOOL_TIMING
			}
		} else {
			printf("[ERROR]\n");
			return 1;
		}
	}

	numa_free(input, p.input_size * sizeof(DTYPE));
	numa_free(queries, p.n_queries * sizeof(DTYPE));

	return 0;
}
