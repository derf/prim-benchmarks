/**
 * app.c
 * GEMV Host Application Source File
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

#if ENERGY
#include <dpu_probe.h>
#endif

#define XSTR(x) STR(x)
#define STR(x) #x

#include "../support/common.h"
#include "../support/timer.h"
#include "../support/params.h"

// Define the DPU Binary path as DPU_BINARY here
#ifndef DPU_BINARY
#define DPU_BINARY "./bin/gemv_dpu"
#endif

static T* A;
static T* B;
static T* C;
static T* C_dpu;

// Create input arrays
static void init_data(T* A, T* B, unsigned int m_size, unsigned int n_size) {
	srand(0);

	for (unsigned int i = 0; i < m_size * n_size; i++)
	{
		A[i] = (unsigned int) (rand()%50);
	}

	for (unsigned int i = 0; i < n_size; i++)
	{
		B[i] = (unsigned int) (rand()%50);
	}
}

// Compute output in the host
static void gemv_host(T* C, T* A, T* B, unsigned int m_size, unsigned int n_size) {
	for (unsigned int i = 0; i < m_size; i++)
	{
		C[i] = 0;
	}

	for (unsigned int m = 0; m < m_size; m++) {
		for (unsigned int n = 0; n < n_size; n++)
		{
			C[m] += A[m * n_size + n] * B[n];
		}
	}
}

// Main of the Host Application
int main(int argc, char **argv) {

	struct Params p = input_params(argc, argv);

	struct dpu_set_t dpu_set, dpu;
	uint32_t nr_of_dpus;

	// Timer
	Timer timer;

    // Allocate DPUs and load binary
#if !WITH_ALLOC_OVERHEAD
    DPU_ASSERT(dpu_alloc(NR_DPUS, NULL, &dpu_set));
    timer.time[0] = 0; // alloc
#endif
#if !WITH_LOAD_OVERHEAD
    DPU_ASSERT(dpu_load(dpu_set, DPU_BINARY, NULL));
    DPU_ASSERT(dpu_get_nr_dpus(dpu_set, &nr_of_dpus));
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

	unsigned int i;
	unsigned int m_size = p.m_size;
	unsigned int n_size = p.n_size;

	// Initialize help data
	dpu_info = (struct dpu_info_t *) malloc(NR_DPUS * sizeof(struct dpu_info_t));
	dpu_arguments_t *input_args = (dpu_arguments_t *) malloc(NR_DPUS * sizeof(dpu_arguments_t));
	uint32_t max_rows_per_dpu = 0;
	uint32_t n_size_pad = n_size;
	if(n_size % 2 == 1)
	{
		n_size_pad++;
	}

	for (i = 0; i < NR_DPUS; i++) {
		uint32_t rows_per_dpu;
		uint32_t prev_rows_dpu = 0;
		uint32_t chunks = m_size / NR_DPUS;
		rows_per_dpu = chunks;
		uint32_t rest_rows = m_size % NR_DPUS;
		if (i < rest_rows)
			rows_per_dpu++;
		if (rest_rows > 0) {
			if (i >= rest_rows)
				prev_rows_dpu = rest_rows * (chunks + 1) + (i - rest_rows) * chunks;
			else
				prev_rows_dpu = i * (chunks + 1);
		} else {
			prev_rows_dpu = i * chunks;
		}

		// Keep max rows for parallel transfers
		uint32_t rows_per_dpu_pad = rows_per_dpu;
		if (rows_per_dpu_pad % 2 == 1) // 4-byte elements
			rows_per_dpu_pad++;
		if (rows_per_dpu_pad > max_rows_per_dpu)
			max_rows_per_dpu = rows_per_dpu_pad;

		dpu_info[i].rows_per_dpu = rows_per_dpu;
		dpu_info[i].rows_per_dpu_pad = rows_per_dpu_pad;
		dpu_info[i].prev_rows_dpu = prev_rows_dpu;

		// Copy input arguments to DPU
		input_args[i].n_size = n_size;
		input_args[i].n_size_pad = n_size_pad;
		input_args[i].nr_rows = rows_per_dpu;
	}

	A = malloc(max_rows_per_dpu * NR_DPUS * n_size_pad * sizeof(T));
	B = malloc(n_size_pad * sizeof(T));
	C = malloc(max_rows_per_dpu * NR_DPUS * sizeof(T));
	C_dpu = malloc(max_rows_per_dpu * NR_DPUS * sizeof(T));

	// Initialize data with arbitrary data
	init_data(A, B, m_size, n_size);

	// Compute output on CPU (performance comparison and verification purposes)
	for (unsigned int rep = 0; rep < p.n_warmup + p.n_reps; rep++) {

#if WITH_ALLOC_OVERHEAD
		if(rep >= p.n_warmup) {
			start(&timer, 0, 0);
		}
		DPU_ASSERT(dpu_alloc(NR_DPUS, NULL, &dpu_set));
		if(rep >= p.n_warmup) {
			stop(&timer, 0);
		}
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
		assert(nr_of_dpus == NR_DPUS);
#endif

		if(rep >= p.n_warmup) {
			start(&timer, 2, 0);
		}
		gemv_host(C, A, B, m_size, n_size);
		if(rep >= p.n_warmup) {
			stop(&timer, 2);
		}
		if (rep >= p.n_warmup) {
			start(&timer, 3, 0);
		}
		// Input arguments
		i = 0;
		DPU_FOREACH(dpu_set, dpu, i) {
			// Copy input arguments to DPU
			input_args[i].max_rows = max_rows_per_dpu;

			DPU_ASSERT(dpu_prepare_xfer(dpu, input_args + i));
		}

		DPU_ASSERT(dpu_push_xfer(dpu_set, DPU_XFER_TO_DPU, "DPU_INPUT_ARGUMENTS", 0, sizeof(dpu_arguments_t), DPU_XFER_DEFAULT));

		// Copy input array and vector
		i = 0;
		DPU_FOREACH(dpu_set, dpu, i) {
			DPU_ASSERT(dpu_prepare_xfer(dpu, A + dpu_info[i].prev_rows_dpu * n_size));
		}
		DPU_ASSERT(dpu_push_xfer(dpu_set, DPU_XFER_TO_DPU, DPU_MRAM_HEAP_POINTER_NAME, 0, max_rows_per_dpu * n_size_pad * sizeof(T), DPU_XFER_DEFAULT));
		DPU_FOREACH(dpu_set, dpu, i) {
			DPU_ASSERT(dpu_prepare_xfer(dpu, B));
		}
		DPU_ASSERT(dpu_push_xfer(dpu_set, DPU_XFER_TO_DPU, DPU_MRAM_HEAP_POINTER_NAME, max_rows_per_dpu * n_size_pad * sizeof(T) , n_size_pad * sizeof(T), DPU_XFER_DEFAULT));

		if (rep >= p.n_warmup) {
			stop(&timer, 3);
		}

		// Run kernel on DPUs
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
		// Display DPU Logs
		DPU_FOREACH(dpu_set, dpu) {
			DPU_ASSERT(dpulog_read_for_dpu(dpu.dpu, stdout));
		}
#endif

		// Retrieve results
		if (rep >= p.n_warmup)
			start(&timer, 5, 0);
		i = 0;
		DPU_FOREACH(dpu_set, dpu, i) {
			DPU_ASSERT(dpu_prepare_xfer(dpu, C_dpu + i * max_rows_per_dpu));
		}
		DPU_ASSERT(dpu_push_xfer(dpu_set, DPU_XFER_FROM_DPU, DPU_MRAM_HEAP_POINTER_NAME, max_rows_per_dpu * n_size_pad * sizeof(T) + n_size_pad * sizeof(T), max_rows_per_dpu * sizeof(T), DPU_XFER_DEFAULT));
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


		// Check output
		bool status = true;
		unsigned int n,j;
		i = 0;
		for (n = 0; n < NR_DPUS; n++) {
			for (j = 0; j < dpu_info[n].rows_per_dpu; j++) {
				if(C[i] != C_dpu[n * max_rows_per_dpu + j]) {
					status = false;
#if PRINT
		//			printf("%d: %d -- %d\n", i, C[i], C_dpu[n * max_rows_per_dpu + j]);
#endif
				}
				i++;
			}
		}
		if (status) {
			printf("[" ANSI_COLOR_GREEN "OK" ANSI_COLOR_RESET "] Outputs are equal\n");
			if (rep >= p.n_warmup) {
				printf("[::] GEMV UPMEM | n_dpus=%d n_tasklets=%d e_type=%s block_size_B=%d n_elements=%d",
					nr_of_dpus, NR_TASKLETS, XSTR(T), BLOCK_SIZE, n_size * m_size);
				printf(" b_with_alloc_overhead=%d b_with_load_overhead=%d b_with_free_overhead=%d ",
					WITH_ALLOC_OVERHEAD, WITH_LOAD_OVERHEAD, WITH_FREE_OVERHEAD);
				printf("| latency_alloc_us=%f latency_load_us=%f latency_cpu_us=%f latency_write_us=%f latency_kernel_us=%f latency_read_us=%f latency_free_us=%f",
					timer.time[0],
					timer.time[1],
					timer.time[2],
					timer.time[3],
					timer.time[4],
					timer.time[5],
					timer.time[6]);
				printf(" throughput_cpu_MBps=%f throughput_upmem_kernel_MBps=%f throughput_upmem_total_MBps=%f",
					n_size * m_size * sizeof(T) / timer.time[2],
					n_size * m_size * sizeof(T) / (timer.time[4]),
					n_size * m_size * sizeof(T) / (timer.time[0] + timer.time[1] + timer.time[3] + timer.time[4] + timer.time[5] + timer.time[6]));
				printf(" throughput_upmem_wxr_MBps=%f throughput_upmem_lwxr_MBps=%f throughput_upmem_alwxr_MBps=%f",
					n_size * m_size * sizeof(T) / (timer.time[3] + timer.time[4] + timer.time[5]),
					n_size * m_size * sizeof(T) / (timer.time[1] + timer.time[3] + timer.time[4] + timer.time[5]),
					n_size * m_size * sizeof(T) / (timer.time[0] + timer.time[1] + timer.time[3] + timer.time[4] + timer.time[5]));
				printf(" throughput_cpu_MOpps=%f throughput_upmem_kernel_MOpps=%f throughput_upmem_total_MOpps=%f",
					n_size * m_size / timer.time[2],
					n_size * m_size / (timer.time[4]),
					n_size * m_size / (timer.time[0] + timer.time[1] + timer.time[3] + timer.time[4] + timer.time[5] + timer.time[6]));
				printf(" throughput_upmem_wxr_MOpps=%f throughput_upmem_lwxr_MOpps=%f throughput_upmem_alwxr_MOpps=%f\n",
					n_size * m_size / (timer.time[3] + timer.time[4] + timer.time[5]),
					n_size * m_size / (timer.time[1] + timer.time[3] + timer.time[4] + timer.time[5]),
					n_size * m_size / (timer.time[0] + timer.time[1] + timer.time[3] + timer.time[4] + timer.time[5]));
			}
		} else {
			printf("[" ANSI_COLOR_RED "ERROR" ANSI_COLOR_RESET "] Outputs differ!\n");
		}

	}
#if ENERGY
	double acc_energy, avg_energy, acc_time, avg_time;
	DPU_ASSERT(dpu_probe_get(&probe, DPU_ENERGY, DPU_ACCUMULATE, &acc_energy));
	DPU_ASSERT(dpu_probe_get(&probe, DPU_ENERGY, DPU_AVERAGE, &avg_energy));
	DPU_ASSERT(dpu_probe_get(&probe, DPU_TIME, DPU_ACCUMULATE, &acc_time));
	DPU_ASSERT(dpu_probe_get(&probe, DPU_TIME, DPU_AVERAGE, &avg_time));
#endif

	// Print timing results
	/*
	printf("CPU Version Time (ms): ");
	print(&timer, 0, 1);
	printf("CPU-DPU Time (ms): ");
	print(&timer, 1, p.n_reps);
	printf("DPU Kernel Time (ms): ");
	print(&timer, 2, p.n_reps);
	printf("DPU-CPU Time (ms): ");
	print(&timer, 3, p.n_reps);
	*/

#if ENERGY
	printf("Energy (J): %f J\t", avg_energy);
#endif

	// Deallocation
	free(A);
	free(B);
	free(C);
	free(C_dpu);
#if !WITH_ALLOC_OVERHEAD
	DPU_ASSERT(dpu_free(dpu_set));
#endif

#if ENERGY
	DPU_ASSERT(dpu_probe_deinit(&probe));
#endif

	return 0;
}
