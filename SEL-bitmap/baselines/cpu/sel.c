/*
 * Copyright 2026 Birte Kristina Friesel <birte.friesel@uni-osnabrueck.de>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <getopt.h>
#include <omp.h>
#include <popcntintrin.h>
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

typedef struct Params {
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

static T* in_data;
static uint32_t* out_bitmap;
unsigned long long n_elements;

bool pred(const T x)
{
	return (x % 2) == 0;
}

void fill_db()
{
	in_data = (T*)numa_bind_alloc(n_elements * sizeof(T), p.bitmask_in);

	out_bitmap = (uint32_t*)numa_bind_alloc(n_elements / 32 * sizeof(uint32_t) + sizeof(uint32_t), p.bitmask_out);

#if NUMA
	numa_node_cpu = numa_cpu_bind(p.numa_node_cpu);
#endif

	for (unsigned long long i = 0; i < n_elements; i++) {
		in_data[i] = i + 1;
	}
}

static void host_select()
{
	memset(out_bitmap, 0, n_elements * sizeof(uint32_t) / 32 + sizeof(uint32_t));

#pragma omp parallel for
	for (unsigned long i = 0; i < n_elements / 32; i++) {
		for (unsigned int j = 0; j < 32; j++) {
			out_bitmap[i] |= (pred(in_data[i * 32 + j]) * 1) << j;
		}
	}

	for (unsigned int j = 0; j < n_elements % 32; j++) {
		out_bitmap[n_elements / 32] |= (pred(in_data[(n_elements / 32) * 32 + j]) * 1) << j;
	}
}

static unsigned long long count_bits()
{
	unsigned long count = 0;

#pragma omp parallel for reduction(+ : count)
	for (unsigned int i = 0; i < n_elements / 32; i++) {
		count += _mm_popcnt_u32(out_bitmap[i]);
	}

	for (unsigned int j = 0; j < n_elements % 32; j++) {
		if (out_bitmap[n_elements / 32] & (1 << j)) {
			count += 1;
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
	    "\n	-h		help"
	    "\n	-t <T>	# of threads (default=4)"
	    "\n	-w <W>	# of untimed warmup iterations (default=1)"
	    "\n	-e <E>	# of timed repetition iterations (default=5)"
	    "\n"
	    "\nBenchmark-specific options:"
	    "\n	-i <I>	input size (default=2²⁸ elements)"
	    "\n");
}

void parse_params(int argc, char** argv)
{
	p.input_size = 1 << 28;
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
			fprintf(stderr, "\nUnrecognized option!\n");
			usage();
			exit(0);
		}
	}
}

int main(int argc, char** argv)
{
	parse_params(argc, argv);
	omp_set_num_threads(p.n_threads);
	n_elements = p.input_size;

	fill_db();

	for (int rep = 0; rep < p.n_warmup + p.n_reps; rep++) {
		perf_start();
		start(&timer, 0, 0);
		host_select();
		stop(&timer, 0);
		perf_stop();

		if (count_bits() != n_elements / 2) {
			printf("Error: Expected host_select to find %llu elements, got %llu elements instead\n",
			    n_elements / 2,
			    count_bits());
			break;
		}

#if NUMA
		numa_node_in = numa_get_node_of_page(in_data, "in_data");
		numa_node_out = numa_get_node_of_page(out_bitmap, "out_bitmap");
#endif

		unsigned int nr_threads = 0;
#pragma omp parallel
#pragma omp atomic
		nr_threads++;

		if (rep >= p.n_warmup) {
#if WITH_PERF_LIB
			printf("[::] SEL-CPU | n_threads=%d e_type=%s n_elements=%llu",
			    nr_threads, XSTR(T), n_elements);
#if NUMA
			printf(" numa_node_in=%d numa_node_out=%d numa_node_cpu=%d numa_distance_in_cpu=%d numa_distance_cpu_out=%d",
			    numa_node_in, numa_node_out, numa_node_cpu, numa_distance(numa_node_in, numa_node_cpu), numa_distance(numa_node_cpu, numa_node_out));
#endif
			printf(" |");
			perf_print();
#elif DFATOOL_TIMING
			printf("[::] SEL-CPU | n_threads=%d e_type=%s n_elements=%llu",
			    nr_threads, XSTR(T), n_elements);
#if NUMA
			printf(" numa_node_in=%d numa_node_out=%d numa_node_cpu=%d numa_distance_in_cpu=%d numa_distance_cpu_out=%d",
			    numa_node_in, numa_node_out, numa_node_cpu, numa_distance(numa_node_in, numa_node_cpu), numa_distance(numa_node_cpu, numa_node_out));
#endif
			printf(" | latency_select_us=%f throughput_MOpps=%f throughput_MBps=%f\n",
			    timer.time[0], n_elements / timer.time[0],
			    n_elements * sizeof(T) / timer.time[0]);
#endif
		}
	}

	numa_free(in_data, n_elements * sizeof(T));
	numa_free(out_bitmap, n_elements / 32 * sizeof(uint32_t) + sizeof(uint32_t));
	return 0;
}
