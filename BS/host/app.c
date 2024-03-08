/**
* app.c
* BS Host Application Source File
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
#include <time.h>

#if ENERGY
#include <dpu_probe.h>
#endif

#if WITH_DPUINFO
#include <dpu_management.h>
#include <dpu_target_macros.h>
#endif

#if SDK_SINGLETHREADED
#define DPU_ALLOC_PROFILE "nrThreadsPerRank=0"
#else
#define DPU_ALLOC_PROFILE NULL
#endif

#define XSTR(x) STR(x)
#define STR(x) #x

#include "params.h"
#include "timer.h"

// Define the DPU Binary path as DPU_BINARY here
#define DPU_BINARY "./bin/bs_dpu"

// Create input arrays
void create_test_file(DTYPE * input, DTYPE * querys, uint64_t  nr_elements, uint64_t nr_querys) {

	input[0] = 1;
	for (uint64_t i = 1; i < nr_elements; i++) {
		input[i] = input[i - 1] + 1;
	}
	for (uint64_t i = 0; i < nr_querys; i++) {
		querys[i] = i;
	}
}

// Compute output in the host
int64_t binarySearch(DTYPE * input, DTYPE * querys, DTYPE input_size, uint64_t num_querys)
{
	uint64_t result = -1;
	DTYPE r;
	for(uint64_t q = 0; q < num_querys; q++)
	{
		DTYPE l = 0;
		r = input_size;
		while (l <= r) {
			DTYPE m = l + (r - l) / 2;

			// XXX shouldn't this short-circuit?
			// Check if x is present at mid
			if (input[m] == querys[q])
			result = m;

			// If x greater, ignore left half
			if (input[m] < querys[q])
			l = m + 1;

			// If x is smaller, ignore right half
			else
			r = m - 1;
		}
	}
	return result;
}


// Main of the Host Application
int main(int argc, char **argv) {

	struct Params p = input_params(argc, argv);
	struct dpu_set_t dpu_set, dpu;
	uint32_t nr_of_dpus;
    uint32_t nr_of_ranks;
	uint64_t input_size = INPUT_SIZE;
	uint64_t num_querys = p.num_querys;
	DTYPE result_host = -1;
	DTYPE result_dpu  = -1;

    // Timer declaration
    Timer timer;

    // Allocate DPUs and load binary
#if !WITH_ALLOC_OVERHEAD
    DPU_ASSERT(dpu_alloc(NR_DPUS, DPU_ALLOC_PROFILE, &dpu_set));
    timer.time[0] = 0; // alloc
#endif
#if !WITH_LOAD_OVERHEAD
    DPU_ASSERT(dpu_load(dpu_set, DPU_BINARY, NULL));
    DPU_ASSERT(dpu_get_nr_dpus(dpu_set, &nr_of_dpus));
    DPU_ASSERT(dpu_get_nr_ranks(dpu_set, &nr_of_ranks));
    assert(nr_of_dpus == NR_DPUS);
    timer.time[1] = 0; // load
#endif
#if !WITH_FREE_OVERHEAD
    timer.time[6] = 0; // free
#endif

	#if ENERGY
	struct dpu_probe_t probe;
	DPU_ASSERT(dpu_probe_init("energy_probe", &probe));
	#endif

	// Query number adjustement for proper partitioning
	if(num_querys % (NR_DPUS * NR_TASKLETS))
	num_querys = num_querys + (NR_DPUS * NR_TASKLETS - num_querys % (NR_DPUS * NR_TASKLETS));

	assert(num_querys % (NR_DPUS * NR_TASKLETS) == 0 && "Input dimension");    // Allocate input and querys vectors

	DTYPE * input  = malloc((input_size) * sizeof(DTYPE));
	DTYPE * querys = malloc((num_querys) * sizeof(DTYPE));

	// Create an input file with arbitrary data
	create_test_file(input, querys, input_size, num_querys);

	// Create kernel arguments
	uint64_t slice_per_dpu          = num_querys / NR_DPUS;
	dpu_arguments_t input_arguments = {input_size, slice_per_dpu, 0};

	for (unsigned int rep = 0; rep < p.n_warmup + p.n_reps; rep++) {
		// Perform input transfers
		uint64_t i = 0;

#if WITH_ALLOC_OVERHEAD
		if(rep >= p.n_warmup) {
			start(&timer, 0, 0);
		}
		DPU_ASSERT(dpu_alloc(NR_DPUS, DPU_ALLOC_PROFILE, &dpu_set));
		if(rep >= p.n_warmup) {
			stop(&timer, 0);
		}
#endif
#if WITH_DPUINFO
		printf("DPUs:");
		DPU_FOREACH (dpu_set, dpu) {
			int rank = dpu_get_rank_id(dpu_get_rank(dpu_from_set(dpu))) & DPU_TARGET_MASK;
			int slice = dpu_get_slice_id(dpu_from_set(dpu));
			int member = dpu_get_member_id(dpu_from_set(dpu));
			printf(" %d(%d.%d)", rank, slice, member);
		}
		printf("\n");
#endif
#if WITH_LOAD_OVERHEAD
		if(rep >= p.n_warmup) {
			start(&timer, 1, 0);
		}
		DPU_ASSERT(dpu_load(dpu_set, DPU_BINARY, NULL));
		if(rep >= p.n_warmup) {
			stop(&timer, 1);
		}
		DPU_ASSERT(dpu_get_nr_dpus(dpu_set, &nr_of_dpus));
		DPU_ASSERT(dpu_get_nr_ranks(dpu_set, &nr_of_ranks));
		assert(nr_of_dpus == NR_DPUS);
#endif

		// Compute host solution
		if(rep >= p.n_warmup) {
			start(&timer, 2, 0);
		}
		result_host = binarySearch(input, querys, input_size - 1, num_querys);
		if(rep >= p.n_warmup) {
			stop(&timer, 2);
		}

		if (rep >= p.n_warmup) {
			start(&timer, 3, 0);
		}

		DPU_FOREACH(dpu_set, dpu, i)
		{
			DPU_ASSERT(dpu_prepare_xfer(dpu, &input_arguments));
		}

		DPU_ASSERT(dpu_push_xfer(dpu_set, DPU_XFER_TO_DPU, "DPU_INPUT_ARGUMENTS", 0, sizeof(input_arguments), DPU_XFER_DEFAULT));

		i = 0;

		DPU_FOREACH(dpu_set, dpu, i)
		{
			DPU_ASSERT(dpu_prepare_xfer(dpu, input));
		}

		DPU_ASSERT(dpu_push_xfer(dpu_set, DPU_XFER_TO_DPU, DPU_MRAM_HEAP_POINTER_NAME, 0, input_size * sizeof(DTYPE), DPU_XFER_DEFAULT));

		i = 0;

		DPU_FOREACH(dpu_set, dpu, i)
		{
			DPU_ASSERT(dpu_prepare_xfer(dpu, querys + slice_per_dpu * i));
		}

		DPU_ASSERT(dpu_push_xfer(dpu_set, DPU_XFER_TO_DPU, DPU_MRAM_HEAP_POINTER_NAME, input_size * sizeof(DTYPE), slice_per_dpu * sizeof(DTYPE), DPU_XFER_DEFAULT));

		if (rep >= p.n_warmup) {
			stop(&timer, 3);
		}

		// Run kernel on DPUs
		if (rep >= p.n_warmup)
		{
			start(&timer, 4, 0);
			#if ENERGY
			DPU_ASSERT(dpu_probe_start(&probe));
			#endif
		}

		DPU_ASSERT(dpu_launch(dpu_set, DPU_SYNCHRONOUS));

		if (rep >= p.n_warmup)
		{
			stop(&timer, 4);
			#if ENERGY
			DPU_ASSERT(dpu_probe_stop(&probe));
			#endif
		}
		// Print logs if required
		#if PRINT
		unsigned int each_dpu = 0;
		printf("Display DPU Logs\n");
		DPU_FOREACH(dpu_set, dpu)
		{
			printf("DPU#%d:\n", each_dpu);
			DPU_ASSERT(dpulog_read_for_dpu(dpu.dpu, stdout));
			each_dpu++;
		}
		#endif

		// Retrieve results
		if (rep >= p.n_warmup) {
			start(&timer, 5, 0);
		}
		dpu_results_t* results_retrieve[NR_DPUS];
		i = 0;
		DPU_FOREACH(dpu_set, dpu, i)
		{
			results_retrieve[i] = (dpu_results_t*)malloc(NR_TASKLETS * sizeof(dpu_results_t));
			DPU_ASSERT(dpu_prepare_xfer(dpu, results_retrieve[i]));
		}

		DPU_ASSERT(dpu_push_xfer(dpu_set, DPU_XFER_FROM_DPU, "DPU_RESULTS", 0, NR_TASKLETS * sizeof(dpu_results_t), DPU_XFER_DEFAULT));

		DPU_FOREACH(dpu_set, dpu, i)
		{
			for(unsigned int each_tasklet = 0; each_tasklet < NR_TASKLETS; each_tasklet++)
			{
				if(results_retrieve[i][each_tasklet].found > result_dpu)
				{
					result_dpu = results_retrieve[i][each_tasklet].found;
				}
			}
			free(results_retrieve[i]);
		}
		if(rep >= p.n_warmup) {
			stop(&timer, 5);
		}

#if WITH_ALLOC_OVERHEAD
#if WITH_FREE_OVERHEAD
		if(rep >= p.n_warmup) {
			start(&timer, 6, 0);
		}
#endif
		DPU_ASSERT(dpu_free(dpu_set));
#if WITH_FREE_OVERHEAD
		if(rep >= p.n_warmup) {
			stop(&timer, 6);
		}
#endif
#endif

		int status = (result_dpu == result_host);
		if (status) {
			printf("[" ANSI_COLOR_GREEN "OK" ANSI_COLOR_RESET "] results are equal\n");
			if (rep >= p.n_warmup) {
				printf("[::] BS UPMEM | n_dpus=%d n_ranks=%d n_tasklets=%d e_type=%s block_size_B=%d n_elements=%lu",
					NR_DPUS, nr_of_ranks, NR_TASKLETS, XSTR(DTYPE), BLOCK_SIZE, input_size);
				printf(" b_sdk_singlethreaded=%d b_with_alloc_overhead=%d b_with_load_overhead=%d b_with_free_overhead=%d ",
					SDK_SINGLETHREADED, WITH_ALLOC_OVERHEAD, WITH_LOAD_OVERHEAD, WITH_FREE_OVERHEAD);
				printf("| latency_alloc_us=%f latency_load_us=%f latency_cpu_us=%f latency_write_us=%f latency_kernel_us=%f latency_read_us=%f latency_free_us=%f",
					timer.time[0],
					timer.time[1],
					timer.time[2],
					timer.time[3],
					timer.time[4],
					timer.time[5],
					timer.time[6]);
				printf(" throughput_cpu_MBps=%f throughput_upmem_kernel_MBps=%f throughput_upmem_total_MBps=%f",
					input_size * sizeof(DTYPE) / timer.time[2],
					input_size * sizeof(DTYPE) / (timer.time[4]),
					input_size * sizeof(DTYPE) / (timer.time[0] + timer.time[1] + timer.time[3] + timer.time[4] + timer.time[5] + timer.time[6]));
				printf(" throughput_upmem_wxr_MBps=%f throughput_upmem_lwxr_MBps=%f throughput_upmem_alwxr_MBps=%f",
					input_size * sizeof(DTYPE) / (timer.time[3] + timer.time[4] + timer.time[5]),
					input_size * sizeof(DTYPE) / (timer.time[1] + timer.time[3] + timer.time[4] + timer.time[5]),
					input_size * sizeof(DTYPE) / (timer.time[0] + timer.time[1] + timer.time[3] + timer.time[4] + timer.time[5]));
				printf(" throughput_cpu_MOpps=%f throughput_upmem_kernel_MOpps=%f throughput_upmem_total_MOpps=%f",
					input_size / timer.time[2],
					input_size / (timer.time[4]),
					input_size / (timer.time[0] + timer.time[1] + timer.time[3] + timer.time[4] + timer.time[5] + timer.time[6]));
				printf(" throughput_upmem_wxr_MOpps=%f throughput_upmem_lwxr_MOpps=%f throughput_upmem_alwxr_MOpps=%f\n",
					input_size / (timer.time[3] + timer.time[4] + timer.time[5]),
					input_size / (timer.time[1] + timer.time[3] + timer.time[4] + timer.time[5]),
					input_size / (timer.time[0] + timer.time[1] + timer.time[3] + timer.time[4] + timer.time[5]));
			}
		} else {
			printf("[" ANSI_COLOR_RED "ERROR" ANSI_COLOR_RESET "] results differ!\n");
		}
	}
	// Print timing results
	/*
	printf("CPU Version Time (ms): ");
	print(&timer, 0, p.n_reps);
	printf("CPU-DPU Time (ms): ");
	print(&timer, 1, p.n_reps);
	printf("DPU Kernel Time (ms): ");
	print(&timer, 2, p.n_reps);
	printf("DPU-CPU Time (ms): ");
	print(&timer, 3, p.n_reps);
	*/

	#if ENERGY
	double energy;
	DPU_ASSERT(dpu_probe_get(&probe, DPU_ENERGY, DPU_AVERAGE, &energy));
	printf("DPU Energy (J): %f\t", energy * num_iterations);
	#endif

	free(input);
#if !WITH_ALLOC_OVERHEAD
	DPU_ASSERT(dpu_free(dpu_set));
#endif

	return 0;
}
