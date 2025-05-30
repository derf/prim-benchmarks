/**
* app.c
* SCAN-RSS Host Application Source File
*
*/
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>

#if ASPECTC
extern "C" {
#endif

#include <dpu.h>
#include <dpu_log.h>
#include <dpu_management.h>
#include <dpu_target_macros.h>

#if ENERGY
#include <dpu_probe.h>
#endif

#if ASPECTC
}
#endif

#include <unistd.h>
#include <getopt.h>
#include <assert.h>

#include "common.h"
#include "timer.h"
#include "params.h"

// Define the DPU Binary path as DPU_BINARY here
#ifndef DPU_BINARY
#define DPU_BINARY "./bin/dpu_code"
#endif

#define XSTR(x) STR(x)
#define STR(x) #x

unsigned int kernel;

// Pointer declaration
static T* A;
static T* C;

// Create input arrays
static void read_input(T* A, unsigned int nr_elements, unsigned int nr_elements_round) {
    srand(0);
    //printf("nr_elements\t%u\t", nr_elements);
    for (unsigned int i = 0; i < nr_elements; i++) {
        A[i] = (T) (rand());
    }
    for (unsigned int i = nr_elements; i < nr_elements_round; i++) {
        A[i] = 0;
    }
}

// Compute output in the host
static void scan_host(T* C, T* A, unsigned int nr_elements) {
    C[0] = A[0];
    for (unsigned int i = 1; i < nr_elements; i++) {
        C[i] = C[i - 1] + A[i];
    }
}

// Main of the Host Application
int main(int argc, char **argv) {

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
    zero(&timer, 0);
#endif
#if !WITH_LOAD_OVERHEAD
    DPU_ASSERT(dpu_load(dpu_set, DPU_BINARY, NULL));
    DPU_ASSERT(dpu_get_nr_dpus(dpu_set, &nr_of_dpus));
    DPU_ASSERT(dpu_get_nr_ranks(dpu_set, &nr_of_ranks));
    assert(nr_of_dpus == NR_DPUS);
    zero(&timer, 1);
#endif
#if !WITH_FREE_OVERHEAD
    zero(&timer, 6);
#endif

    unsigned int i = 0;
    T accum = 0;

    const unsigned int input_size = p.exp == 0 ? p.input_size * NR_DPUS : p.input_size; // Total input size (weak or strong scaling)
    const unsigned int input_size_dpu_ = divceil(input_size, NR_DPUS); // Input size per DPU (max.)
    const unsigned int input_size_dpu_round = 
        (input_size_dpu_ % (NR_TASKLETS * REGS) != 0) ? roundup(input_size_dpu_, (NR_TASKLETS * REGS)) : input_size_dpu_; // Input size per DPU (max.), 8-byte aligned

    // Input/output allocation
    A = (T*) malloc(input_size_dpu_round * NR_DPUS * sizeof(T));
    C = (T*) malloc(input_size_dpu_round * NR_DPUS * sizeof(T));
    T *bufferA = A;
    T *bufferC = C;

    // Create an input file with arbitrary data
    read_input(A, input_size, input_size_dpu_round * NR_DPUS);

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

        // int prev_rank_id = -1;
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

        // Compute output on CPU (performance comparison and verification purposes)
        if(rep >= p.n_warmup) {
            start(&timer, 2, 0);
        }
        scan_host(C, A, input_size);
        if(rep >= p.n_warmup) {
            stop(&timer, 2);
        }

        //printf("Load input data\n");
        if(rep >= p.n_warmup) {
            start(&timer, 3, 0);
        }
        // Input arguments
        const unsigned int input_size_dpu = input_size_dpu_round;
        kernel = 0;
        dpu_arguments_t input_arguments = {(uint32_t)(input_size_dpu * sizeof(T)), (enum kernels)kernel, 0};
        // Copy input arrays
        i = 0;
        DPU_FOREACH(dpu_set, dpu, i) {
            DPU_ASSERT(dpu_prepare_xfer(dpu, &input_arguments));
        }
        DPU_ASSERT(dpu_push_xfer(dpu_set, DPU_XFER_TO_DPU, "DPU_INPUT_ARGUMENTS", 0, sizeof(input_arguments), DPU_XFER_DEFAULT));
        DPU_FOREACH(dpu_set, dpu, i) {
            DPU_ASSERT(dpu_prepare_xfer(dpu, bufferA + input_size_dpu * i));
        }
        DPU_ASSERT(dpu_push_xfer(dpu_set, DPU_XFER_TO_DPU, DPU_MRAM_HEAP_POINTER_NAME, 0, input_size_dpu * sizeof(T), DPU_XFER_DEFAULT));
        if(rep >= p.n_warmup) {
            stop(&timer, 3);
        }

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
        dpu_results_t results[nr_of_dpus];
        T* results_scan = (T*) malloc(nr_of_dpus * sizeof(T));
        i = 0;
        accum = 0;

        if(rep >= p.n_warmup) {
            start(&timer, 5, 0);
        }
        // PARALLEL RETRIEVE TRANSFER
        dpu_results_t* results_retrieve[nr_of_dpus];

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
            free(results_retrieve[i]);
            // Sequential scan
            T temp = results[i].t_count;
            results_scan[i] = accum;
            accum += temp;
#if PRINT
            printf("i=%d -- %lu,  %lu, %lu\n", i, results_scan[i], accum, temp);
#endif
        }

        // Arguments for scan kernel (2nd kernel)
        kernel = 1;
        dpu_arguments_t input_arguments_2[NR_DPUS];
        for(i=0; i<nr_of_dpus; i++) {
            input_arguments_2[i].size=input_size_dpu * sizeof(T); 
            input_arguments_2[i].kernel=(enum kernels)kernel;
            input_arguments_2[i].t_count=results_scan[i];
        }
        DPU_FOREACH(dpu_set, dpu, i) {
            DPU_ASSERT(dpu_prepare_xfer(dpu, &input_arguments_2[i]));
        }
        DPU_ASSERT(dpu_push_xfer(dpu_set, DPU_XFER_TO_DPU, "DPU_INPUT_ARGUMENTS", 0, sizeof(input_arguments_2[0]), DPU_XFER_DEFAULT));
        if(rep >= p.n_warmup) {
            stop(&timer, 5);
        }

        //printf("Run program on DPU(s) \n");
        // Run DPU kernel
        if(rep >= p.n_warmup) {
            start(&timer, 6, 0);
            #if ENERGY
            DPU_ASSERT(dpu_probe_start(&probe));
            #endif
        }
        DPU_ASSERT(dpu_launch(dpu_set, DPU_SYNCHRONOUS));
        if(rep >= p.n_warmup) {
            stop(&timer, 6);
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
        if(rep >= p.n_warmup) {
            start(&timer, 7, 0);
        }
        i = 0;
        // PARALLEL RETRIEVE TRANSFER
        DPU_FOREACH(dpu_set, dpu, i) {
            DPU_ASSERT(dpu_prepare_xfer(dpu, bufferC + input_size_dpu * i));
        }
        DPU_ASSERT(dpu_push_xfer(dpu_set, DPU_XFER_FROM_DPU, DPU_MRAM_HEAP_POINTER_NAME, input_size_dpu * sizeof(T), input_size_dpu * sizeof(T), DPU_XFER_DEFAULT));
        if(rep >= p.n_warmup) {
            stop(&timer, 7);
        }

#if WITH_ALLOC_OVERHEAD
#if WITH_FREE_OVERHEAD
        if(rep >= p.n_warmup) {
            start(&timer, 8, 0);
        }
#endif
        DPU_ASSERT(dpu_free(dpu_set));
#if WITH_FREE_OVERHEAD
        if(rep >= p.n_warmup) {
            stop(&timer, 8);
        }
#endif
#endif

        // Free memory
        free(results_scan);

        // Check output
        bool status = true;
        for (i = 0; i < input_size; i++) {
            if(C[i] != bufferC[i]){ 
                status = false;
#if PRINT
                printf("%d: %lu -- %lu\n", i, C[i], bufferC[i]);
#endif
            }
        }
        if (status) {
            printf("[" ANSI_COLOR_GREEN "OK" ANSI_COLOR_RESET "] Outputs are equal\n");
            dfatool_printf("[::] SCAN-RSS-UPMEM | n_dpus=%d n_ranks=%d n_tasklets=%d e_type=%s block_size_B=%d b_unroll=%d n_elements=%d",
                NR_DPUS, nr_of_ranks, NR_TASKLETS, XSTR(T), BLOCK_SIZE, UNROLL, input_size);
            dfatool_printf(" b_with_alloc_overhead=%d b_with_load_overhead=%d b_with_free_overhead=%d numa_node_rank=%d ",
                WITH_ALLOC_OVERHEAD, WITH_LOAD_OVERHEAD, WITH_FREE_OVERHEAD, numa_node_rank);
            dfatool_printf("| latency_alloc_us=%f latency_load_us=%f latency_cpu_us=%f latency_write_us=%f latency_kernel_us=%f latency_sync_us=%f latency_read_us=%f latency_free_us=%f",
                timer.time[0],
                timer.time[1],
                timer.time[2],
                timer.time[3], // write
                timer.time[4] + timer.time[6], // kernel
                timer.time[5], // sync
                timer.time[7], // read
                timer.time[8]);
            dfatool_printf(" throughput_cpu_MBps=%f throughput_upmem_kernel_MBps=%f throughput_upmem_total_MBps=%f",
                input_size * sizeof(T) / timer.time[2],
                input_size * sizeof(T) / (timer.time[4] + timer.time[5] + timer.time[6]),
                input_size * sizeof(T) / (timer.time[0] + timer.time[1] + timer.time[3] + timer.time[4] + timer.time[5] + timer.time[6] + timer.time[7] + timer.time[8]));
            dfatool_printf(" throughput_upmem_s_MBps=%f throughput_upmem_wxsxr_MBps=%f throughput_upmem_lwxsxr_MBps=%f throughput_upmem_alwxsxr_MBps=%f",
                input_size * sizeof(T) / (timer.time[5]),
                input_size * sizeof(T) / (timer.time[3] + timer.time[4] + timer.time[5] + timer.time[6] + timer.time[7]),
                input_size * sizeof(T) / (timer.time[1] + timer.time[3] + timer.time[4] + timer.time[5] + timer.time[6] + timer.time[7]),
                input_size * sizeof(T) / (timer.time[0] + timer.time[1] + timer.time[3] + timer.time[4] + timer.time[5] + timer.time[6] + timer.time[7]));
            dfatool_printf(" throughput_cpu_MOpps=%f throughput_upmem_kernel_MOpps=%f throughput_upmem_total_MOpps=%f",
                input_size / timer.time[2],
                input_size / (timer.time[4] + timer.time[5] + timer.time[6]),
                input_size / (timer.time[0] + timer.time[1] + timer.time[3] + timer.time[4] + timer.time[5] + timer.time[6] + timer.time[7] + timer.time[8]));
            dfatool_printf(" throughput_upmem_s_MOpps=%f throughput_upmem_wxsxr_MOpps=%f throughput_upmem_lwxsxr_MOpps=%f throughput_upmem_alwxsxr_MOpps=%f\n",
                input_size / (timer.time[5]),
                input_size / (timer.time[3] + timer.time[4] + timer.time[5] + timer.time[6] + timer.time[7]),
                input_size / (timer.time[1] + timer.time[3] + timer.time[4] + timer.time[5] + timer.time[6] + timer.time[7]),
                input_size / (timer.time[0] + timer.time[1] + timer.time[3] + timer.time[4] + timer.time[5] + timer.time[6] + timer.time[7]));
        } else {
            printf("[" ANSI_COLOR_RED "ERROR" ANSI_COLOR_RESET "] Outputs differ!\n");
        }
    }

    #if ENERGY
    double energy;
    DPU_ASSERT(dpu_probe_get(&probe, DPU_ENERGY, DPU_AVERAGE, &energy));
    printf("DPU Energy (J): %f\t", energy);
    #endif	



    // Deallocation
    free(A);
    free(C);

#if !WITH_ALLOC_OVERHEAD
    DPU_ASSERT(dpu_free(dpu_set));
#endif
	
    return 0;
}
