/**
* app.c
* STREAM Host Application Source File
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

#if WITH_DPUINFO
#include <dpu_management.h>
#include <dpu_target_macros.h>
#endif

#if SDK_SINGLETHREADED
#define DPU_ALLOC_PROFILE "nrThreadsPerRank=0"
#else
#define DPU_ALLOC_PROFILE NULL
#endif

// Pointer declaration
static T* A;
static T* B;
#if defined(add) || defined(triad)
static T* C;
#endif
static T* C2;

static const char transfer_mode[] =
#if SERIAL
"SERIAL"
#else
"PUSH"
#endif
;

// Create input arrays
static void read_input(T* A, T* B, unsigned int nr_elements) {
    srand(0);
    printf("nr_elements\t%u\t", nr_elements);
    for (unsigned int i = 0; i < nr_elements; i++) {
        A[i] = (T) (rand());
        B[i] = (T) (rand());
    }
}

static char benchmark_name[] =
#ifdef scale
"SCALE"
#elif add
"ADD"
#elif triad
"TRIAD"
#elif copy
"COPY"
#elif copyw
"COPYW"
#endif
;

static char mem_name[] =
#ifdef WRAM
"WRAM"
#endif
#ifdef MRAM
"MRAM"
#endif
;

// Compute output in the host
#if defined(add) || defined(triad)
static void stream_host(T* C, T* B, T* A, unsigned int nr_elements) {
#else
static void stream_host(T* C, T* A, unsigned int nr_elements) {
#endif
    for (unsigned int i = 0; i < nr_elements; i++) {
#ifdef scale
        C[i] = (nr_elements / NR_DPUS) * A[i];
#elif add
        C[i] = A[i] + B[i];
#elif triad
        C[i] = A[i] + (nr_elements / NR_DPUS) * B[i];
#else // copy
        C[i] = A[i];
#endif
    }
}

// Main of the Host Application
int main(int argc, char **argv) {

    struct Params p = input_params(argc, argv);

    struct dpu_set_t dpu_set, dpu;
    uint32_t nr_of_dpus;
    uint32_t nr_of_ranks;

    printf("WITH_ALLOC_OVERHEAD=%d WITH_LOAD_OVERHEAD=%d WITH_FREE_OVERHEAD=%d\n", WITH_ALLOC_OVERHEAD, WITH_LOAD_OVERHEAD, WITH_FREE_OVERHEAD);

    // Timer declaration
    Timer timer;

    // Allocate DPUs and load binary
#if !WITH_ALLOC_OVERHEAD
    DPU_ASSERT(dpu_alloc(NR_DPUS, DPU_ALLOC_PROFILE, &dpu_set));
    timer.nanoseconds[0] = 0; // alloc
#endif
#if !WITH_LOAD_OVERHEAD
    DPU_ASSERT(dpu_load(dpu_set, DPU_BINARY, NULL));
    DPU_ASSERT(dpu_get_nr_dpus(dpu_set, &nr_of_dpus));
    DPU_ASSERT(dpu_get_nr_ranks(dpu_set, &nr_of_ranks));
    assert(nr_of_dpus == NR_DPUS);
    timer.nanoseconds[1] = 0; // load
#endif
#if !WITH_FREE_OVERHEAD
    timer.nanoseconds[6] = 0; // free
#endif

    unsigned int i = 0;
    double cc = 0;
    double cc_min = 0;
    const unsigned int input_size = p.exp == 0 ? p.input_size * NR_DPUS : p.input_size;

#if defined(add) || defined(triad)
    const unsigned int n_arrays = 2;
#else
    const unsigned int n_arrays = 1;
#endif

    // Input/output allocation
    A = malloc(input_size * sizeof(T));
    B = malloc(input_size * sizeof(T));
    T *bufferA = A;
    T *bufferB = B;
#if defined(add) || defined(triad)
    C = malloc(input_size * sizeof(T));
    T *bufferC = C;
#endif
    C2 = malloc(input_size * sizeof(T));

    // Create an input file with arbitrary data
    read_input(A, B, input_size);

    // Loop over main kernel
    for(int rep = 0; rep < p.n_warmup + p.n_reps; rep++) {

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

        // Compute output on CPU (performance comparison and verification purposes)
        if(rep >= p.n_warmup) {
            start(&timer, 2, 0);
        }
#if defined(add) || defined(triad)
        stream_host(C2, B, A, input_size);
#else
        stream_host(C2, A, input_size);
#endif
        if(rep >= p.n_warmup) {
            stop(&timer, 2);
        }

        if(rep >= p.n_warmup) {
            start(&timer, 3, 0);
        }
        // Input arguments
        const unsigned int input_size_dpu = input_size / NR_DPUS;
        unsigned int kernel = 0;
        dpu_arguments_t input_arguments = {input_size_dpu * sizeof(T), kernel};
        DPU_ASSERT(dpu_copy_to(dpu_set, "DPU_INPUT_ARGUMENTS", 0, (const void *)&input_arguments, sizeof(input_arguments)));
        // Copy input arrays
#ifdef SERIAL
        DPU_FOREACH (dpu_set, dpu, i) {
            DPU_ASSERT(dpu_copy_to(dpu, DPU_MRAM_HEAP_POINTER_NAME, 0, bufferA + input_size_dpu * i, input_size_dpu * sizeof(T)));
#if defined(add) || defined(triad)
            DPU_ASSERT(dpu_copy_to(dpu, DPU_MRAM_HEAP_POINTER_NAME, input_size_dpu * sizeof(T), bufferB + input_size_dpu * i, input_size_dpu * sizeof(T)));
#endif
        }
#else
        DPU_FOREACH (dpu_set, dpu, i) {
            DPU_ASSERT(dpu_prepare_xfer(dpu, bufferA + input_size_dpu * i));
        }
        DPU_ASSERT(dpu_push_xfer(dpu_set, DPU_XFER_TO_DPU, DPU_MRAM_HEAP_POINTER_NAME, 0, input_size_dpu * sizeof(T), DPU_XFER_DEFAULT));
#if defined(add) || defined(triad)
        DPU_FOREACH (dpu_set, dpu, i) {
            DPU_ASSERT(dpu_prepare_xfer(dpu, bufferB + input_size_dpu * i));
        }
        DPU_ASSERT(dpu_push_xfer(dpu_set, DPU_XFER_TO_DPU, DPU_MRAM_HEAP_POINTER_NAME, input_size_dpu * sizeof(T), input_size_dpu * sizeof(T), DPU_XFER_DEFAULT));
#endif
#endif
        if(rep >= p.n_warmup) {
            stop(&timer, 3);
        }

        // Run DPU kernel
        if(rep >= p.n_warmup) {
            start(&timer, 4, 0);
        }
        DPU_ASSERT(dpu_launch(dpu_set, DPU_SYNCHRONOUS));
        if(rep >= p.n_warmup) {
            stop(&timer, 4);
        }

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

        if(rep >= p.n_warmup) {
            start(&timer, 5, 0);
        }

        dpu_results_t results[NR_DPUS];

#ifdef SERIAL
        DPU_FOREACH (dpu_set, dpu, i) {
            // Copy output array
#if defined(add) || defined(triad)
            DPU_ASSERT(dpu_copy_from(dpu, DPU_MRAM_HEAP_POINTER_NAME, 2 * input_size_dpu * sizeof(T), bufferC + input_size_dpu * i, input_size_dpu * sizeof(T)));
#else
            DPU_ASSERT(dpu_copy_from(dpu, DPU_MRAM_HEAP_POINTER_NAME, input_size_dpu * sizeof(T), bufferB + input_size_dpu * i, input_size_dpu * sizeof(T)));
#endif
        }
#else
        DPU_FOREACH (dpu_set, dpu, i) {
#if defined(add) || defined(triad)
            DPU_ASSERT(dpu_prepare_xfer(dpu, bufferC + input_size_dpu * i));
#else
            DPU_ASSERT(dpu_prepare_xfer(dpu, bufferB + input_size_dpu * i));
#endif
        }
#if defined(add) || defined(triad)
        DPU_ASSERT(dpu_push_xfer(dpu_set, DPU_XFER_FROM_DPU, DPU_MRAM_HEAP_POINTER_NAME, 2 * input_size_dpu * sizeof(T), input_size_dpu * sizeof(T), DPU_XFER_DEFAULT));
#else
        DPU_ASSERT(dpu_push_xfer(dpu_set, DPU_XFER_FROM_DPU, DPU_MRAM_HEAP_POINTER_NAME, input_size_dpu * sizeof(T), input_size_dpu * sizeof(T), DPU_XFER_DEFAULT));
#endif
#endif

        if(rep >= p.n_warmup) {
            stop(&timer, 5);
        }

#if PERF
        i = 0;
        DPU_FOREACH (dpu_set, dpu) {
            results[i].cycles = 0;
            // Retrieve tasklet timings
            for (unsigned int each_tasklet = 0; each_tasklet < NR_TASKLETS; each_tasklet++) {
                dpu_results_t result;
                result.cycles = 0;
                DPU_ASSERT(dpu_copy_from(dpu, "DPU_RESULTS", each_tasklet * sizeof(dpu_results_t), &result, sizeof(dpu_results_t)));
                if (result.cycles > results[i].cycles)
                    results[i].cycles = result.cycles;
            }
            i++;
        }
#endif

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
        for (i = 0; i < input_size; i++) {
#if defined(add) || defined(triad)
            if(C2[i] != bufferC[i]){
#else
            if(C2[i] != bufferB[i]){
#endif
                status = false;
#if PRINT
#if defined(add) || defined(triad)
                printf("%d: %u -- %u\n", i, C2[i], bufferC[i]);
#else
                printf("%d: %u -- %u\n", i, C2[i], bufferB[i]);
#endif
#endif
            }
        }

        if (status) {
            printf("[" ANSI_COLOR_GREEN "OK" ANSI_COLOR_RESET "] Outputs are equal\n");
            printf("[::] STREAM UPMEM | n_dpus=%d n_ranks=%d n_tasklets=%d e_benchmark=%-6s e_type=%s e_mem=%s e_mode=%s b_unroll=%d block_size_B=%d n_elements=%d n_elements_per_dpu=%d b_sdk_singlethreaded=%d ",
                NR_DPUS, nr_of_ranks, NR_TASKLETS, benchmark_name, XSTR(T), mem_name, transfer_mode, UNROLL, BLOCK_SIZE, input_size, input_size / NR_DPUS, SDK_SINGLETHREADED);
            printf("| latency_alloc_ns=%lu latency_load_ns=%lu latency_cpu_ns=%lu latency_write_ns=%lu latency_kernel_ns=%lu latency_read_ns=%lu latency_free_ns=%lu",
                timer.nanoseconds[0],
                timer.nanoseconds[1],
                timer.nanoseconds[2],
                timer.nanoseconds[3],
                timer.nanoseconds[4],
                timer.nanoseconds[5],
                timer.nanoseconds[6]);
            printf(" throughput_cpu_Bps=%f throughput_upmem_kernel_Bps=%f throughput_upmem_total_Bps=%f",
                input_size * n_arrays * sizeof(T) * 1e9 / timer.nanoseconds[2],
                input_size * n_arrays * sizeof(T) * 1e9 / (timer.nanoseconds[4]),
                input_size * n_arrays * sizeof(T) * 1e9 / (timer.nanoseconds[0] + timer.nanoseconds[1] + timer.nanoseconds[3] + timer.nanoseconds[4] + timer.nanoseconds[5] + timer.nanoseconds[6]));
            printf(" throughput_upmem_wxr_Bps=%f throughput_upmem_lwxr_Bps=%f throughput_upmem_alwxr_Bps=%f",
                input_size * n_arrays * sizeof(T) * 1e9 / (timer.nanoseconds[3] + timer.nanoseconds[4] + timer.nanoseconds[5]),
                input_size * n_arrays * sizeof(T) * 1e9 / (timer.nanoseconds[1] + timer.nanoseconds[3] + timer.nanoseconds[4] + timer.nanoseconds[5]),
                input_size * n_arrays * sizeof(T) * 1e9 / (timer.nanoseconds[0] + timer.nanoseconds[1] + timer.nanoseconds[3] + timer.nanoseconds[4] + timer.nanoseconds[5]));
            printf(" throughput_cpu_Opps=%f throughput_upmem_kernel_Opps=%f throughput_upmem_total_Opps=%f",
                input_size * 1e9 / timer.nanoseconds[2],
                input_size * 1e9 / (timer.nanoseconds[4]),
                input_size * 1e9 / (timer.nanoseconds[0] + timer.nanoseconds[1] + timer.nanoseconds[3] + timer.nanoseconds[4] + timer.nanoseconds[5] + timer.nanoseconds[6]));
            printf(" throughput_upmem_wxr_Opps=%f throughput_upmem_lwxr_Opps=%f throughput_upmem_alwxr_Opps=%f\n",
                input_size * 1e9 / (timer.nanoseconds[3] + timer.nanoseconds[4] + timer.nanoseconds[5]),
                input_size * 1e9 / (timer.nanoseconds[1] + timer.nanoseconds[3] + timer.nanoseconds[4] + timer.nanoseconds[5]),
                input_size * 1e9 / (timer.nanoseconds[0] + timer.nanoseconds[1] + timer.nanoseconds[3] + timer.nanoseconds[4] + timer.nanoseconds[5]));
        } else {
            printf("[" ANSI_COLOR_RED "ERROR" ANSI_COLOR_RESET "] Outputs differ!\n");
        }
        fflush(stdout);

    }

    // Deallocation
    free(A);
    free(B);
#if defined(add) || defined(triad)
    free(C);
#endif
    free(C2);
#if !WITH_ALLOC_OVERHEAD
    DPU_ASSERT(dpu_free(dpu_set));
#endif
	
    return 0;
}
