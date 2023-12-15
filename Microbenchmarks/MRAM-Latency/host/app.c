/**
* app.c
* MRAM Latency Host Application Source File
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

// Pointer declaration
static T* A;
static T* B;
static T* C2;

// Create input arrays
static void read_input(T* A, T* B, unsigned int nr_elements) {
    srand(0);
    for (unsigned int i = 0; i < nr_elements; i++) {
        A[i] = (T) (rand());
        B[i] = (T) (rand());
    }
}

// Compute output in the host
static void stream_host(T* C, T* A, unsigned int nr_elements) {
    for (unsigned int i = 0; i < nr_elements; i++) {
        C[i] = A[i];
    }
}

// Main of the Host Application
int main(int argc, char **argv) {

    struct Params p = input_params(argc, argv);

    struct dpu_set_t dpu_set, dpu;
    uint32_t clocks_per_sec;
    uint32_t nr_of_dpus;
    uint32_t nr_of_ranks;
    
    // Allocate DPUs and load binary
    DPU_ASSERT(dpu_alloc(NR_DPUS, NULL, &dpu_set));
    DPU_ASSERT(dpu_load(dpu_set, DPU_BINARY, NULL));
    DPU_ASSERT(dpu_get_nr_dpus(dpu_set, &nr_of_dpus));
    DPU_ASSERT(dpu_get_nr_ranks(dpu_set, &nr_of_ranks));

    unsigned int i = 0;
    const unsigned int input_size = p.exp == 0 ? p.input_size * nr_of_dpus : p.input_size;
    assert(input_size % (nr_of_dpus * NR_TASKLETS) == 0 && "Input size!");

    // Input/output allocation
    A = malloc(input_size * sizeof(T));
    B = malloc(input_size * sizeof(T));
    T *bufferA = A;
    T *bufferB = B;
    C2 = malloc(input_size * sizeof(T));

    // Create an input file with arbitrary data
    read_input(A, B, input_size);

    // Timer declaration
    Timer timer;

    // Loop over main kernel
    for(int rep = 0; rep < p.n_warmup + p.n_reps; rep++) {

        // Compute output on CPU (performance comparison and verification purposes)
        if(rep >= p.n_warmup)
            start(&timer, 0, rep - p.n_warmup);
        stream_host(C2, A, input_size);
        if(rep >= p.n_warmup)
            stop(&timer, 0);

        if(rep >= p.n_warmup)
            start(&timer, 1, rep - p.n_warmup);
        // Input arguments
        const unsigned int input_size_dpu = input_size / nr_of_dpus;
        unsigned int kernel = 0;
        dpu_arguments_t input_arguments = {input_size_dpu * sizeof(T), kernel};
        DPU_ASSERT(dpu_copy_to(dpu_set, "DPU_INPUT_ARGUMENTS", 0, (const void *)&input_arguments, sizeof(input_arguments)));
        // Copy input arrays
        i = 0;
        DPU_FOREACH (dpu_set, dpu) {
            DPU_ASSERT(dpu_copy_to(dpu, DPU_MRAM_HEAP_POINTER_NAME, 0, bufferA + input_size_dpu * i, input_size_dpu * sizeof(T)));
            i++;
        }
        if(rep >= p.n_warmup)
            stop(&timer, 1);

        // Run DPU kernel
        if(rep >= p.n_warmup)
            start(&timer, 2, rep - p.n_warmup);
        DPU_ASSERT(dpu_launch(dpu_set, DPU_SYNCHRONOUS));
        if(rep >= p.n_warmup)
            stop(&timer, 2);

#if PRINT
        {
            unsigned int each_dpu = 0;
            printf("Display DPU Logs\n");
            DPU_FOREACH (dpu_set, dpu) {
                printf("DPU#%d:\n", each_dpu);
                DPU_ASSERT(dpulog_read_for_dpu(dpu.dpu, stdout));
                each_dpu++;
            }
        }
#endif

        if(rep >= p.n_warmup)
            start(&timer, 3, rep - p.n_warmup);
        i = 0;
        DPU_FOREACH (dpu_set, dpu) {
            // Copy output array
            DPU_ASSERT(dpu_copy_from(dpu, DPU_MRAM_HEAP_POINTER_NAME, input_size_dpu * sizeof(T), bufferB + input_size_dpu * i, input_size_dpu * sizeof(T)));
			
#if PERF
            DPU_ASSERT(dpu_copy_from(dpu, "CLOCKS_PER_SEC", 0, &clocks_per_sec, sizeof(clocks_per_sec)));
            // Retrieve tasklet timings
            for (unsigned int each_tasklet = 0; each_tasklet < NR_TASKLETS; each_tasklet++) {
                dpu_results_t result;
                result.count = 0;
                result.r_cycles = 0;
                result.w_cycles = 0;
                DPU_ASSERT(dpu_copy_from(dpu, "DPU_RESULTS", each_tasklet * sizeof(dpu_results_t), &result, sizeof(dpu_results_t)));
                printf("[::] DMA UPMEM | n_dpus=%d n_ranks=%d n_tasklets=%d e_type=%s n_elements=%u block_size_B=%d"
                    " | latency_mram_read_us=%f latency_mram_write_us=%f"
                    " throughput_dpu_mram_read_MBps=%f throughput_dpu_mram_write_MBps=%f"
                    " throughput_mram_read_MBps=%f throughput_mram_write_MBps=%f\n",
                    nr_of_dpus, nr_of_ranks, NR_TASKLETS, XSTR(T), input_size_dpu, BLOCK_SIZE,
                    ((double)result.r_cycles * 1e6 / clocks_per_sec) / result.count,
                    ((double)result.w_cycles * 1e6 / clocks_per_sec) / result.count,
                    input_size * sizeof(T) / ((double)result.r_cycles * 1e6 / clocks_per_sec),
                    input_size * sizeof(T) / ((double)result.w_cycles * 1e6 / clocks_per_sec),
                    input_size * sizeof(T) / ((double)result.r_cycles * 1e6 * NR_TASKLETS / clocks_per_sec),
                    input_size * sizeof(T) / ((double)result.w_cycles * 1e6 * NR_TASKLETS / clocks_per_sec));
            }
#endif
            i++;
        }
        if(rep >= p.n_warmup)
            stop(&timer, 3);

    }

    // Check output
    bool status = true;
    for (i = 0; i < input_size; i++) {
        if(C2[i] != bufferB[i]){ 
            status = false;
#if PRINT
            printf("%d: %u -- %u\n", i, C2[i], bufferB[i]);
#endif
        }
    }
    if (status) {
    } else {
        printf("[" ANSI_COLOR_RED "ERROR" ANSI_COLOR_RESET "] Outputs differ!\n");
    }

    // Deallocation
    free(A);
    free(B);
    free(C2);
    DPU_ASSERT(dpu_free(dpu_set));
	
    return status ? 0 : -1;
}
