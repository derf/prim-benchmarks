/**
* app.c
* RED Host Application Source File
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

// Pointer declaration
static T* A;

// Create input arrays
static void read_input(T* A, unsigned int nr_elements) {
    srand(0);
    //printf("nr_elements\t%u\t", nr_elements);
    for (unsigned int i = 0; i < nr_elements; i++) {
        A[i] = (T)(rand());
    }
}

// Compute output in the host
static T reduction_host(T* A, unsigned int nr_elements) {
    T count = 0;
    for (unsigned int i = 0; i < nr_elements; i++) {
        count += A[i];
    }
    return count;
}

// Main of the Host Application
int main(int argc, char **argv) {

    struct Params p = input_params(argc, argv);

    struct dpu_set_t dpu_set, dpu;
    uint32_t nr_of_dpus;
    uint32_t nr_of_ranks;

    // Timer declaration
    Timer timer;

    // Allocate DPUs and load binary
#if !WITH_ALLOC_OVERHEAD
    DPU_ASSERT(dpu_alloc(NR_DPUS, NULL, &dpu_set));
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

    unsigned int i = 0;
#if PERF
    double cc = 0;
    double cc_min = 0;
#endif

    const unsigned int input_size = p.exp == 0 ? p.input_size * NR_DPUS : p.input_size; // Total input size (weak or strong scaling)
    const unsigned int input_size_8bytes = 
        ((input_size * sizeof(T)) % 8) != 0 ? roundup(input_size, 8) : input_size; // Input size per DPU (max.), 8-byte aligned
    const unsigned int input_size_dpu = divceil(input_size, NR_DPUS); // Input size per DPU (max.)
    const unsigned int input_size_dpu_8bytes = 
        ((input_size_dpu * sizeof(T)) % 8) != 0 ? roundup(input_size_dpu, 8) : input_size_dpu; // Input size per DPU (max.), 8-byte aligned

    // Input/output allocation
    A = malloc(input_size_dpu_8bytes * NR_DPUS * sizeof(T));
    T *bufferA = A;
    T count = 0;
    T count_host = 0;

    // Create an input file with arbitrary data
    read_input(A, input_size);

    //printf("NR_TASKLETS\t%d\tBL\t%d\n", NR_TASKLETS, BL);

    // Loop over main kernel
    for(int rep = 0; rep < p.n_warmup + p.n_reps; rep++) {

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
        DPU_ASSERT(dpu_get_nr_ranks(dpu_set, &nr_of_ranks));
        assert(nr_of_dpus == NR_DPUS);
#endif

        // Compute output on CPU (performance comparison and verification purposes)
        if(rep >= p.n_warmup)
            start(&timer, 2, 0);
        count_host = reduction_host(A, input_size);
        if(rep >= p.n_warmup)
            stop(&timer, 2);

        //printf("Load input data\n");
        if(rep >= p.n_warmup)
            start(&timer, 3, 0);
        count = 0;
        // Input arguments
        unsigned int kernel = 0;
        dpu_arguments_t input_arguments[NR_DPUS];
        for(i=0; i<NR_DPUS-1; i++) {
            input_arguments[i].size=input_size_dpu_8bytes * sizeof(T); 
            input_arguments[i].kernel=kernel;
        }
        input_arguments[NR_DPUS-1].size=(input_size_8bytes - input_size_dpu_8bytes * (NR_DPUS-1)) * sizeof(T); 
        input_arguments[NR_DPUS-1].kernel=kernel;
        // Copy input arrays
        i = 0;
        DPU_FOREACH(dpu_set, dpu, i) {
            DPU_ASSERT(dpu_prepare_xfer(dpu, &input_arguments[i]));
        }
        DPU_ASSERT(dpu_push_xfer(dpu_set, DPU_XFER_TO_DPU, "DPU_INPUT_ARGUMENTS", 0, sizeof(input_arguments[0]), DPU_XFER_DEFAULT));
        DPU_FOREACH(dpu_set, dpu, i) {
            DPU_ASSERT(dpu_prepare_xfer(dpu, bufferA + input_size_dpu_8bytes * i));
        }
        DPU_ASSERT(dpu_push_xfer(dpu_set, DPU_XFER_TO_DPU, DPU_MRAM_HEAP_POINTER_NAME, 0, input_size_dpu_8bytes * sizeof(T), DPU_XFER_DEFAULT));
        if(rep >= p.n_warmup)
            stop(&timer, 3);

        //printf("Run program on DPU(s) \n");
        // Run DPU kernel
        if(rep >= p.n_warmup) {
            start(&timer, 4, 0);
            #if ENERGY
            DPU_ASSERT(dpu_probe_start(&probe));
            #endif
        }
 
        DPU_ASSERT(dpu_launch(dpu_set, DPU_SYNCHRONOUS));
        if(rep >= p.n_warmup) {
            stop(&timer, 4);
            #if ENERGY
            DPU_ASSERT(dpu_probe_stop(&probe));
            #endif
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

        //printf("Retrieve results\n");
        dpu_results_t results[NR_DPUS];
        T* results_count = malloc(NR_DPUS * sizeof(T));
        if(rep >= p.n_warmup)
            start(&timer, 5, 0);
        i = 0;
        // PARALLEL RETRIEVE TRANSFER
        dpu_results_t* results_retrieve[NR_DPUS];

        DPU_FOREACH(dpu_set, dpu, i) {
            results_retrieve[i] = (dpu_results_t*)malloc(NR_TASKLETS * sizeof(dpu_results_t));
            DPU_ASSERT(dpu_prepare_xfer(dpu, results_retrieve[i]));
        }
        DPU_ASSERT(dpu_push_xfer(dpu_set, DPU_XFER_FROM_DPU, "DPU_RESULTS", 0, NR_TASKLETS * sizeof(dpu_results_t), DPU_XFER_DEFAULT));

        DPU_FOREACH(dpu_set, dpu, i) {
            // Retrieve tasklet timings
            for (unsigned int each_tasklet = 0; each_tasklet < NR_TASKLETS; each_tasklet++) {
                if(each_tasklet == 0)
                    results[i].t_count = results_retrieve[i][each_tasklet].t_count;
            }
#if !PERF
            free(results_retrieve[i]);
#endif
            // Sequential reduction
            count += results[i].t_count;
#if PRINT
            printf("i=%d -- %lu\n", i, count);
#endif
        }

#if PERF
        DPU_FOREACH(dpu_set, dpu, i) {
            results[i].cycles = 0;
            // Retrieve tasklet timings
            for (unsigned int each_tasklet = 0; each_tasklet < NR_TASKLETS; each_tasklet++) {
                if (results_retrieve[i][each_tasklet].cycles > results[i].cycles)
                    results[i].cycles = results_retrieve[i][each_tasklet].cycles;
            }
            free(results_retrieve[i]);
        }
#endif
        if(rep >= p.n_warmup)
            stop(&timer, 5);

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

        // Free memory
        free(results_count);

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
        if(count != count_host) status = false;
        if (status) {
            printf("[" ANSI_COLOR_GREEN "OK" ANSI_COLOR_RESET "] Outputs are equal\n");
            if (rep >= p.n_warmup) {
                printf("[::] RED UPMEM | n_dpus=%d n_ranks=%d n_tasklets=%d e_type=%s block_size_B=%d n_elements=%d",
                    NR_DPUS, nr_of_ranks, NR_TASKLETS, XSTR(T), BLOCK_SIZE, input_size);
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
                    input_size * sizeof(T) / timer.time[2],
                    input_size * sizeof(T) / (timer.time[4]),
                    input_size * sizeof(T) / (timer.time[0] + timer.time[1] + timer.time[3] + timer.time[4] + timer.time[5] + timer.time[6]));
                printf(" throughput_upmem_wxr_MBps=%f throughput_upmem_lwxr_MBps=%f throughput_upmem_alwxr_MBps=%f",
                    input_size * sizeof(T) / (timer.time[3] + timer.time[4] + timer.time[5]),
                    input_size * sizeof(T) / (timer.time[1] + timer.time[3] + timer.time[4] + timer.time[5]),
                    input_size * sizeof(T) / (timer.time[0] + timer.time[1] + timer.time[3] + timer.time[4] + timer.time[5]));
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
            printf("[" ANSI_COLOR_RED "ERROR" ANSI_COLOR_RESET "] Outputs differ!\n");
        }

    }
#if PERF
    printf("DPU cycles  = %g cc\n", cc / p.n_reps);
#endif

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
