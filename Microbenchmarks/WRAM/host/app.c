/**
* app.c
* WRAM Access Host Application Source File
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
static unsigned int* A;
static T* B;
static T* C;
static T* C2;

// Create input arrays
#ifdef strided
static void read_input(unsigned int* A, T* B, unsigned int nr_elements, unsigned int stride) {
#else
static void read_input(unsigned int* A, T* B, unsigned int nr_elements) {
#endif
    srand(0);
    for (unsigned int i = 0; i < nr_elements; i++) {
#ifdef streaming
        A[i] = i % (BLOCK_SIZE >> DIV);
#elif strided
        A[i] = ((i>0 ? A[i-1]:0) + stride) % (BLOCK_SIZE >> DIV);
#else
        A[i] = ((unsigned int)rand()) % (BLOCK_SIZE >> DIV);
#endif
        B[i] = (T)(rand());
        C[i] = 0;
    }
}

#ifdef streaming
char transfer_mode[] = "streaming";
#elif strided
char transfer_mode[] = "strided";
#else
char transfer_mode[] = "random";
#endif

// Compute output in the host
static void copy_host(T* C, T* B, unsigned int* A, unsigned int nr_elements) {
    unsigned int wram_size = BLOCK_SIZE >> DIV;
    for (unsigned int i = 0; i < nr_elements / wram_size; i++) {
        for (unsigned int j = 0; j < wram_size; j++) {
            unsigned int address = A[i * wram_size + j];
            C[i * wram_size + address] = B[i * wram_size + address];
        }
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
    double cc = 0;
    double cc_min = 0;
    const unsigned int input_size = p.exp == 0 ? p.input_size * nr_of_dpus : p.input_size;

    // Input/output allocation
    A = malloc(input_size * sizeof(unsigned int));
    unsigned int *bufferA = A;
    B = malloc(input_size * sizeof(T));
    T *bufferB = B;
    C = malloc(input_size * sizeof(T));
    T *bufferC = C;
    C2 = malloc(input_size * sizeof(T));

    // Create an input file with arbitrary data
#ifdef strided
    read_input(A, B, input_size, p.stride);
#else
    read_input(A, B, input_size);
#endif

    // Timer declaration
    Timer timer;

    // Loop over main kernel
    for(int rep = 0; rep < p.n_warmup + p.n_reps; rep++) {

        // Compute output on CPU (performance comparison and verification purposes)
        if(rep >= p.n_warmup)
            start(&timer, 0, rep - p.n_warmup);
        copy_host(C2, B, A, input_size);
        if(rep >= p.n_warmup)
            stop(&timer, 0);

        if(rep >= p.n_warmup)
            start(&timer, 1, rep - p.n_warmup);
        // Input arguments
        const unsigned int input_size_dpu = input_size / nr_of_dpus;
        unsigned int kernel = 0;
        dpu_arguments_t input_arguments = {input_size_dpu, kernel};
        DPU_ASSERT(dpu_copy_to(dpu_set, "DPU_INPUT_ARGUMENTS", 0, (const void *)&input_arguments, sizeof(input_arguments)));
        // Copy input arrays
        i = 0;
        DPU_FOREACH (dpu_set, dpu) {
            DPU_ASSERT(dpu_copy_to(dpu, DPU_MRAM_HEAP_POINTER_NAME, 0, bufferA + input_size_dpu * i, input_size_dpu * sizeof(unsigned int)));
            DPU_ASSERT(dpu_copy_to(dpu, DPU_MRAM_HEAP_POINTER_NAME, input_size_dpu * sizeof(unsigned int), bufferB + input_size_dpu * i, input_size_dpu * sizeof(T)));
            DPU_ASSERT(dpu_copy_to(dpu, DPU_MRAM_HEAP_POINTER_NAME, input_size_dpu * (sizeof(unsigned int) + sizeof(T)), bufferC + input_size_dpu * i, input_size_dpu * sizeof(T)));
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
        dpu_results_t results[nr_of_dpus];
        i = 0;
        DPU_FOREACH (dpu_set, dpu) {
            // Copy output array
            DPU_ASSERT(dpu_copy_from(dpu, DPU_MRAM_HEAP_POINTER_NAME, input_size_dpu * (sizeof(unsigned int) + sizeof(T)), bufferC + input_size_dpu * i, input_size_dpu * sizeof(T)));
			
#if PERF
            results[i].cycles = 0;
            DPU_ASSERT(dpu_copy_from(dpu, "CLOCKS_PER_SEC", 0, &clocks_per_sec, sizeof(clocks_per_sec)));
            // Retrieve tasklet timings
            for (unsigned int each_tasklet = 0; each_tasklet < NR_TASKLETS; each_tasklet++) {
                dpu_results_t result;
                result.cycles = 0;
                result.count = 0;
                DPU_ASSERT(dpu_copy_from(dpu, "DPU_RESULTS", each_tasklet * sizeof(dpu_results_t), &result, sizeof(dpu_results_t)));
                printf("[::] COPY UPMEM | n_dpus=%d n_ranks=%d n_tasklets=%d e_type=%s n_elements=%u e_mode=%s block_size_B=%d"
                    " | latency_block_copy_us=%f throughput_dpu_copy_MBps=%f throughput_tasklet_copy_MBps=%f\n",
                    nr_of_dpus, nr_of_ranks, NR_TASKLETS, XSTR(T), input_size_dpu, transfer_mode, BLOCK_SIZE,
                    ((double)result.cycles * 1e6 / clocks_per_sec) / result.count,
                    input_size_dpu * sizeof(T) / ((double)result.cycles * 1e6 / clocks_per_sec),
                    input_size_dpu * sizeof(T) / ((double)result.cycles * 1e6 * NR_TASKLETS / clocks_per_sec));
                if (result.cycles > results[i].cycles)
                    results[i].cycles = result.cycles;
            }
#endif
            i++;
        }
        if(rep >= p.n_warmup)
            stop(&timer, 3);

#if PERF
        uint64_t max_cycles = 0;
        uint64_t min_cycles = 0xFFFFFFFFFFFFFFFF;
        // Print performance results
        if(rep >= p.n_warmup){
            i = 0;
            DPU_FOREACH(dpu_set, dpu) {
                if(results[i].cycles > max_cycles)
                    max_cycles = results[i].cycles;
                if(results[i].cycles < min_cycles)
                    min_cycles = results[i].cycles;
                i++;
            }
            cc += (double)max_cycles;
            cc_min += (double)min_cycles;
        }
#endif

    }

    // Check output
    bool status = true;
    for (i = 0; i < input_size; i++) {
        if(C2[i] != bufferC[i]){ 
            status = false;
#if PRINT
            printf("%d: %u -- %u\n", i, C2[i], bufferC[i]);
#endif
        }
    }

    // Deallocation
    free(A);
    free(B);
    free(C);
    free(C2);
    DPU_ASSERT(dpu_free(dpu_set));
	
    return status ? 0 : -1;
}
