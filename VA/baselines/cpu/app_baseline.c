/**
* @file app.c
* @brief Template for a Host Application Source File.
*
*/
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <unistd.h>
#include <getopt.h>
#include <assert.h>
#include <stdint.h>

#include <omp.h>

#if WITH_BENCHMARK
#include "../../support/timer.h"
#else
#define start(...)
#define stop(...)
#endif

#if NUMA
#include <numaif.h>
#include <numa.h>

void *mp_pages[1];
int mp_status[1];
int mp_nodes[1];
int numa_node_in = -1;
int numa_node_out = -1;
int numa_node_cpu = -1;
#endif

#define XSTR(x) STR(x)
#define STR(x) #x

#ifndef T
#define T int32_t
#endif

static T *A;
static T *B;
static T *C;

#if NUMA_MEMCPY
int numa_node_cpu_memcpy = -1;
int numa_node_local = -1;
int numa_node_in_is_local = 0;
static T *A_local;
static T *B_local;
#endif

/**
* @brief compute output in the host
*/
static void vector_addition_host(unsigned long nr_elements, int t)
{
	omp_set_num_threads(t);
#pragma omp parallel for
	for (long i = 0; i < nr_elements; i++) {
#if NUMA_MEMCPY
		C[i] = A_local[i] + B_local[i];
#else
		C[i] = A[i] + B[i];
#endif
	}
}

// Params ---------------------------------------------------------------------
typedef struct Params {
	long input_size;
	int n_warmup;
	int n_reps;
	int exp;
	int n_threads;
#if NUMA
	struct bitmask *bitmask_in;
	struct bitmask *bitmask_out;
	int numa_node_cpu;
#endif
#if NUMA_MEMCPY
	int numa_node_cpu_memcpy;
	struct bitmask *bitmask_cpu;
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
		"\n    -x <X>    Weak (0) or strong (1) scaling (default=0)"
		"\n"
		"\nBenchmark-specific options:"
		"\n    -i <I>    input size (default=8M elements)" "\n");
}

struct Params input_params(int argc, char **argv)
{
	struct Params p;
	p.input_size = 16777216;
	p.n_warmup = 1;
	p.n_reps = 3;
	p.exp = 1;
	p.n_threads = 5;
#if NUMA
	p.bitmask_in = NULL;
	p.bitmask_out = NULL;
	p.numa_node_cpu = -1;
#endif
#if NUMA_MEMCPY
	p.numa_node_cpu_memcpy = -1;
	p.bitmask_cpu = NULL;
#endif

	int opt;
	while ((opt = getopt(argc, argv, "hi:w:e:x:t:a:b:c:C:M:")) >= 0) {
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
		case 'x':
			p.exp = atoi(optarg);
			break;
		case 't':
			p.n_threads = atoi(optarg);
			break;
#if NUMA
		case 'a':
			p.bitmask_in = numa_parse_nodestring(optarg);
			break;
		case 'b':
			p.bitmask_out = numa_parse_nodestring(optarg);
			break;
		case 'c':
			p.numa_node_cpu = atoi(optarg);
			break;
#if NUMA_MEMCPY
		case 'C':
			p.bitmask_cpu = numa_parse_nodestring(optarg);
			break;
		case 'M':
			p.numa_node_cpu_memcpy = atoi(optarg);
			break;
#endif				// NUMA_MEMCPY
#endif				// NUMA
		default:
			fprintf(stderr, "\nUnrecognized option!\n");
			usage();
			exit(0);
		}
	}
	assert(p.n_threads > 0 && "Invalid # of ranks!");

	return p;
}

/**
* @brief Main of the Host Application.
*/
int main(int argc, char **argv)
{

	struct Params p = input_params(argc, argv);

	const unsigned long input_size =
	    p.exp == 0 ? p.input_size * p.n_threads : p.input_size;

	// Create an input file with arbitrary data.
    /**
    * @brief creates a "test file" by filling a buffer of 64MB with pseudo-random values
    * @param nr_elements how many 32-bit elements we want the file to be
    * @return the buffer address
    */
	srand(0);

#if NUMA
	if (p.bitmask_in) {
		numa_set_membind(p.bitmask_in);
		numa_free_nodemask(p.bitmask_in);
	}
	A = (T *) numa_alloc(input_size * sizeof(T));
	B = (T *) numa_alloc(input_size * sizeof(T));
#else
	A = (T *) malloc(input_size * sizeof(T));
	B = (T *) malloc(input_size * sizeof(T));
#endif

#if NUMA
	if (p.bitmask_out) {
		numa_set_membind(p.bitmask_out);
		numa_free_nodemask(p.bitmask_out);
	}
	C = (T *) numa_alloc(input_size * sizeof(T));
#else
	C = (T *) malloc(input_size * sizeof(T));
#endif

	for (unsigned long i = 0; i < input_size; i++) {
		A[i] = (T) (rand());
		B[i] = (T) (rand());
	}

#if NUMA
#if NUMA_MEMCPY
	if (p.bitmask_cpu) {
		numa_set_membind(p.bitmask_cpu);
		numa_free_nodemask(p.bitmask_cpu);
	}
#else
	struct bitmask *bitmask_all = numa_allocate_nodemask();
	numa_bitmask_setall(bitmask_all);
	numa_set_membind(bitmask_all);
	numa_free_nodemask(bitmask_all);
#endif				// NUMA_MEMCPY
#endif				// NUMA

#if NUMA
	mp_pages[0] = A;
	if (move_pages(0, 1, mp_pages, NULL, mp_status, 0) == -1) {
		perror("move_pages(A)");
	} else if (mp_status[0] < 0) {
		printf("move_pages error: %d", mp_status[0]);
	} else {
		numa_node_in = mp_status[0];
	}

	mp_pages[0] = C;
	if (move_pages(0, 1, mp_pages, NULL, mp_status, 0) == -1) {
		perror("move_pages(C)");
	} else if (mp_status[0] < 0) {
		printf("move_pages error: %d", mp_status[0]);
	} else {
		numa_node_out = mp_status[0];
	}

	numa_node_cpu = p.numa_node_cpu;
	if (p.numa_node_cpu != -1) {
		if (numa_run_on_node(p.numa_node_cpu) == -1) {
			perror("numa_run_on_node");
			numa_node_cpu = -1;
		}
	}
#endif

#if NUMA_MEMCPY
	numa_node_in_is_local = ((numa_node_cpu == numa_node_in)
				 || (numa_node_cpu + 8 == numa_node_in)) * 1;
#endif

#if WITH_BENCHMARK
	Timer timer;
#endif

#if NOP_SYNC
	for (int rep = 0; rep < 200000; rep++) {
		asm volatile ("nop"::);
	}
#endif

	for (int rep = 0; rep < p.n_warmup + p.n_reps; rep++) {

#if NUMA_MEMCPY
		numa_node_cpu_memcpy = p.numa_node_cpu_memcpy;
		start(&timer, 1, 0);
		if (!numa_node_in_is_local) {
			A_local = (T *) numa_alloc(input_size * sizeof(T));
			B_local = (T *) numa_alloc(input_size * sizeof(T));
		}
		stop(&timer, 1);
		if (!numa_node_in_is_local) {
			if (p.numa_node_cpu_memcpy != -1) {
				if (numa_run_on_node(p.numa_node_cpu_memcpy) ==
				    -1) {
					perror("numa_run_on_node");
					numa_node_cpu_memcpy = -1;
				}
			}
		}
		start(&timer, 2, 0);
		if (!numa_node_in_is_local) {
			memcpy(A_local, A, input_size * sizeof(T));
			memcpy(B_local, B, input_size * sizeof(T));
		} else {
			A_local = A;
			B_local = B;
		}
		stop(&timer, 2);
		if (p.numa_node_cpu != -1) {
			if (numa_run_on_node(p.numa_node_cpu) == -1) {
				perror("numa_run_on_node");
				numa_node_cpu = -1;
			}
		}
		mp_pages[0] = A_local;
		if (move_pages(0, 1, mp_pages, NULL, mp_status, 0) == -1) {
			perror("move_pages(A_local)");
		} else if (mp_status[0] < 0) {
			printf("move_pages error: %d", mp_status[0]);
		} else {
			numa_node_local = mp_status[0];
		}
#endif

		start(&timer, 0, 0);
		vector_addition_host(input_size, p.n_threads);
		stop(&timer, 0);

#if NUMA_MEMCPY
		start(&timer, 3, 0);
		if (!numa_node_in_is_local) {
			numa_free(A_local, input_size * sizeof(T));
			numa_free(B_local, input_size * sizeof(T));
		}
		stop(&timer, 3);
#endif

#if WITH_BENCHMARK
		unsigned int nr_threads = 0;
#pragma omp parallel
#pragma omp atomic
		nr_threads++;

		if (rep >= p.n_warmup) {
#if NUMA_MEMCPY
			printf
			    ("[::] VA-CPU-MEMCPY | n_threads=%d e_type=%s n_elements=%ld"
			     " numa_node_in=%d numa_node_local=%d numa_node_out=%d numa_node_cpu=%d numa_node_cpu_memcpy=%d numa_distance_in_cpu=%d numa_distance_cpu_out=%d"
			     " | throughput_MBps=%f", nr_threads, XSTR(T),
			     input_size, numa_node_in, numa_node_local,
			     numa_node_out, numa_node_cpu, numa_node_cpu_memcpy,
			     numa_distance(numa_node_in, numa_node_cpu),
			     numa_distance(numa_node_cpu, numa_node_out),
			     input_size * 3 * sizeof(T) / timer.time[0]);
			printf(" throughput_MOpps=%f",
			       input_size / timer.time[0]);
			printf
			    (" latency_kernel_us=%f latency_alloc_us=%f latency_memcpy_us=%f latency_free_us=%f latency_total_us=%f\n",
			     timer.time[0], timer.time[1], timer.time[2],
			     timer.time[3],
			     timer.time[0] + timer.time[1] + timer.time[2] +
			     timer.time[3]);
#else
			printf
			    ("[::] VA-CPU | n_threads=%d e_type=%s n_elements=%ld"
#if NUMA
			     " numa_node_in=%d numa_node_out=%d numa_node_cpu=%d numa_distance_in_cpu=%d numa_distance_cpu_out=%d"
#endif
			     " | throughput_MBps=%f",
			     nr_threads, XSTR(T), input_size,
#if NUMA
			     numa_node_in, numa_node_out, numa_node_cpu,
			     numa_distance(numa_node_in, numa_node_cpu),
			     numa_distance(numa_node_cpu, numa_node_out),
#endif
			     input_size * 3 * sizeof(T) / timer.time[0]);
			printf(" throughput_MOpps=%f",
			       input_size / timer.time[0]);
			printf(" latency_us=%f\n", timer.time[0]);
#endif				// NUMA_MEMCPY
		}
#endif				// WITH_BENCHMARK
	}

#if NOP_SYNC
	for (int rep = 0; rep < 200000; rep++) {
		asm volatile ("nop"::);
	}
#endif

#if NUMA
	numa_free(A, input_size * sizeof(T));
	numa_free(B, input_size * sizeof(T));
	numa_free(C, input_size * sizeof(T));
#else
	free(A);
	free(B);
	free(C);
#endif

	return 0;
}
