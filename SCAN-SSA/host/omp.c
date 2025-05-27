/**
* app.c
* SCAN-SSA Host Application Source File
*
*/
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <stdint.h>
#include <unistd.h>
#include <getopt.h>
#include <assert.h>

#include "common.h"
#include "timer.h"
#include "params.h"

#define XSTR(x) STR(x)
#define STR(x) #x

#define NR_DPUS 1

extern int omp_get_num_threads();

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

// Compute output in the host (slow reference implementation)
static void scan_host(T* C, T* A, unsigned int nr_elements) {
    C[0] = A[0];
    for (unsigned int i = 1; i < nr_elements; i++) {
        C[i] = C[i - 1] + A[i];
    }
}

// Compute output in the host (OMP implementation)
static void scan_omp(T* C, T* A, unsigned int nr_elements, unsigned int nr_threads) {
    unsigned int i;
    unsigned int elem_per_block = nr_elements / nr_threads;
    T accum = 0;
    T tmp[nr_threads];

    // parallel block-wise scan
#pragma omp parallel for
    for (i=0; i < nr_threads; i++) {
        unsigned int start_index = elem_per_block * i;
        unsigned int stop_index = start_index + elem_per_block;
        C[start_index] = A[start_index];
        for (unsigned int j = start_index + 1; j < stop_index; j++) {
            C[j] = C[j - 1] + A[j];
        }
    }

    // sequential scan
    tmp[0] = 0;
    for (i=1; i < nr_threads; i++) {
        unsigned int start_index = elem_per_block * i;
        accum += C[start_index-1];
        tmp[i] = accum;
    }

    // parallel block-wise add
#pragma omp parallel for
    for (i=0; i < nr_threads; i++) {
        unsigned int start_index = elem_per_block * i;
        unsigned int stop_index = start_index + elem_per_block;
        for (unsigned int j = start_index; j < stop_index; j++) {
            C[j] += tmp[i];
        }
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

    unsigned int i = 0;

    unsigned int nr_threads = 0;
#pragma omp parallel
#pragma omp atomic
    nr_threads++;

    const unsigned int input_size = p.input_size * nr_threads; // Total input size (weak or strong scaling)

    // Input/output allocation
    A = malloc(input_size * sizeof(T));
    C = malloc(input_size * sizeof(T));
    C2 = malloc(input_size * sizeof(T));
    T *bufferC = C2;

    // Create an input file with arbitrary data
    read_input(A, input_size, input_size);

    // Timer declaration
    Timer timer;

    // Loop over main kernel
    for(int rep = 0; rep < p.n_warmup + p.n_reps; rep++) {

        // Compute output on CPU (performance comparison and verification purposes)
        if(rep >= p.n_warmup)
            start(&timer, 0, 0);
        scan_host(C, A, input_size);
        if(rep >= p.n_warmup)
            stop(&timer, 0);

        // Compute output on CPU (OMP)
        if(rep >= p.n_warmup)
            start(&timer, 1, 0);
        scan_omp(C2, A, input_size, nr_threads);
        if(rep >= p.n_warmup)
            stop(&timer, 1);

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
            printf("[::] n_threads=%d e_type=%s n_elements=%d "
                "| throughput_cpu_ref_MBps=%f throughput_cpu_omp_MBps=%f\n",
                nr_threads, XSTR(T), input_size,
                input_size * sizeof(T) / timer.time[0],
                input_size * sizeof(T) / timer.time[1]);
            printf("[::] n_threads=%d e_type=%s n_elements=%d "
                "| throughput_cpu_ref_MOpps=%f throughput_cpu_omp_MOpps=%f\n",
                nr_threads, XSTR(T), input_size,
                input_size / timer.time[0],
                input_size / timer.time[1]);
            printf("[::] n_threads=%d e_type=%s n_elements=%d | ",
                nr_threads, XSTR(T), input_size);
            printall(&timer, 1);
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
	
    return 0;
}
