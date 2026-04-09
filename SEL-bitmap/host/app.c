/*
 * Copyright 2026 Birte Kristina Friesel <birte.friesel@uni-osnabrueck.de>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <dpu.h>
#include <dpu_log.h>
#include <unistd.h>
#include <getopt.h>
#include <assert.h>
#include <popcntintrin.h>

#include "common.h"
#include "timer.h"

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

#include <dpu_management.h>
#include <dpu_target_macros.h>

dpu_arguments_t* input_arguments;

typedef struct Params {
	unsigned long long   input_size;
	int   n_warmup;
	int   n_reps;
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

unsigned int n_elements_dpu, n_fill_dpu, first_filled_dpu;
struct dpu_set_t dpu_set, dpu;
uint32_t n_dpus, n_ranks;

Timer timer;

void fill_db() {
	printf("Allocating %llu GiB for input data \n", ((n_elements + n_fill_dpu) * sizeof(T)) >> 30);
#if NUMA
	if (p.bitmask_in) {
		numa_set_membind(p.bitmask_in);
		numa_free_nodemask(p.bitmask_in);
	}
	in_data = (T*) numa_alloc((n_elements + n_fill_dpu) * sizeof(T));
#else
	in_data = (T*) malloc((n_elements + n_fill_dpu) * sizeof(T));
#endif
	assert(in_data != NULL);

	printf("Allocating %llu MiB for output bitmap\n", ((n_elements + n_fill_dpu) / 32 * sizeof(uint32_t) + sizeof(uint32_t)) >> 20);
#if NUMA
	if (p.bitmask_out) {
		numa_set_membind(p.bitmask_out);
		numa_free_nodemask(p.bitmask_out);
	}
	out_bitmap = (uint32_t*) numa_alloc((n_elements + n_fill_dpu) / 32 * sizeof(uint32_t) + sizeof(uint32_t));
#else
	out_bitmap = (uint32_t*) malloc((n_elements + n_fill_dpu) / 32 * sizeof(uint32_t) + sizeof(uint32_t));
#endif
	assert(out_bitmap != NULL);

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

/*
 * Ensure that each DPU processes a multiple of 64 elements
 * → there are no partially-used bitmasks for SELECT queries at DPU boundaries
 *   (multiple of 32 elements)
 * → all MRAM bitmask accesses are 8-Byte aligned
 *   (multiple of 64 elements == bitmask size is a multiple of 8 Bytes)
 */
static void set_n_elements_dpu(unsigned long n_elem)
{
	n_elements_dpu = n_elem / n_dpus;

	assert(n_elements_dpu > 0);
	assert(n_elements_dpu * sizeof(T) < 65011712);

	if (n_elem % n_dpus) {
		n_elements_dpu += 1;
	}

	if (n_elements_dpu % 64) {
		n_elements_dpu += 64 - (n_elements_dpu % 64);
	}

	n_fill_dpu = n_elements_dpu * n_dpus - n_elem;
	first_filled_dpu = n_dpus - (n_fill_dpu / n_elements_dpu) - (n_fill_dpu % n_elements_dpu ? 1 : 0);

	printf("Using %d elements per DPU (filling DPUs %d..%d with %d non-elements)\n", n_elements_dpu, first_filled_dpu, n_dpus -1, n_fill_dpu);
}

static void db_to_upmem()
{
	unsigned int i = 0;
	DPU_FOREACH(dpu_set, dpu, i) {
		DPU_ASSERT(dpu_prepare_xfer(dpu, in_data + n_elements_dpu * i));
	}
	start(&timer, 2, 0);
	DPU_ASSERT(dpu_push_xfer(dpu_set, DPU_XFER_TO_DPU, DPU_MRAM_HEAP_POINTER_NAME, 0, n_elements_dpu * sizeof(T), DPU_XFER_DEFAULT));
	stop(&timer, 2);
}

static void upmem_select()
{
	unsigned int i = 0;
	DPU_FOREACH(dpu_set, dpu, i) {
		input_arguments[i].size = n_elements_dpu * sizeof(T);
		input_arguments[i].kernel = kernel_select;
		if (i == first_filled_dpu) {
			input_arguments[i].size = (n_elements_dpu - (n_fill_dpu % n_elements_dpu)) * sizeof(T);
		} else if (i > first_filled_dpu) {
			input_arguments[i].size = 0;
		}
		DPU_ASSERT(dpu_prepare_xfer(dpu, &input_arguments[i]));
	}

	start(&timer, 3, 0);
	DPU_ASSERT(dpu_push_xfer(dpu_set, DPU_XFER_TO_DPU, "DPU_INPUT_ARGUMENTS", 0, sizeof(dpu_arguments_t), DPU_XFER_DEFAULT));
	stop(&timer, 3);

	start(&timer, 4, 0);
	DPU_ASSERT(dpu_launch(dpu_set, DPU_SYNCHRONOUS));
	stop(&timer, 4);

	DPU_FOREACH(dpu_set, dpu, i) {
		DPU_ASSERT(dpu_prepare_xfer(dpu, out_bitmap + (n_elements_dpu/32) * i));
	}
	start(&timer, 5, 0);
	DPU_ASSERT(dpu_push_xfer(dpu_set, DPU_XFER_FROM_DPU, DPU_MRAM_HEAP_POINTER_NAME, 65011712, n_elements_dpu / 32 * sizeof(uint32_t), DPU_XFER_DEFAULT));
	stop(&timer, 5);
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


int main(int argc, char **argv) {
	parse_params(argc, argv);

	int numa_node_rank = -2;

	start(&timer, 0, 0);
#if NR_DPUS
	DPU_ASSERT(dpu_alloc(NR_DPUS, NULL, &dpu_set));
#else
	DPU_ASSERT(dpu_alloc_ranks(NR_RANKS, NULL, &dpu_set));
#endif
	stop(&timer, 0); // alloc

	int rank_id = -1;
	DPU_FOREACH (dpu_set, dpu) {
		rank_id = dpu_get_rank_id(dpu_get_rank(dpu_from_set(dpu))) & DPU_TARGET_MASK;
		if ((numa_node_rank != -2) && numa_node_rank != dpu_get_rank_numa_node(dpu_get_rank(dpu_from_set(dpu)))) {
			numa_node_rank = -1;
		} else {
			numa_node_rank = dpu_get_rank_numa_node(dpu_get_rank(dpu_from_set(dpu)));
		}
		/*
		if (rank_id != prev_rank_id) {
			printf("/dev/dpu_rank%d @ NUMA node %d\n", rank_id, numa_node_rank);
			prev_rank_id = rank_id;
		}
		*/
	}


	start(&timer, 1, 0);
	DPU_ASSERT(dpu_load(dpu_set, "bin/dpu_code", NULL));
	stop(&timer, 1); // load
	DPU_ASSERT(dpu_get_nr_dpus(dpu_set, &n_dpus));
	DPU_ASSERT(dpu_get_nr_ranks(dpu_set, &n_ranks));
#if NR_DPUS
	assert(n_dpus == NR_DPUS);
#endif

	n_elements = p.input_size;
	set_n_elements_dpu(n_elements);

	input_arguments = (dpu_arguments_t*)malloc(n_dpus * sizeof(dpu_arguments_t));
	assert(input_arguments != NULL);

	fill_db();

	for(int rep = 0; rep < p.n_warmup + p.n_reps; rep++) {

		db_to_upmem();
		upmem_select();

		if (count_bits() != n_elements / 2) {
			printf("Error: Expected upmem_select to find %llu elements, got %llu elements instead\n",
					n_elements / 2,
					count_bits()
			);
			break;
		}

		if (rep >= p.n_warmup) {
			printf("[::] SEL-NMC | n_dpus=%d n_ranks=%d n_tasklets=%d e_type=%s n_elements=%llu n_elements_per_dpu=%d",
					n_dpus, n_ranks, NR_TASKLETS, XSTR(T), n_elements, n_elements_dpu);
#if NUMA
			printf(" numa_node_in=%d numa_node_out=%d numa_node_cpu=%d numa_node_rank=%d numa_distance_in_rank=%d numa_distance_rank_out=%d",
				  numa_node_in, numa_node_out, numa_node_cpu, numa_node_rank, numa_distance(numa_node_in, numa_node_rank), numa_distance(numa_node_rank, numa_node_out));
#endif
			printf(" | latency_alloc_us=%f latency_load_us=%f latency_write_data_us=%f latency_write_args_us=%f latency_kernel_us=%f latency_read_result_us=%f\n",
					timer.time[0],
					timer.time[1],
					timer.time[2],
					timer.time[3],
					timer.time[4],
					timer.time[5]
			);
		}
	}

#if NUMA
	numa_free(in_data, n_elements * sizeof(T));
	numa_free(out_bitmap, n_elements / 32 * sizeof(uint32_t) + sizeof(uint32_t));
#else
	free(in_data);
	free(out_bitmap);
#endif
	free(input_arguments);

	DPU_ASSERT(dpu_free(dpu_set));

	return 0;
}
