/**
* app.c
* CPU-DPU Communication Host Application Source File
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

#if NUMA


#include <dpu_management.h>
#include <dpu_target_macros.h>


#include <numaif.h>
#include <numa.h>

void* mp_pages[1];
int mp_status[1];
int mp_nodes[1];
int numa_node_in = -1;
int numa_node_out = -1;
int numa_node_rank = -1;
int numa_node_cpu = -1;
#endif

// Define the DPU Binary path as DPU_BINARY here
#ifndef DPU_BINARY
#define DPU_BINARY "./bin/dpu_code"
#endif

#define XSTR(x) STR(x)
#define STR(x) #x

// Pointer declaration
static T* A;
static T* B;
static T* C;

static const char transfer_mode[] =
#if SERIAL
"SERIAL"
#elif BROADCAST
"BROADCAST"
#else
"PUSH"
#endif
;

// Create input arrays
static void read_input(T* A, T* B, unsigned int nr_elements) {
    srand(0);
    //printf("nr_elements\t%u\t", nr_elements);
    for (unsigned int i = 0; i < nr_elements; i++) {
        A[i] = (T) (rand());
        B[i] = A[i];
    }
}

// Main of the Host Application
int main(int argc, char **argv) {

    struct Params p = input_params(argc, argv);

    struct dpu_set_t dpu_set, dpu;
    uint32_t nr_of_dpus;
    uint32_t nr_of_ranks;

    char ntpp[24];

    // Timer declaration
    Timer timer;

    snprintf(ntpp, 24, "nrThreadPerPool=%d", p.n_threads);
    // Allocate DPUs and load binary
    start(&timer, 4, 0);
#if NR_DPUS
    DPU_ASSERT(dpu_alloc(NR_DPUS, ntpp, &dpu_set));
#elif NR_RANKS
    DPU_ASSERT(dpu_alloc_ranks(NR_RANKS, ntpp, &dpu_set));
#else
#error "NR_DPUS o NR_RANKS must be set"
#endif
    stop(&timer, 4);
    start(&timer, 5, 0);
    DPU_ASSERT(dpu_load(dpu_set, DPU_BINARY, NULL));
    stop(&timer, 5);
    start(&timer, 6, 0);
    DPU_ASSERT(dpu_get_nr_dpus(dpu_set, &nr_of_dpus));
    DPU_ASSERT(dpu_get_nr_ranks(dpu_set, &nr_of_ranks));
    stop(&timer, 6);
    //printf("Allocated %d DPU(s)\n", nr_of_dpus);

    unsigned int i = 0;
    uint64_t input_size = p.exp == 0 ? p.input_size * nr_of_dpus : p.input_size;

    //printf("Load input data\n");
    // Input arguments
    const uint64_t input_size_dpu = input_size / nr_of_dpus;
#ifdef BROADCAST
    const uint64_t transfer_size = input_size;
#else
    const uint64_t transfer_size = input_size;
#endif

    // Input/output allocation

#if NUMA
    if (p.bitmask_in) {
        numa_set_membind(p.bitmask_in);
        numa_free_nodemask(p.bitmask_in);
    }
#endif

    A = malloc(input_size * sizeof(T));
    B = malloc(input_size * sizeof(T));

#if NUMA
    if (p.bitmask_out) {
        numa_set_membind(p.bitmask_out);
        numa_free_nodemask(p.bitmask_out);
    }
#endif

    C = malloc(input_size * sizeof(T));

    T *bufferA = A;
    T *bufferC = C;


#if NUMA
    struct bitmask *bitmask_all = numa_allocate_nodemask();
    numa_bitmask_setall(bitmask_all);
    numa_set_membind(bitmask_all);
    numa_free_nodemask(bitmask_all);
#endif

#if NUMA
    mp_pages[0] = A;
    if (move_pages(0, 1, mp_pages, NULL, mp_status, 0) == -1) {
        perror("move_pages(A)");
    }
    else if (mp_status[0] < 0) {
        printf("move_pages error: %d", mp_status[0]);
    }
    else {
        numa_node_in = mp_status[0];
    }

    mp_pages[0] = C;
    if (move_pages(0, 1, mp_pages, NULL, mp_status, 0) == -1) {
        perror("move_pages(C)");
    }
    else if (mp_status[0] < 0) {
        printf("move_pages error: %d", mp_status[0]);
    }
    else {
        numa_node_out = mp_status[0];
    }

    numa_node_cpu = p.numa_node_cpu;
    if (numa_node_cpu != -1) {
        if (numa_run_on_node(numa_node_cpu) == -1) {
            perror("numa_run_on_node");
            numa_node_cpu = -1;
        }
    }
#endif


#if NUMA
    int prev_rank_id = -1;
    int rank_id = -1;
    DPU_FOREACH (dpu_set, dpu) {
        rank_id = dpu_get_rank_id(dpu_get_rank(dpu_from_set(dpu))) & DPU_TARGET_MASK;
        numa_node_rank = dpu_get_rank_numa_node(dpu_get_rank(dpu_from_set(dpu)));
        if (rank_id != prev_rank_id) {
            printf("/dev/dpu_rank%d @ NUMA node %d\n", rank_id, numa_node_rank);
            prev_rank_id = rank_id;
        }
/*
        printf("DPU @ rank %d slice.member %d.%d\n",
            rank_id,
            dpu_get_slice_id(dpu_from_set(dpu)),
            dpu_get_member_id(dpu_from_set(dpu))
        );
*/
    }
#endif


    // Create an input file with arbitrary data
    read_input(A, B, input_size);

    //printf("NR_TASKLETS\t%d\tBL\t%d\n", NR_TASKLETS, BL);
    printf("[::] NMC reconfiguration | n_dpus=%d n_ranks=%d n_tasklets=%d n_nops=%d n_instr=%d e_type=%s n_elements=%lu e_mode=%s"
#if NUMA
        " numa_node_in=%d numa_node_out=%d numa_node_cpu=%d numa_node_rank=%d"
#endif
        " | latency_dpu_alloc_ns=%lu latency_dpu_load_ns=%lu latency_dpu_get_ns=%lu\n",
        nr_of_dpus, nr_of_ranks, NR_TASKLETS, p.n_nops, p.n_instr, XSTR(T), transfer_size, transfer_mode,
#if NUMA
        numa_node_in, numa_node_out, numa_node_cpu, numa_node_rank,
#endif
        timer.nanoseconds[4], timer.nanoseconds[5], timer.nanoseconds[6]);

    // Loop over main kernel
    for(int rep = 0; rep < p.n_warmup + p.n_reps; rep++) {
        // Copy input arrays
        if(rep >= p.n_warmup)
            start(&timer, 1, 0);
        i = 0;
#ifdef SERIAL
        DPU_FOREACH (dpu_set, dpu) {
            DPU_ASSERT(dpu_copy_to(dpu, DPU_MRAM_HEAP_POINTER_NAME, 0, bufferA + input_size_dpu * i, input_size_dpu * sizeof(T)));
            i++;
        }
#elif BROADCAST
        DPU_ASSERT(dpu_broadcast_to(dpu_set, DPU_MRAM_HEAP_POINTER_NAME, 0, bufferA, input_size * sizeof(T), DPU_XFER_DEFAULT));
#else
        DPU_FOREACH(dpu_set, dpu, i) {
            DPU_ASSERT(dpu_prepare_xfer(dpu, bufferA + input_size_dpu * i));
        }
        DPU_ASSERT(dpu_push_xfer(dpu_set, DPU_XFER_TO_DPU, DPU_MRAM_HEAP_POINTER_NAME, 0, input_size_dpu * sizeof(T), DPU_XFER_DEFAULT));
#endif
        if(rep >= p.n_warmup)
            stop(&timer, 1);

        //printf("Run program on DPU(s) \n");
        // Run DPU kernel
        if(rep >= p.n_warmup)
            start(&timer, 2, 0);
        // empty kernel -> measure communication overhead
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

        //printf("Retrieve results\n");
        if(rep >= p.n_warmup)
            start(&timer, 3, 0);
        i = 0;
#ifdef SERIAL
        DPU_FOREACH (dpu_set, dpu) {
            DPU_ASSERT(dpu_copy_from(dpu, DPU_MRAM_HEAP_POINTER_NAME, 0, bufferC + input_size_dpu * i, input_size_dpu * sizeof(T)));
            i++;
        }
#else
        DPU_FOREACH(dpu_set, dpu, i) {
            DPU_ASSERT(dpu_prepare_xfer(dpu, bufferC + input_size_dpu * i));
        }
        DPU_ASSERT(dpu_push_xfer(dpu_set, DPU_XFER_FROM_DPU, DPU_MRAM_HEAP_POINTER_NAME, 0, input_size_dpu * sizeof(T), DPU_XFER_DEFAULT));
#endif
        if(rep >= p.n_warmup)
            stop(&timer, 3);

        if (rep >= p.n_warmup) {
            printf("[::] transfer UPMEM | n_dpus=%d n_ranks=%d n_tasklets=%d n_nops=%d n_instr=%d e_type=%s n_elements=%lu n_elements_per_dpu=%lu e_mode=%s"
#if NUMA
                " numa_node_in=%d numa_node_out=%d numa_node_cpu=%d numa_node_rank=%d"
#endif
                " | latency_dram_mram_ns=%lu latency_mram_dram_ns=%lu throughput_dram_mram_Bps=%f throughput_mram_dram_Bps=%f",
#ifdef BROADCAST
                nr_of_dpus, nr_of_ranks, NR_TASKLETS, p.n_nops, p.n_instr, XSTR(T), transfer_size, transfer_size, transfer_mode,
#else
                nr_of_dpus, nr_of_ranks, NR_TASKLETS, p.n_nops, p.n_instr, XSTR(T), transfer_size, transfer_size / nr_of_dpus, transfer_mode,
#endif
#if NUMA
                numa_node_in, numa_node_out, numa_node_cpu, numa_node_rank,
#endif
                timer.nanoseconds[1], timer.nanoseconds[3],
                transfer_size * sizeof(T) * 1e9 / timer.nanoseconds[1],
                transfer_size * sizeof(T) * 1e9 / timer.nanoseconds[3]);
            printf(" throughput_dram_mram_Opps=%f throughput_mram_dram_Opps=%f",
                transfer_size * 1e9 / timer.nanoseconds[1],
                transfer_size * 1e9 / timer.nanoseconds[3]);
            printf(" latency_dpu_launch_ns=%lu\n",
                timer.nanoseconds[2]);
        }
    }

    // Check output
    bool status = true;
#ifdef BROADCAST
    for (i = 0; i < input_size/nr_of_dpus; i++) {
        if(B[i] != bufferC[i]){ 
            status = false;
#if PRINT
            printf("%d: %u -- %u\n", i, B[i], bufferA[i]);
#endif
        }
    }
#else
    for (i = 0; i < input_size; i++) {
        if(B[i] != bufferC[i]){ 
            status = false;
#if PRINT
            printf("%d: %u -- %u\n", i, B[i], bufferA[i]);
#endif
        }
    }
#endif
    if (status) {
        //printf("[" ANSI_COLOR_GREEN "OK" ANSI_COLOR_RESET "] Outputs are equal\n");
    } else {
        printf("[" ANSI_COLOR_RED "ERROR" ANSI_COLOR_RESET "] Outputs differ!\n");
    }

    // Deallocation
    free(A);
    free(B);
    free(C);
    DPU_ASSERT(dpu_free(dpu_set));

    return 0;
}
