/*
 * Copyright 2026 Birte Kristina Friesel <birte.friesel@uni-osnabrueck.de>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <unistd.h>
#include <getopt.h>
#include <stdint.h>
#include <omp.h>
#include <popcntintrin.h>
#include "../../include/timer.h"

#if NUMA
#include <numaif.h>
#include <numa.h>

void* mp_pages[1];
int mp_status[1];
int mp_nodes[1];
int numa_node_in = -1;
int numa_node_out = -1;
int numa_node_cpu = -1;
#endif

#define XSTR(x) STR(x)
#define STR(x) #x

#ifndef T
#define T uint64_t
#endif

typedef struct Params {
	unsigned long long   input_size;
	int   n_warmup;
	int   n_reps;
	int   n_threads;
#if NUMA
	struct bitmask* bitmask_in;
	struct bitmask* bitmask_out;
	int numa_node_cpu;
#endif
} Params;

struct Params p;

static T *in_data;
static uint32_t *out_bitmap;
unsigned long long n_elements;

bool pred(const T x) {
	return (x % 2) == 0;
}

void fill_db() {
#if NUMA
	if (p.bitmask_in) {
		numa_set_membind(p.bitmask_in);
		numa_free_nodemask(p.bitmask_in);
	}
	in_data = (T*) numa_alloc(n_elements * sizeof(T));
#else
	in_data = (T*) malloc(n_elements * sizeof(T));
#endif

#if NUMA
	if (p.bitmask_out) {
		numa_set_membind(p.bitmask_out);
		numa_free_nodemask(p.bitmask_out);
	}
	out_bitmap = (uint32_t*) numa_alloc(n_elements / 32 * sizeof(uint32_t) + sizeof(uint32_t));
#else
	out_bitmap = (uint32_t*) malloc(n_elements / 32 * sizeof(uint32_t) + sizeof(uint32_t));
#endif

#if NUMA
	struct bitmask *bitmask_all = numa_allocate_nodemask();
	numa_bitmask_setall(bitmask_all);
	numa_set_membind(bitmask_all);
	numa_free_nodemask(bitmask_all);
#endif

	for (unsigned long long i = 0; i < n_elements; i++) {
		in_data[i] = i+1;
	}

#if NUMA
	mp_pages[0] = in_data;
	if (move_pages(0, 1, mp_pages, NULL, mp_status, 0) == -1) {
		perror("move_pages(in_data)");
	}
	else if (mp_status[0] < 0) {
		printf("move_pages error: %d", mp_status[0]);
	}
	else {
		numa_node_in = mp_status[0];
	}

	mp_pages[0] = out_bitmap;
	if (move_pages(0, 1, mp_pages, NULL, mp_status, 0) == -1) {
		perror("move_pages(out_bitmap)");
	}
	else if (mp_status[0] < 0) {
		printf("move_pages error: %d", mp_status[0]);
	}
	else {
		numa_node_out = mp_status[0];
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

static void host_select() {
	memset(out_bitmap, 0, n_elements * sizeof(uint32_t) / 32 + sizeof(uint32_t));

	#pragma omp parallel for
	for (unsigned long i = 0; i < n_elements / 32; i++) {
		for (unsigned int j = 0; j < 32; j++) {
			out_bitmap[i] |= (pred(in_data[i*32+j]) * 1) << j;
		}
	}

	for (unsigned int j = 0; j < n_elements % 32; j++) {
		out_bitmap[n_elements/32] |= (pred(in_data[(n_elements/32)*32 + j]) * 1) << j;
	}

}

static unsigned long long count_bits()
{
	unsigned long count = 0;

	#pragma omp parallel for reduction(+:count)
	for (unsigned int i = 0; i < n_elements/32; i++) {
		count += _mm_popcnt_u32(out_bitmap[i]);
	}

	for (unsigned int j = 0; j < n_elements % 32; j++) {
		if (out_bitmap[n_elements/32] & (1<<j)) {
			count += 1;
		}
	}

	return count;
}


void usage() {
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

void parse_params(int argc, char **argv) {
	p.input_size = 1 << 28;
	p.n_warmup   = 1;
	p.n_reps     = 5;
	p.n_threads  = 4;
#if NUMA
	p.bitmask_in    = NULL;
	p.bitmask_out   = NULL;
	p.numa_node_cpu = -1;
#endif

	int opt;
	while((opt = getopt(argc, argv, "hi:w:e:t:a:b:c:")) >= 0) {
		switch(opt) {
		case 'h':
		usage();
		exit(0);
		break;
		case 'i': p.input_size	= atoll(optarg); break;
		case 'w': p.n_warmup	  = atoi(optarg); break;
		case 'e': p.n_reps		= atoi(optarg); break;
		case 't': p.n_threads	 = atoi(optarg); break;
#if NUMA
		case 'a': p.bitmask_in	= numa_parse_nodestring(optarg); break;
		case 'b': p.bitmask_out   = numa_parse_nodestring(optarg); break;
		case 'c': p.numa_node_cpu = atoi(optarg); break;
#endif
		default:
			fprintf(stderr, "\nUnrecognized option!\n");
			usage();
			exit(0);
		}
	}
}

int main(int argc, char **argv) {
	parse_params(argc, argv);
	omp_set_num_threads(p.n_threads);
	n_elements = p.input_size;

	fill_db();

	Timer timer;

	for(int rep = 0; rep < p.n_warmup + p.n_reps; rep++) {
		start(&timer, 0, 0);
		host_select();
		stop(&timer, 0);

		if (count_bits() != n_elements / 2) {
			printf("Error: Expected host_select to find %llu elements, got %llu elements instead\n",
					n_elements / 2,
					count_bits()
			);
			break;
		}

		unsigned int nr_threads = 0;
#pragma omp parallel
#pragma omp atomic
		nr_threads++;

#if DFATOOL_TIMING
		if (rep >= p.n_warmup) {
			printf("[::] SEL-CPU | n_threads=%d e_type=%s n_elements=%llu"
#if NUMA
				" numa_node_in=%d numa_node_out=%d numa_node_cpu=%d numa_distance_in_cpu=%d numa_distance_cpu_out=%d"
#endif
				" | latency_select_us=%f throughput_MOpps=%f throughput_MBps=%f\n",
				nr_threads, XSTR(T), n_elements,
#if NUMA
				numa_node_in, numa_node_out, numa_node_cpu, numa_distance(numa_node_in, numa_node_cpu), numa_distance(numa_node_cpu, numa_node_out),
#endif
				timer.time[0], n_elements / timer.time[0],
				n_elements * sizeof(T) / timer.time[0]);
		}
	}
#endif

#if NUMA
	numa_free(in_data, n_elements * sizeof(T));
	numa_free(out_bitmap, n_elements / 32 * sizeof(uint32_t) + sizeof(uint32_t));
#else
	free(in_data);
	free(out_bitmap);
#endif
	return 0;
}
