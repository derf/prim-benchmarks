/**
 * @file app.c
 * @brief Template for a Host Application Source File.
 *
 */
#include "../../include/common.h"
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

// weights - bitmask_in
T** A;

// input/output - bitmask_out
T* B;

// intermediate - bitmask_out
T* C;

// Create input arrays
static void init_data(T** A, unsigned int m_size, unsigned int n_size)
{
	for (unsigned int l = 0; l < NUM_LAYERS; l++) {
		for (unsigned int i = 0; i < m_size * n_size; i++) {
			if (i % 100 < 98) {
				A[l][i] = 0;
			} else {
				A[l][i] = (l + i) % 2;
			}
		}
	}
}

static void init_B(T* B, unsigned int n_size)
{
	for (unsigned int i = 0; i < n_size; i++) {
		if (i % 50 < 48) {
			B[i] = 0;
		} else {
			B[i] = i % 2;
		}
	}
}

// Compute output in the host
static void mlp_host(T* C, T** A, T* B, unsigned int m_size,
    unsigned int n_size)
{
	for (unsigned int nl = 0; nl < NUM_LAYERS; nl++) {
		for (unsigned int m = 0; m < m_size; m++) {
			C[m] = 0;
		}
#pragma omp parallel for
		for (unsigned int m = 0; m < m_size; m++) {
			for (unsigned int n = 0; n < n_size; n++) {
				C[m] += A[nl][m * n_size + n] * B[n];
			}
			C[m] = max(0, C[m]);
		}
		for (unsigned int n = 0; n < n_size; n++) {
			B[n] = C[n];
		}
	}
}

// Params ---------------------------------------------------------------------
typedef struct Params {
	int input_size_n;
	int input_size_m;
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
	fprintf(stderr, "\nUsage:  ./program [options]"
	                "\n");
}

struct Params input_params(int argc, char** argv)
{
	struct Params p;
	p.input_size_n = 8192;
	p.input_size_m = 20480;
	p.n_warmup = 1;
	p.n_reps = 3;
	p.n_threads = 4;
#if NUMA
	p.bitmask_in = NULL;
	p.bitmask_out = NULL;
	p.numa_node_cpu = -1;
#endif

	int opt;
	while ((opt = getopt(argc, argv, "e:n:m:t:w:A:B:C:")) >= 0) {
		switch (opt) {
		case 'h':
			usage();
			exit(0);
			break;
		case 'e':
			p.n_reps = atoi(optarg);
			break;
		case 'n':
			p.input_size_n = atoi(optarg);
			break;
		case 'm':
			p.input_size_m = atoi(optarg);
			break;
		case 't':
			p.n_threads = atoi(optarg);
			break;
		case 'w':
			p.n_warmup = atoi(optarg);
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

	return p;
}

/**
 * @brief Main of the Host Application.
 */
int main(int argc, char** argv)
{

	struct Params p = input_params(argc, argv);
	uint64_t n_size = p.input_size_n;
	uint64_t m_size = p.input_size_m;

	omp_set_num_threads(p.n_threads);

	A = numa_bind_alloc(NUM_LAYERS * sizeof(T*), p.bitmask_in);
	for (int l = 0; l < NUM_LAYERS; l++) {
		A[l] = numa_bind_alloc(n_size * m_size * sizeof(unsigned int), p.bitmask_in);
	}
	B = numa_bind_alloc(m_size * sizeof(unsigned int), p.bitmask_out);
	C = numa_bind_alloc(m_size * sizeof(unsigned int), p.bitmask_out);

#if NUMA
	numa_node_cpu = numa_cpu_bind(p.numa_node_cpu);
#endif

	// Create an input file with arbitrary data.
	init_data(A, m_size, n_size);

	for (int i = 0; i < p.n_warmup + p.n_reps; i++) {
		init_B(B, n_size);

		perf_start();
		start(&timer, 0, 0);
		mlp_host(C, A, B, n_size, m_size);
		stop(&timer, 0);
		perf_stop();

		unsigned int nr_threads = 0;
#pragma omp parallel
#pragma omp atomic
		nr_threads++;

#if NUMA
		numa_node_in = numa_get_node_of_page(A, "A");
		numa_node_out = numa_get_node_of_page(B, "B");
#endif

		if (i >= p.n_warmup) {
#if WITH_PERF_LIB
			printf("[::] MLP-CPU | n_threads=%d e_type=%s n_elements=%lu",
			    nr_threads, XSTR(T), n_size * m_size);
#if NUMA
			printf(" numa_node_in=%d numa_node_cpu=%d numa_node_out=%d"
			       " numa_distance_in_cpu=%d numa_distance_cpu_out=%d",
			    numa_node_in, numa_node_cpu, numa_node_out,
			    numa_distance(numa_node_in, numa_node_cpu),
			    numa_distance(numa_node_cpu, numa_node_out));
#endif
			printf(" |");
			perf_print();
#elif DFATOOL_TIMING
			printf("[::] MLP-CPU | n_threads=%d e_type=%s n_elements=%lu",
			    nr_threads, XSTR(T), n_size * m_size);
#if NUMA
			printf(" numa_node_in=%d numa_node_cpu=%d numa_node_out=%d"
			       " numa_distance_in_cpu=%d numa_distance_cpu_out=%d",
			    numa_node_in, numa_node_cpu, numa_node_out,
			    numa_distance(numa_node_in, numa_node_cpu),
			    numa_distance(numa_node_cpu, numa_node_out));
#endif
			printf(" | throughput_MBps=%f throughput_MOpps=%f",
			    n_size * m_size * sizeof(T) / timer.time[0],
			    n_size * m_size / timer.time[0]);
			printf(" latency_us=%f\n", timer.time[0]);
#endif // DFATOOL_TIMING
		}
	}

#if NOP_SYNC
	for (int rep = 0; rep < 200000; rep++) {
		asm volatile("nop" ::);
	}
#endif

	for (int l = 0; l < NUM_LAYERS; l++) {
		numa_free(A[l], n_size * m_size * sizeof(unsigned int));
	}
	numa_free(A, NUM_LAYERS * sizeof(T*));
	numa_free(B, m_size * sizeof(unsigned int));
	numa_free(C, m_size * sizeof(unsigned int));

	return 0;
}
