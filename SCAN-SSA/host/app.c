/**
* app.c
* SCAN-SSA Host Application Source File
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
static T* C;
static T* C2;

// Create input arrays
static void read_input(T* A, unsigned int nr_elements, unsigned int nr_elements_round) {
    srand(0);
    printf("nr_elements\t%u\t", nr_elements);
    for (unsigned int i = 0; i < nr_elements; i++) {
        A[i] = (T) (rand());
    }
    for (unsigned int i = nr_elements; i < nr_elements_round; i++) {
        A[i] = 0;
    }
}

// Compute output in the host
// XXX for a proper comparison, this function would need to use OpenMP or similar
// (i.e., a SCAN-SSA variant using multiple threads)
static void scan_host(T* C, T* A, unsigned int nr_elements) {
    C[0] = A[0];
    for (unsigned int i = 1; i < nr_elements; i++) {
        C[i] = C[i - 1] + A[i];
    }
}

/*
 * "SCAN" := [a1, a1, ..., an] -> [a1, a1 + a2, ..., a1 + a2 + ... + an]
 * From the paper:
 * SCAN-SSA has three steps:
 * * (copy data to the DPU) (timer 1)
 * * compute the partial scan operation locally inside each DPU (timer 2)
 * * retrieve last element of each local scan from the DPUs, do a local (complete) scan on the last elements, and push them to the next DPU (timer 3)
 * * finish the scan operation in each DPU (add sum [a1 ... ax] to a(x+1), ..., a(x+m) for DPU size m) (timer 4)
 * * (retrieve results from DPU) (timer 5)
 */

// Main of the Host Application
int main(int argc, char **argv) {

    struct Params p = input_params(argc, argv);

    struct dpu_set_t dpu_set, dpu;
    uint32_t nr_of_dpus;
    
#if ENERGY
    struct dpu_probe_t probe;
    DPU_ASSERT(dpu_probe_init("energy_probe", &probe));
#endif

    // Allocate DPUs and load binary
    DPU_ASSERT(dpu_alloc(NR_DPUS, NULL, &dpu_set));
    DPU_ASSERT(dpu_load(dpu_set, DPU_BINARY, NULL));
    DPU_ASSERT(dpu_get_nr_dpus(dpu_set, &nr_of_dpus));
    printf("Allocated %d DPU(s)\n", nr_of_dpus);

    unsigned int i = 0;
    T accum = 0;

    const unsigned int input_size = p.input_size * nr_of_dpus; // Total input size (weak or strong scaling)
    const unsigned int input_size_dpu_ = divceil(input_size, nr_of_dpus); // Input size per DPU (max.)
    const unsigned int input_size_dpu_round = 
        (input_size_dpu_ % (NR_TASKLETS * REGS) != 0) ? roundup(input_size_dpu_, (NR_TASKLETS * REGS)) : input_size_dpu_; // Input size per DPU (max.), 8-byte aligned

    // Input/output allocation
    A = malloc(input_size_dpu_round * nr_of_dpus * sizeof(T));
    C = malloc(input_size_dpu_round * nr_of_dpus * sizeof(T));
    C2 = malloc(input_size_dpu_round * nr_of_dpus * sizeof(T));
    T *bufferA = A;
    T *bufferC = C2;

    // Create an input file with arbitrary data
    read_input(A, input_size, input_size_dpu_round * nr_of_dpus);

    // Timer declaration
    Timer timer;

    printf("NR_TASKLETS\t%d\tBL\t%d\n", NR_TASKLETS, BL);

    // Loop over main kernel
    for(int rep = 0; rep < p.n_warmup + p.n_reps; rep++) {

        // Compute output on CPU (performance comparison and verification purposes)
        if(rep >= p.n_warmup)
            start(&timer, 0, 0);
        scan_host(C, A, input_size);
        if(rep >= p.n_warmup)
            stop(&timer, 0);

        //printf("Load input data\n");
        if(rep >= p.n_warmup)
            start(&timer, 1, 0);
        // Input arguments
        const unsigned int input_size_dpu = input_size_dpu_round;
        unsigned int kernel = 0;
        dpu_arguments_t input_arguments = {input_size_dpu * sizeof(T), kernel, 0};
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
        if(rep >= p.n_warmup)
            stop(&timer, 1);

        //printf("Run program on DPU(s) \n");
        // Run DPU kernel
        if(rep >= p.n_warmup) {
            start(&timer, 2, 0);
            #if ENERGY
            DPU_ASSERT(dpu_probe_start(&probe));
            #endif
        }
 
        DPU_ASSERT(dpu_launch(dpu_set, DPU_SYNCHRONOUS));
        if(rep >= p.n_warmup) {
            stop(&timer, 2);
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
        T* results_scan = malloc(nr_of_dpus * sizeof(T));
        i = 0;
        accum = 0;

        // PARALLEL RETRIEVE TRANSFER
        dpu_results_t* results_retrieve[nr_of_dpus];

        if(rep >= p.n_warmup)
            start(&timer, 3, 0);

        DPU_FOREACH(dpu_set, dpu, i) {
            results_retrieve[i] = (dpu_results_t*)malloc(NR_TASKLETS * sizeof(dpu_results_t));
            DPU_ASSERT(dpu_prepare_xfer(dpu, results_retrieve[i]));
        }
        DPU_ASSERT(dpu_push_xfer(dpu_set, DPU_XFER_FROM_DPU, "DPU_RESULTS", 0, NR_TASKLETS * sizeof(dpu_results_t), DPU_XFER_DEFAULT));

        DPU_FOREACH(dpu_set, dpu, i) {
            // Retrieve tasklet timings
            for (unsigned int each_tasklet = 0; each_tasklet < NR_TASKLETS; each_tasklet++) {
                if(each_tasklet == NR_TASKLETS - 1)
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

        // Arguments for add kernel (2nd kernel)
        kernel = 1;
        dpu_arguments_t input_arguments_2[NR_DPUS];
        for(i=0; i<nr_of_dpus; i++) {
            input_arguments_2[i].size=input_size_dpu * sizeof(T); 
            input_arguments_2[i].kernel=kernel;
            input_arguments_2[i].t_count=results_scan[i];
        }
        DPU_FOREACH(dpu_set, dpu, i) {
            DPU_ASSERT(dpu_prepare_xfer(dpu, &input_arguments_2[i]));
        }
        DPU_ASSERT(dpu_push_xfer(dpu_set, DPU_XFER_TO_DPU, "DPU_INPUT_ARGUMENTS", 0, sizeof(input_arguments_2[0]), DPU_XFER_DEFAULT));
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
        if(rep >= p.n_warmup)
            start(&timer, 5, 0);
        i = 0;
        // PARALLEL RETRIEVE TRANSFER
        DPU_FOREACH(dpu_set, dpu, i) {
            DPU_ASSERT(dpu_prepare_xfer(dpu, bufferC + input_size_dpu * i));
        }
        DPU_ASSERT(dpu_push_xfer(dpu_set, DPU_XFER_FROM_DPU, DPU_MRAM_HEAP_POINTER_NAME, input_size_dpu * sizeof(T), input_size_dpu * sizeof(T), DPU_XFER_DEFAULT));
        if(rep >= p.n_warmup)
            stop(&timer, 5);

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
            printf("[::] n_dpus=%d n_tasklets=%d e_type=%s block_size_B=%d b_unroll=%d n_elements=%d "
                "| throughput_cpu_MBps=%f throughput_pim_MBps=%f throughput_MBps=%f\n",
                nr_of_dpus, NR_TASKLETS, XSTR(T), BLOCK_SIZE, UNROLL, input_size,
                input_size * sizeof(T) / timer.time[0],
                input_size * sizeof(T) / (timer.time[2] + timer.time[4]),
                input_size * sizeof(T) / (timer.time[1] + timer.time[2] + timer.time[3] + timer.time[4]));
            printf("[::] n_dpus=%d n_tasklets=%d e_type=%s block_size_B=%d b_unroll=%d n_elements=%d "
                "| throughput_cpu_MOpps=%f throughput_pim_MOpps=%f throughput_MOpps=%f\n",
                nr_of_dpus, NR_TASKLETS, XSTR(T), BLOCK_SIZE, UNROLL, input_size,
                input_size / timer.time[0],
                input_size / (timer.time[2] + timer.time[4]),
                input_size / (timer.time[1] + timer.time[2] + timer.time[3] + timer.time[4]));
            printf("[::] n_dpus=%d n_tasklets=%d e_type=%s block_size_B=%d b_unroll=%d n_elements=%d | ",
                nr_of_dpus, NR_TASKLETS, XSTR(T), BLOCK_SIZE, UNROLL, input_size);
            printall(&timer, 5);
        } else {
            printf("[" ANSI_COLOR_RED "ERROR" ANSI_COLOR_RESET "] Outputs differ!\n");
        }
    }

	/*
    // Print timing results
    printf("CPU ");
    print(&timer, 0, p.n_reps);
    printf("CPU-DPU ");
    print(&timer, 1, p.n_reps);
    printf("DPU Kernel Scan ");
    print(&timer, 2, p.n_reps);
    printf("Inter-DPU (Scan) ");
    print(&timer, 3, p.n_reps);
    printf("DPU Kernel Add ");
    print(&timer, 4, p.n_reps);
    printf("DPU-CPU ");
    print(&timer, 5, p.n_reps);
    printf("\n");
	*/

    #if ENERGY
    double energy;
    DPU_ASSERT(dpu_probe_get(&probe, DPU_ENERGY, DPU_AVERAGE, &energy));
    printf("DPU Energy (J): %f\t", energy);
    #endif	



    // Deallocation
    free(A);
    free(C);
    free(C2);
    DPU_ASSERT(dpu_free(dpu_set));
	
    return 0;
}
