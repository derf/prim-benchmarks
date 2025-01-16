/**
* app.c
* COUNT Host Application Source File
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

// Create input arrays
static void read_input(T *A, unsigned int nr_elements,
		       unsigned int nr_elements_round)
{
	//srand(0);
	printf("nr_elements\t%u\t", nr_elements);
	for (unsigned int i = 0; i < nr_elements; i++) {
		//A[i] = (T) (rand());
		A[i] = i + 1;
	}
	for (unsigned int i = nr_elements; i < nr_elements_round; i++) {	// Complete with removable elements
		A[i] = 0;
	}
}

// Compute output in the host
static unsigned int count_host(T *A, unsigned int nr_elements)
{
	unsigned int count = 0;
	for (unsigned int i = 0; i < nr_elements; i++) {
		if (!pred(A[i])) {
			count++;
		}
	}
	return count;
}

// Main of the Host Application
int main(int argc, char **argv)
{

	struct Params p = input_params(argc, argv);

	struct dpu_set_t dpu_set, dpu;
	uint32_t nr_of_dpus;
	uint32_t nr_of_ranks;

	// Timer declaration
	Timer timer;

	int numa_node_rank = -2;

	// Allocate DPUs and load binary
#if !WITH_ALLOC_OVERHEAD
	DPU_ASSERT(dpu_alloc(NR_DPUS, NULL, &dpu_set));
	timer.time[TMR_ALLOC] = 0;	// alloc
#endif
#if !WITH_LOAD_OVERHEAD
	DPU_ASSERT(dpu_load(dpu_set, DPU_BINARY, NULL));
	DPU_ASSERT(dpu_get_nr_dpus(dpu_set, &nr_of_dpus));
	DPU_ASSERT(dpu_get_nr_ranks(dpu_set, &nr_of_ranks));
	assert(nr_of_dpus == NR_DPUS);
	timer.time[TMR_LOAD] = 0;	// load
#endif
#if !WITH_FREE_OVERHEAD
	timer.time[TMR_FREE] = 0;	// free
#endif

#if ENERGY
	struct dpu_probe_t probe;
	DPU_ASSERT(dpu_probe_init("energy_probe", &probe));
#endif

	unsigned int i = 0;
	uint32_t accum = 0;
	uint32_t total_count = 0;

	const unsigned int input_size = p.exp == 0 ? p.input_size * NR_DPUS : p.input_size;	// Total input size (weak or strong scaling)
	const unsigned int input_size_dpu_ = divceil(input_size, NR_DPUS);	// Input size per DPU (max.)
	const unsigned int input_size_dpu_round = (input_size_dpu_ % (NR_TASKLETS * REGS) != 0) ? roundup(input_size_dpu_, (NR_TASKLETS * REGS)) : input_size_dpu_;	// Input size per DPU (max.), 8-byte aligned

	// Input allocation
	A = malloc(input_size_dpu_round * NR_DPUS * sizeof(T));
	T *bufferA = A;

	dpu_results_t *results_retrieve[NR_DPUS];
	for (i = 0; i < NR_DPUS; i++) {
		results_retrieve[i] =
		    (dpu_results_t *) malloc(NR_TASKLETS *
					     sizeof(dpu_results_t));
	}

	// Create an input file with arbitrary data
	read_input(A, input_size, input_size_dpu_round * NR_DPUS);

	printf("NR_TASKLETS\t%d\tBL\t%d\n", NR_TASKLETS, BL);

	// Loop over main kernel
	for (int rep = 0; rep < p.n_warmup + p.n_reps; rep++) {

#if WITH_ALLOC_OVERHEAD
		if (rep >= p.n_warmup) {
			start(&timer, TMR_ALLOC, 0);
		}
		DPU_ASSERT(dpu_alloc(NR_DPUS, NULL, &dpu_set));
		if (rep >= p.n_warmup) {
			stop(&timer, TMR_ALLOC);
		}
#endif
#if WITH_LOAD_OVERHEAD
		if (rep >= p.n_warmup) {
			start(&timer, TMR_LOAD, 0);
		}
		DPU_ASSERT(dpu_load(dpu_set, DPU_BINARY, NULL));
		if (rep >= p.n_warmup) {
			stop(&timer, TMR_LOAD);
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
		if (rep >= p.n_warmup)
			start(&timer, TMR_CPU, 0);
		total_count = count_host(A, input_size);
		if (rep >= p.n_warmup)
			stop(&timer, TMR_CPU);

		printf("Load input data\n");
		if (rep >= p.n_warmup)
			start(&timer, TMR_WRITE, 0);
		// Input arguments
		const unsigned int input_size_dpu = input_size_dpu_round;
		unsigned int kernel = 0;
		dpu_arguments_t input_arguments =
		    { input_size_dpu * sizeof(T), kernel };
		// Copy input arrays
		i = 0;
		DPU_FOREACH(dpu_set, dpu, i) {
			DPU_ASSERT(dpu_prepare_xfer(dpu, &input_arguments));
		}
		DPU_ASSERT(dpu_push_xfer
			   (dpu_set, DPU_XFER_TO_DPU, "DPU_INPUT_ARGUMENTS", 0,
			    sizeof(input_arguments), DPU_XFER_DEFAULT));
		DPU_FOREACH(dpu_set, dpu, i) {
			DPU_ASSERT(dpu_prepare_xfer
				   (dpu, bufferA + input_size_dpu * i));
		}
		DPU_ASSERT(dpu_push_xfer
			   (dpu_set, DPU_XFER_TO_DPU,
			    DPU_MRAM_HEAP_POINTER_NAME, 0,
			    input_size_dpu * sizeof(T), DPU_XFER_DEFAULT));
		if (rep >= p.n_warmup)
			stop(&timer, TMR_WRITE);

		printf("Run program on DPU(s) \n");
		// Run DPU kernel
		if (rep >= p.n_warmup) {
			start(&timer, TMR_KERNEL, 0);
#if ENERGY
			DPU_ASSERT(dpu_probe_start(&probe));
#endif
		}
		DPU_ASSERT(dpu_launch(dpu_set, DPU_SYNCHRONOUS));
		if (rep >= p.n_warmup) {
			stop(&timer, TMR_KERNEL);
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

		printf("Retrieve results\n");
		dpu_results_t results[NR_DPUS];
		i = 0;
		accum = 0;

		if (rep >= p.n_warmup)
			start(&timer, TMR_READ, 0);
		// PARALLEL RETRIEVE TRANSFER

		DPU_FOREACH(dpu_set, dpu, i) {
			DPU_ASSERT(dpu_prepare_xfer(dpu, results_retrieve[i]));
		}
		DPU_ASSERT(dpu_push_xfer
			   (dpu_set, DPU_XFER_FROM_DPU, "DPU_RESULTS", 0,
			    NR_TASKLETS * sizeof(dpu_results_t),
			    DPU_XFER_DEFAULT));

		DPU_FOREACH(dpu_set, dpu, i) {
			// Retrieve tasklet timings
			for (unsigned int each_tasklet = 0;
			     each_tasklet < NR_TASKLETS; each_tasklet++) {
				// Count of this DPU
				if (each_tasklet == NR_TASKLETS - 1) {
					results[i].t_count =
					    results_retrieve[i][each_tasklet].
					    t_count;
				}
			}
			// Sequential scan
			accum += results[i].t_count;
		}
		if (rep >= p.n_warmup)
			stop(&timer, TMR_READ);

		i = 0;

#if WITH_ALLOC_OVERHEAD
#if WITH_FREE_OVERHEAD
		if (rep >= p.n_warmup) {
			start(&timer, TMR_FREE, 0);
		}
#endif
		DPU_ASSERT(dpu_free(dpu_set));
#if WITH_FREE_OVERHEAD
		if (rep >= p.n_warmup) {
			stop(&timer, TMR_FREE);
		}
#endif
#endif

		// Check output
		bool status = true;
		if (accum != total_count)
			status = false;
		if (status) {
			printf("[" ANSI_COLOR_GREEN "OK" ANSI_COLOR_RESET
			       "] Outputs are equal\n");
			if (rep >= p.n_warmup) {
				printf
				    ("[::] COUNT-UPMEM | n_dpus=%d n_ranks=%d n_tasklets=%d e_type=%s block_size_B=%d n_elements=%d n_elements_per_dpu=%d",
				     NR_DPUS, nr_of_ranks, NR_TASKLETS, XSTR(T),
				     BLOCK_SIZE, input_size,
				     input_size_dpu_round);
				printf
				    (" b_with_alloc_overhead=%d b_with_load_overhead=%d b_with_free_overhead=%d numa_node_rank=%d ",
				     WITH_ALLOC_OVERHEAD, WITH_LOAD_OVERHEAD,
				     WITH_FREE_OVERHEAD, numa_node_rank);
				printf("| latency_alloc_us=%f latency_load_us=%f latency_cpu_us=%f latency_write_us=%f latency_kernel_us=%f latency_read_us=%f latency_free_us=%f", timer.time[0], timer.time[1], timer.time[2], timer.time[3],	// write
				       timer.time[4],	// kernel
				       timer.time[5],	// read
				       timer.time[8]);
				printf(" latency_total_us=%f",
				       timer.time[0] + timer.time[1] +
				       timer.time[3] + timer.time[4] +
				       timer.time[5] + timer.time[8]);
				printf
				    (" throughput_cpu_MBps=%f throughput_upmem_kernel_MBps=%f throughput_upmem_total_MBps=%f",
				     input_size * sizeof(T) / timer.time[2],
				     input_size * sizeof(T) / timer.time[4],
				     input_size * sizeof(T) / (timer.time[0] +
							       timer.time[1] +
							       timer.time[3] +
							       timer.time[4] +
							       timer.time[5] +
							       timer.time[8])
				    );
				printf
				    (" throughput_upmem_wxr_MBps=%f throughput_upmem_lwxr_MBps=%f throughput_upmem_alwxr_MBps=%f",
				     input_size * sizeof(T) / (timer.time[3] +
							       timer.time[4] +
							       timer.time[5]),
				     input_size * sizeof(T) / (timer.time[1] +
							       timer.time[3] +
							       timer.time[4] +
							       timer.time[5]),
				     input_size * sizeof(T) / (timer.time[0] +
							       timer.time[1] +
							       timer.time[3] +
							       timer.time[4] +
							       timer.time[5]));
				printf
				    (" throughput_cpu_MOpps=%f throughput_upmem_kernel_MOpps=%f throughput_upmem_total_MOpps=%f",
				     input_size / timer.time[2],
				     input_size / timer.time[4],
				     input_size / (timer.time[0] +
						   timer.time[1] +
						   timer.time[3] +
						   timer.time[4] +
						   timer.time[5] +
						   timer.time[8])
				    );
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

#if !WITH_ALLOC_OVERHEAD
	DPU_ASSERT(dpu_free(dpu_set));
#endif

	return 0;
}
