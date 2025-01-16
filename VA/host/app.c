/**
* app.c
* VA Host Application Source File
*
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

#include "../support/common.h"
#include "../support/timer.h"
#include "../support/params.h"

// Define the DPU Binary path as DPU_BINARY here
#ifndef DPU_BINARY
#define DPU_BINARY "./bin/dpu_code"
#endif

#define XSTR(x) STR(x)
#define STR(x) #x

#if ENERGY
#include <dpu_probe.h>
#endif

#include <dpu_management.h>
#include <dpu_target_macros.h>

// Pointer declaration
static T *A;
static T *B;
static T *C;
static T *C2;

// Create input arrays
static void read_input(T *A, T *B, unsigned int nr_elements)
{
	srand(0);
	for (unsigned int i = 0; i < nr_elements; i++) {
		A[i] = (T) (rand());
		B[i] = (T) (rand());
	}
}

// Compute output in the host
static void vector_addition_host(T *C, T *A, T *B, unsigned int nr_elements)
{
	for (unsigned int i = 0; i < nr_elements; i++) {
		C[i] = A[i] + B[i];
	}
}

// Main of the Host Application
int main(int argc, char **argv)
{

	struct Params p = input_params(argc, argv);

	struct dpu_set_t dpu_set, dpu;
	uint32_t nr_of_dpus;
	uint32_t nr_of_ranks;

#if ENERGY
	struct dpu_probe_t probe;
	DPU_ASSERT(dpu_probe_init("energy_probe", &probe));
#endif

	// Timer declaration
	Timer timer;

	int numa_node_rank = -2;

	// Allocate DPUs and load binary
#if !WITH_ALLOC_OVERHEAD
	DPU_ASSERT(dpu_alloc(NR_DPUS, NULL, &dpu_set));
	timer.time[0] = 0;	// alloc
#endif
#if !WITH_LOAD_OVERHEAD
	DPU_ASSERT(dpu_load(dpu_set, DPU_BINARY, NULL));
	DPU_ASSERT(dpu_get_nr_dpus(dpu_set, &nr_of_dpus));
	DPU_ASSERT(dpu_get_nr_ranks(dpu_set, &nr_of_ranks));
	assert(nr_of_dpus == NR_DPUS);
	timer.time[1] = 0;	// load
#endif
#if !WITH_FREE_OVERHEAD
	timer.time[6] = 0;	// free
#endif

	unsigned int i = 0;
	const unsigned int input_size =
	    p.exp == 0 ? p.input_size * NR_DPUS : p.input_size;
	const unsigned int input_size_8bytes = ((input_size * sizeof(T)) % 8) != 0 ? roundup(input_size, 8) : input_size;	// Input size per DPU (max.), 8-byte aligned
	const unsigned int input_size_dpu = divceil(input_size, NR_DPUS);	// Input size per DPU (max.)
	const unsigned int input_size_dpu_8bytes = ((input_size_dpu * sizeof(T)) % 8) != 0 ? roundup(input_size_dpu, 8) : input_size_dpu;	// Input size per DPU (max.), 8-byte aligned

	// Input/output allocation
	A = malloc(input_size_dpu_8bytes * NR_DPUS * sizeof(T));
	B = malloc(input_size_dpu_8bytes * NR_DPUS * sizeof(T));
	C = malloc(input_size_dpu_8bytes * NR_DPUS * sizeof(T));
	C2 = malloc(input_size_dpu_8bytes * NR_DPUS * sizeof(T));
	T *bufferA = A;
	T *bufferB = B;
	T *bufferC = C2;

	// Create an input file with arbitrary data
	read_input(A, B, input_size);

	// Loop over main kernel
	for (int rep = 0; rep < p.n_warmup + p.n_reps; rep++) {

#if WITH_ALLOC_OVERHEAD
		if (rep >= p.n_warmup) {
			start(&timer, 0, 0);
		}
		DPU_ASSERT(dpu_alloc(NR_DPUS, NULL, &dpu_set));
		if (rep >= p.n_warmup) {
			stop(&timer, 0);
		}
#endif
#if WITH_DPUINFO
		printf("DPUs:");
		DPU_FOREACH(dpu_set, dpu) {
			int rank =
			    dpu_get_rank_id(dpu_get_rank(dpu_from_set(dpu))) &
			    DPU_TARGET_MASK;
			int slice = dpu_get_slice_id(dpu_from_set(dpu));
			int member = dpu_get_member_id(dpu_from_set(dpu));
			printf(" %d(%d.%d)", rank, slice, member);
		}
		printf("\n");
#endif
#if WITH_LOAD_OVERHEAD
		if (rep >= p.n_warmup) {
			start(&timer, 1, 0);
		}
		DPU_ASSERT(dpu_load(dpu_set, DPU_BINARY, NULL));
		if (rep >= p.n_warmup) {
			stop(&timer, 1);
		}
		DPU_ASSERT(dpu_get_nr_dpus(dpu_set, &nr_of_dpus));
		DPU_ASSERT(dpu_get_nr_ranks(dpu_set, &nr_of_ranks));
		assert(nr_of_dpus == NR_DPUS);
#endif

		// int prev_rank_id = -1;
		int rank_id = -1;
		DPU_FOREACH(dpu_set, dpu) {
			rank_id =
			    dpu_get_rank_id(dpu_get_rank(dpu_from_set(dpu))) &
			    DPU_TARGET_MASK;
			if ((numa_node_rank != -2)
			    && numa_node_rank !=
			    dpu_get_rank_numa_node(dpu_get_rank
						   (dpu_from_set(dpu)))) {
				numa_node_rank = -1;
			} else {
				numa_node_rank =
				    dpu_get_rank_numa_node(dpu_get_rank
							   (dpu_from_set(dpu)));
			}
			/*
			   if (rank_id != prev_rank_id) {
			   printf("/dev/dpu_rank%d @ NUMA node %d\n", rank_id, numa_node_rank);
			   prev_rank_id = rank_id;
			   }
			 */
		}

		// Compute output on CPU (performance comparison and verification purposes)
		if (rep >= p.n_warmup) {
			start(&timer, 2, 0);
		}
		vector_addition_host(C, A, B, input_size);
		if (rep >= p.n_warmup) {
			stop(&timer, 2);
		}

		if (rep >= p.n_warmup) {
			start(&timer, 3, 0);
		}
		// Input arguments
		unsigned int kernel = 0;
		dpu_arguments_t input_arguments[NR_DPUS];
		for (i = 0; i < nr_of_dpus - 1; i++) {
			input_arguments[i].size =
			    input_size_dpu_8bytes * sizeof(T);
			input_arguments[i].transfer_size =
			    input_size_dpu_8bytes * sizeof(T);
			input_arguments[i].kernel = kernel;
		}
		input_arguments[nr_of_dpus - 1].size =
		    (input_size_8bytes -
		     input_size_dpu_8bytes * (NR_DPUS - 1)) * sizeof(T);
		input_arguments[nr_of_dpus - 1].transfer_size =
		    input_size_dpu_8bytes * sizeof(T);
		input_arguments[nr_of_dpus - 1].kernel = kernel;

		// Copy input arrays
		i = 0;
		DPU_FOREACH(dpu_set, dpu, i) {
			DPU_ASSERT(dpu_prepare_xfer(dpu, &input_arguments[i]));
		}
		DPU_ASSERT(dpu_push_xfer
			   (dpu_set, DPU_XFER_TO_DPU, "DPU_INPUT_ARGUMENTS", 0,
			    sizeof(input_arguments[0]), DPU_XFER_DEFAULT));

		DPU_FOREACH(dpu_set, dpu, i) {
			DPU_ASSERT(dpu_prepare_xfer
				   (dpu, bufferA + input_size_dpu_8bytes * i));
		}
		DPU_ASSERT(dpu_push_xfer
			   (dpu_set, DPU_XFER_TO_DPU,
			    DPU_MRAM_HEAP_POINTER_NAME, 0,
			    input_size_dpu_8bytes * sizeof(T),
			    DPU_XFER_DEFAULT));

		DPU_FOREACH(dpu_set, dpu, i) {
			DPU_ASSERT(dpu_prepare_xfer
				   (dpu, bufferB + input_size_dpu_8bytes * i));
		}
		DPU_ASSERT(dpu_push_xfer
			   (dpu_set, DPU_XFER_TO_DPU,
			    DPU_MRAM_HEAP_POINTER_NAME,
			    input_size_dpu_8bytes * sizeof(T),
			    input_size_dpu_8bytes * sizeof(T),
			    DPU_XFER_DEFAULT));
		if (rep >= p.n_warmup) {
			stop(&timer, 3);
		}
		// Run DPU kernel
		if (rep >= p.n_warmup) {
			start(&timer, 4, 0);
#if ENERGY
			DPU_ASSERT(dpu_probe_start(&probe));
#endif
		}
		DPU_ASSERT(dpu_launch(dpu_set, DPU_SYNCHRONOUS));
		if (rep >= p.n_warmup) {
			stop(&timer, 4);
#if ENERGY
			DPU_ASSERT(dpu_probe_stop(&probe));
#endif
		}
#if PRINT
		{
			unsigned int each_dpu = 0;
			printf("Display DPU Logs\n");
			DPU_FOREACH(dpu_set, dpu) {
				printf("DPU#%d:\n", each_dpu);
				DPU_ASSERT(dpulog_read_for_dpu
					   (dpu.dpu, stdout));
				each_dpu++;
			}
		}
#endif

		if (rep >= p.n_warmup) {
			start(&timer, 5, 0);
		}
		i = 0;
		// PARALLEL RETRIEVE TRANSFER
		DPU_FOREACH(dpu_set, dpu, i) {
			DPU_ASSERT(dpu_prepare_xfer
				   (dpu, bufferC + input_size_dpu_8bytes * i));
		}
		DPU_ASSERT(dpu_push_xfer
			   (dpu_set, DPU_XFER_FROM_DPU,
			    DPU_MRAM_HEAP_POINTER_NAME,
			    input_size_dpu_8bytes * sizeof(T),
			    input_size_dpu_8bytes * sizeof(T),
			    DPU_XFER_DEFAULT));
		if (rep >= p.n_warmup) {
			stop(&timer, 5);
		}
#if WITH_ALLOC_OVERHEAD
#if WITH_FREE_OVERHEAD
		if (rep >= p.n_warmup) {
			start(&timer, 6, 0);
		}
#endif
		DPU_ASSERT(dpu_free(dpu_set));
#if WITH_FREE_OVERHEAD
		if (rep >= p.n_warmup) {
			stop(&timer, 6);
		}
#endif
#endif

		// Check output
		bool status = true;
		for (i = 0; i < input_size; i++) {
			if (C[i] != bufferC[i]) {
				status = false;
#if PRINT
				printf("%d: %u -- %u\n", i, C[i], bufferC[i]);
#endif
			}
		}
		if (status) {
			printf("[" ANSI_COLOR_GREEN "OK" ANSI_COLOR_RESET
			       "] Outputs are equal\n");
			if (rep >= p.n_warmup) {
				printf
				    ("[::] VA-UPMEM | n_dpus=%d n_ranks=%d n_tasklets=%d e_type=%s block_size_B=%d n_elements=%d n_elements_per_dpu=%d",
				     nr_of_dpus, nr_of_ranks, NR_TASKLETS,
				     XSTR(T), BLOCK_SIZE, input_size,
				     input_size / NR_DPUS);
				printf
				    (" b_with_alloc_overhead=%d b_with_load_overhead=%d b_with_free_overhead=%d numa_node_rank=%d ",
				     WITH_ALLOC_OVERHEAD, WITH_LOAD_OVERHEAD,
				     WITH_FREE_OVERHEAD, numa_node_rank);
				printf
				    ("| latency_alloc_us=%f latency_load_us=%f latency_cpu_us=%f latency_write_us=%f latency_kernel_us=%f latency_read_us=%f latency_free_us=%f",
				     timer.time[0], timer.time[1],
				     timer.time[2], timer.time[3],
				     timer.time[4], timer.time[5],
				     timer.time[6]);
				printf
				    (" throughput_cpu_MBps=%f throughput_upmem_kernel_MBps=%f throughput_upmem_total_MBps=%f",
				     input_size * 3 * sizeof(T) / timer.time[2],
				     input_size * 3 * sizeof(T) /
				     (timer.time[4]),
				     input_size * 3 * sizeof(T) /
				     (timer.time[0] + timer.time[1] +
				      timer.time[3] + timer.time[4] +
				      timer.time[5] + timer.time[6]));
				printf
				    (" throughput_upmem_wxr_MBps=%f throughput_upmem_lwxr_MBps=%f throughput_upmem_alwxr_MBps=%f",
				     input_size * 3 * sizeof(T) /
				     (timer.time[3] + timer.time[4] +
				      timer.time[5]),
				     input_size * 3 * sizeof(T) /
				     (timer.time[1] + timer.time[3] +
				      timer.time[4] + timer.time[5]),
				     input_size * 3 * sizeof(T) /
				     (timer.time[0] + timer.time[1] +
				      timer.time[3] + timer.time[4] +
				      timer.time[5]));
				printf
				    (" throughput_cpu_MOpps=%f throughput_upmem_kernel_MOpps=%f throughput_upmem_total_MOpps=%f",
				     input_size / timer.time[2],
				     input_size / (timer.time[4]),
				     input_size / (timer.time[0] +
						   timer.time[1] +
						   timer.time[3] +
						   timer.time[4] +
						   timer.time[5] +
						   timer.time[6]));
				printf
				    (" throughput_upmem_wxr_MOpps=%f throughput_upmem_lwxr_MOpps=%f throughput_upmem_alwxr_MOpps=%f\n",
				     input_size / (timer.time[3] +
						   timer.time[4] +
						   timer.time[5]),
				     input_size / (timer.time[1] +
						   timer.time[3] +
						   timer.time[4] +
						   timer.time[5]),
				     input_size / (timer.time[0] +
						   timer.time[1] +
						   timer.time[3] +
						   timer.time[4] +
						   timer.time[5]));
			}
		} else {
			printf("[" ANSI_COLOR_RED "ERROR" ANSI_COLOR_RESET
			       "] Outputs differ!\n");
		}
	}

#if ENERGY
	double energy;
	DPU_ASSERT(dpu_probe_get(&probe, DPU_ENERGY, DPU_AVERAGE, &energy));
	printf("DPU Energy (J): %f\t", energy);
#endif

	// Deallocation
	free(A);
	free(B);
	free(C);
	free(C2);

#if !WITH_ALLOC_OVERHEAD
	DPU_ASSERT(dpu_free(dpu_set));
#endif

	return 0;
}
