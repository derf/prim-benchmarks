/**
* app.c
* TRNS Host Application Source File
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
#include <math.h>

#include "../support/common.h"
#include "../support/timer.h"
#include "../support/params.h"

#define XSTR(x) STR(x)
#define STR(x) #x

// Define the DPU Binary path as DPU_BINARY here
#ifndef DPU_BINARY
#define DPU_BINARY "./bin/dpu_code"
#endif

// Pointer declaration
static T* A_host;
static T* A_backup;
static T* A_result;

struct Params p;
unsigned int kernel = 0;

// Create input arrays
static void read_input(T* A, unsigned int nr_elements) {
    srand(0);
    for (unsigned int i = 0; i < nr_elements; i++) {
        A[i] = (T) (rand());
    }
}

// Compute output in the host
static void trns_host(T* input, unsigned int A, unsigned int B, unsigned int b){
   T* output = (T*) malloc(sizeof(T) * A * B * b);
   unsigned int next;
   for (unsigned int j = 0; j < b; j++){
      for (unsigned int i = 0; i < A * B; i++){
         next = (i * A) - (A * B - 1) * (i / B);
         output[next * b + j] = input[i*b+j];
      }
   }
   for (unsigned int k = 0; k < A * B * b; k++){
      input[k] = output[k];
   }
   free(output);
}

// Main of the Host Application
int main(int argc, char **argv) {

    p = input_params(argc, argv);

    struct dpu_set_t dpu_set, dpu;
    uint32_t nr_of_dpus;
    uint32_t nr_of_ranks;
    
#if ENERGY
    struct dpu_probe_t probe;
    DPU_ASSERT(dpu_probe_init("energy_probe", &probe));
#endif

    unsigned int i = 0;
    unsigned int N_ = p.N_;
    const unsigned int n = p.n;
    const unsigned int M_ = p.M_;
    const unsigned int m = p.m;
    N_ = p.exp == 0 ? N_ * NR_DPUS : N_;

    // Input/output allocation
    A_host = (T*)malloc(M_ * m * N_ * n * sizeof(T));
    A_backup = (T*)malloc(M_ * m * N_ * n * sizeof(T));
    A_result = (T*)malloc(M_ * m * N_ * n * sizeof(T));
    T* done_host = (T*)malloc(M_ * n); // Host array to reset done array of step 3
    memset(done_host, 0, M_ * n);

    // Create an input file with arbitrary data
    read_input(A_host, M_ * m * N_ * n);
    memcpy(A_backup, A_host, M_ * m * N_ * n * sizeof(T));

    // Timer declaration
    Timer timer;

    int numa_node_rank = -2;

    // Loop over main kernel
    for(int rep = 0; rep < p.n_warmup + p.n_reps; rep++) {

        // Compute output on CPU (performance comparison and verification purposes)
        memcpy(A_host, A_backup, M_ * m * N_ * n * sizeof(T));
        if(rep >= p.n_warmup)
            start(&timer, 0, 0);
        trns_host(A_host, M_ * m, N_ * n, 1);
        if(rep >= p.n_warmup)
            stop(&timer, 0);

        unsigned int curr_dpu = 0;
        unsigned int active_dpus;
        unsigned int active_dpus_before = 0;
        unsigned int first_round = 1;

        while(curr_dpu < N_){
            // Allocate DPUs and load binary
            if((N_ - curr_dpu) > NR_DPUS){
                active_dpus = NR_DPUS;
            } else {
                active_dpus = (N_ - curr_dpu);
            }
            if((active_dpus_before != active_dpus) && (!(first_round))){
                start(&timer, 1, 1);
                DPU_ASSERT(dpu_free(dpu_set));
                DPU_ASSERT(dpu_alloc(active_dpus, NULL, &dpu_set));
                stop(&timer, 1);
                start(&timer, 2, 1);
                DPU_ASSERT(dpu_load(dpu_set, DPU_BINARY, NULL));
                stop(&timer, 2);
                DPU_ASSERT(dpu_get_nr_dpus(dpu_set, &nr_of_dpus));
                DPU_ASSERT(dpu_get_nr_ranks(dpu_set, &nr_of_ranks));
            } else if (first_round){
                start(&timer, 1, 0);
                DPU_ASSERT(dpu_alloc(active_dpus, NULL, &dpu_set));
                stop(&timer, 1);
                start(&timer, 2, 0);
                DPU_ASSERT(dpu_load(dpu_set, DPU_BINARY, NULL));
                stop(&timer, 2);
                DPU_ASSERT(dpu_get_nr_dpus(dpu_set, &nr_of_dpus));
                DPU_ASSERT(dpu_get_nr_ranks(dpu_set, &nr_of_ranks));
            }
            active_dpus_before = active_dpus;

            if(rep >= p.n_warmup) {
                start(&timer, 3, !first_round);
            }
            // Load input matrix (step 1)
            for(unsigned int j = 0; j < M_ * m; j++){
                unsigned int i = 0;
                DPU_FOREACH(dpu_set, dpu) {
                    DPU_ASSERT(dpu_prepare_xfer(dpu, &A_backup[j * N_ * n + n * (i + curr_dpu)]));
                    i++;
                }
                DPU_ASSERT(dpu_push_xfer(dpu_set, DPU_XFER_TO_DPU, DPU_MRAM_HEAP_POINTER_NAME, sizeof(T) * j * n, sizeof(T) * n, DPU_XFER_DEFAULT));
            }
            if(rep >= p.n_warmup) {
                stop(&timer, 3);
            }
            // Reset done array (for step 3)
            if(rep >= p.n_warmup) {
                start(&timer, 4, !first_round);
            }
            DPU_FOREACH(dpu_set, dpu) {
                DPU_ASSERT(dpu_prepare_xfer(dpu, done_host));
            }
            DPU_ASSERT(dpu_push_xfer(dpu_set, DPU_XFER_TO_DPU, DPU_MRAM_HEAP_POINTER_NAME, M_ * m * n * sizeof(T), (M_ * n) / 8 == 0 ? 8 : M_ * n, DPU_XFER_DEFAULT));

            if(rep >= p.n_warmup) {
                stop(&timer, 4);
            }
            if(rep >= p.n_warmup) {
                start(&timer, 5, !first_round);
            }

            kernel = 0;
            dpu_arguments_t input_arguments = {m, n, M_, (enum kernels)kernel};
            // transfer control instructions to DPUs (run first program part)
            DPU_FOREACH(dpu_set, dpu, i) {
                DPU_ASSERT(dpu_prepare_xfer(dpu, &input_arguments));
            }
            DPU_ASSERT(dpu_push_xfer(dpu_set, DPU_XFER_TO_DPU, "DPU_INPUT_ARGUMENTS", 0, sizeof(input_arguments), DPU_XFER_DEFAULT));
            if(rep >= p.n_warmup) {
                stop(&timer, 5);
            }
            // Run DPU kernel
            if(rep >= p.n_warmup){
                start(&timer, 6, !first_round);
#if ENERGY
                DPU_ASSERT(dpu_probe_start(&probe));
#endif
            }
            DPU_ASSERT(dpu_launch(dpu_set, DPU_SYNCHRONOUS));
            if(rep >= p.n_warmup){
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

            // transfer control instructions to DPUs (run second program part)
            if(rep >= p.n_warmup) {
                start(&timer, 7, !first_round);
            }
            kernel = 1;
            dpu_arguments_t input_arguments2 = {m, n, M_, (enum kernels)kernel};
            DPU_FOREACH(dpu_set, dpu, i) {
                DPU_ASSERT(dpu_prepare_xfer(dpu, &input_arguments2));
            }
            DPU_ASSERT(dpu_push_xfer(dpu_set, DPU_XFER_TO_DPU, "DPU_INPUT_ARGUMENTS", 0, sizeof(input_arguments2), DPU_XFER_DEFAULT));
            if(rep >= p.n_warmup) {
                stop(&timer, 7);
            }
            // Run DPU kernel
            if(rep >= p.n_warmup){
                start(&timer, 8, !first_round);
#if ENERGY
                DPU_ASSERT(dpu_probe_start(&probe));
#endif
            }
            DPU_ASSERT(dpu_launch(dpu_set, DPU_SYNCHRONOUS));
            if(rep >= p.n_warmup){
                stop(&timer, 8);
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

            if(rep >= p.n_warmup) {
                start(&timer, 9, !first_round);
            }
            DPU_FOREACH(dpu_set, dpu) {
                DPU_ASSERT(dpu_prepare_xfer(dpu, (T*)(&A_result[curr_dpu * m * n * M_])));
                curr_dpu++;
            }
            DPU_ASSERT(dpu_push_xfer(dpu_set, DPU_XFER_FROM_DPU, DPU_MRAM_HEAP_POINTER_NAME, 0, sizeof(T) * m * n * M_, DPU_XFER_DEFAULT));
            if(rep >= p.n_warmup) {
                stop(&timer, 9);
            }

            if(first_round){
                first_round = 0;
            }
        }

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

        start(&timer, 1, 1);
        DPU_ASSERT(dpu_free(dpu_set));
        stop(&timer, 1);

        // Check output
        bool status = true;
        for (i = 0; i < M_ * m * N_ * n; i++) {
            if(A_host[i] != A_result[i]){ 
                status = false;
#if PRINT
                printf("%d: %lu -- %lu\n", i, A_host[i], A_result[i]);
#endif
            }
        }
        if (status) {
            printf("[" ANSI_COLOR_GREEN "OK" ANSI_COLOR_RESET "] Outputs are equal\n");
#if DFATOOL_TIMING
            unsigned long input_size = M_ * m * N_ * n;
#endif
            if (rep >= p.n_warmup) {
                /*
                 * timer 0: CPU version
                 * timer 1: realloc (dpu_free, dpu_alloc)
                 * timer 2: dpu_load
                 * timer 3: write input matrix (step 1)
                 * timer 4: write zeroed 'done' array (for step 3)
                 * timer 5: write control instructions (run first kernel)
                 * timer 6: run DPU program (first kernel)
                 * timer 7: write control instructions (run second kernel)
                 * timer 8: run DPU program (second kernel)
                 * timer 9: read transposed matrix
                 */
                dfatool_printf("[::] TRNS-UPMEM | n_dpus=%d n_ranks=%d n_tasklets=%d e_type=%s n_elements=%lu numa_node_rank=%d ",
                    NR_DPUS, nr_of_ranks, NR_TASKLETS, XSTR(T), input_size, numa_node_rank);
                dfatool_printf("| latency_cpu_us=%f latency_realloc_us=%f latency_load_us=%f latency_write_us=%f latency_kernel_us=%f latency_read_us=%f",
                    timer.time[0], // CPU
                    timer.time[1], // free + alloc
                    timer.time[2], // load
                    timer.time[3] + timer.time[4] + timer.time[5] + timer.time[7], // write
                    timer.time[6] + timer.time[8], // kernel
                    timer.time[9]); // read
                dfatool_printf(" latency_write1_us=%f latency_write2_us=%f latency_write3_us=%f latency_write4_us=%f latency_kernel1_us=%f latency_kernel2_us=%f",
                    timer.time[3],
                    timer.time[4],
                    timer.time[5],
                    timer.time[7],
                    timer.time[6],
                    timer.time[8]);
                dfatool_printf(" throughput_cpu_MBps=%f throughput_upmem_kernel_MBps=%f throughput_upmem_total_MBps=%f",
                    input_size * sizeof(T) / timer.time[0],
                    input_size * sizeof(T) / (timer.time[6] + timer.time[8]),
                    input_size * sizeof(T) / (timer.time[1] + timer.time[2] + timer.time[3] + timer.time[4] + timer.time[5] + timer.time[6] + timer.time[7] + timer.time[8] + timer.time[9]));
                dfatool_printf(" throughput_upmem_wxr_MBps=%f throughput_upmem_lwxr_MBps=%f throughput_upmem_alwxr_MBps=%f",
                    input_size *  sizeof(T) / (timer.time[3] + timer.time[4] + timer.time[5] + timer.time[6] + timer.time[7] + timer.time[8] + timer.time[9]),
                    input_size *  sizeof(T) / (timer.time[2] + timer.time[3] + timer.time[4] + timer.time[5] + timer.time[6] + timer.time[7] + timer.time[8] + timer.time[9]),
                    input_size *  sizeof(T) / (timer.time[1] + timer.time[2] + timer.time[3] + timer.time[4] + timer.time[5] + timer.time[6] + timer.time[7] + timer.time[8] + timer.time[9]));
                dfatool_printf(" throughput_cpu_MOpps=%f throughput_upmem_kernel_MOpps=%f throughput_upmem_total_MOpps=%f",
                    input_size / timer.time[0],
                    input_size / (timer.time[6] + timer.time[8]),
                    input_size / (timer.time[1] + timer.time[2] + timer.time[3] + timer.time[4] + timer.time[5] + timer.time[6] + timer.time[7] + timer.time[8] + timer.time[9]));
                dfatool_printf(" throughput_upmem_wxr_MOpps=%f throughput_upmem_lwxr_MOpps=%f throughput_upmem_alwxr_MOpps=%f\n",
                    input_size / (timer.time[3] + timer.time[4] + timer.time[5] + timer.time[6] + timer.time[7] + timer.time[8] + timer.time[9]),
                    input_size / (timer.time[2] + timer.time[3] + timer.time[4] + timer.time[5] + timer.time[6] + timer.time[7] + timer.time[8] + timer.time[9]),
                    input_size / (timer.time[1] + timer.time[2] + timer.time[3] + timer.time[4] + timer.time[5] + timer.time[6] + timer.time[7] + timer.time[8] + timer.time[9]));
            }
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
    free(A_host);
    free(A_backup);
    free(A_result);
    free(done_host);

    return 0;
}
