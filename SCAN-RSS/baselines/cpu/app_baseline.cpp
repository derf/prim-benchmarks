/*
* JGL@SAFARI
*/

/**
* CPU code with Thrust
*/
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <unistd.h>
#include <getopt.h>
#include <assert.h>

#include <iostream>
#include <fstream>
#include <cstdlib>
#include <ctime>
#include <cstdio>
#include <math.h>
#include <sys/time.h>

#include <vector>
#include <thrust/device_vector.h>
#include <thrust/host_vector.h>
#include <thrust/scan.h>
#include <thrust/copy.h>
#include <thrust/system/omp/execution_policy.h>
#include <thrust/system/omp/vector.h>

#include <omp.h>

#include "../../support/common.h"
#include "../../support/timer.h"

#if NUMA
#include <numaif.h>
#include <numa.h>

void* mp_pages[1];
int mp_status[1];
int mp_nodes[1];
int numa_node_in = -1;
int numa_node_out = -1;
int numa_node_cpu = -1;
#endif

#define XSTR(x) STR(x)
#define STR(x) #x

#define ANSI_COLOR_RED     "\x1b[31m"
#define ANSI_COLOR_GREEN   "\x1b[32m"
#define ANSI_COLOR_RESET   "\x1b[0m"

// Pointer declaration
static T* A;
static T* C;

/**
* @brief creates input arrays
* @param nr_elements how many elements in input arrays
*/
static void read_input(T* A, unsigned int nr_elements) {
    //srand(0);
    printf("nr_elements\t%u\t", nr_elements);
    for (unsigned int i = 0; i < nr_elements; i++) {
        //A[i] = (T) (rand()) % 2;
        A[i] = i;
    }
}

/**
* @brief compute output in the host
*/
static void scan_host(T* C, T* A, unsigned int nr_elements) {
    C[0] = A[0];
    for (unsigned int i = 1; i < nr_elements; i++) {
        C[i] = C[i - 1] + A[i - 1];
    }
}

// Params ---------------------------------------------------------------------
typedef struct Params {
    unsigned int   input_size;
    int   n_warmup;
    int   n_reps;
    int   n_threads;
    int   exp;
#if NUMA
    struct bitmask* bitmask_in;
    struct bitmask* bitmask_out;
    int numa_node_cpu;
#endif
}Params;

void usage() {
    fprintf(stderr,
        "\nUsage:  ./program [options]"
        "\n"
        "\nGeneral options:"
        "\n    -h        help"
        "\n    -w <W>    # of untimed warmup iterations (default=1)"
        "\n    -e <E>    # of timed repetition iterations (default=3)"
        "\n    -x <X>    Weak (0) or strong (1) scaling (default=0)"
        "\n    -t <T>    # of threads (default=8)"
        "\n"
        "\nBenchmark-specific options:"
        "\n    -i <I>    input size (default=8M elements)"
        "\n");
}

struct Params input_params(int argc, char **argv) {
    struct Params p;
    p.input_size    = 2 << 20;
    p.n_warmup      = 1;
    p.n_reps        = 3;
    p.exp           = 0;
    p.n_threads     = 8;
#if NUMA
    p.bitmask_in     = NULL;
    p.bitmask_out    = NULL;
    p.numa_node_cpu = -1;
#endif

    int opt;
    while((opt = getopt(argc, argv, "hi:w:e:x:t:a:b:c:")) >= 0) {
        switch(opt) {
        case 'h':
        usage();
        exit(0);
        break;
        case 'i': p.input_size    = atoi(optarg); break;
        case 'w': p.n_warmup      = atoi(optarg); break;
        case 'e': p.n_reps        = atoi(optarg); break;
        case 'x': p.exp           = atoi(optarg); break;
        case 't': p.n_threads     = atoi(optarg); break;
#if NUMA
        case 'a': p.bitmask_in    = numa_parse_nodestring(optarg); break;
        case 'b': p.bitmask_out   = numa_parse_nodestring(optarg); break;
        case 'c': p.numa_node_cpu = atoi(optarg); break;
#endif
        default:
            fprintf(stderr, "\nUnrecognized option!\n");
            usage();
            exit(0);
        }
    }
    assert(p.n_threads > 0 && "Invalid # of threads!");

    return p;
}

/**
* @brief Main of the Host Application.
*/
int main(int argc, char **argv) {

    struct Params p = input_params(argc, argv);

    unsigned int i = 0;
    const unsigned int input_size = p.exp == 0 ? p.input_size * p.n_threads : p.input_size;
    assert(input_size % (p.n_threads) == 0 && "Input size!");

    // Input/output allocation

#if NUMA
    if (p.bitmask_in) {
        numa_set_membind(p.bitmask_in);
        numa_free_nodemask(p.bitmask_in);
    }
    A = (T*) numa_alloc(input_size * sizeof(T));
#else
    A = (T*)malloc(input_size * sizeof(T));
#endif

#if NUMA
    if (p.bitmask_out) {
        numa_set_membind(p.bitmask_out);
        numa_free_nodemask(p.bitmask_out);
    }
    C = (T*) numa_alloc(input_size * sizeof(T));
#else
    C = (T*) malloc(input_size * sizeof(T));
#endif

    // Create an input file with arbitrary data.
    read_input(A, input_size);

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

    // Timer declaration
    Timer timer;

    thrust::omp::vector<T> h_output(input_size);

    // Loop over main kernel
    for(int rep = 0; rep < p.n_warmup + p.n_reps; rep++) {

        // Compute output on CPU (performance comparison and verification purposes)
        if(rep >= p.n_warmup)
            start(&timer, 0, 0);
        scan_host(C, A, input_size);
        if(rep >= p.n_warmup)
            stop(&timer, 0);

        memcpy(thrust::raw_pointer_cast(&h_output[0]), A, input_size * sizeof(T));

        omp_set_num_threads(p.n_threads);

        unsigned int nr_threads = 0;
#pragma omp parallel
#pragma omp atomic
        nr_threads++;

        if(rep >= p.n_warmup)
            start(&timer, 1, 0);
        thrust::exclusive_scan(thrust::omp::par, h_output.begin(),h_output.end(),h_output.begin());
        if(rep >= p.n_warmup)
            stop(&timer, 1);

        // Check output
        bool status = true;
        for (i = 0; i < input_size; i++) {
            if(C[i] != h_output[i]){ 
                status = false;
                //printf("%d: %lu -- %lu\n", i, C[i], h_output[i]);
            }
        }
        if (status) {
            printf("[" ANSI_COLOR_GREEN "OK" ANSI_COLOR_RESET "] Outputs are equal\n");

            if(rep >= p.n_warmup) {
                printf("[::] SCAN-RSS-CPU | n_threads=%d e_type=%s n_elements=%d"
#if NUMA
                    " numa_node_in=%d numa_node_out=%d numa_node_cpu=%d numa_distance_in_cpu=%d numa_distance_cpu_out=%d"
#endif
                    " | throughput_cpu_ref_MBps=%f throughput_MBps=%f",
                    nr_threads, XSTR(T), input_size,
#if NUMA
                    numa_node_in, numa_node_out, numa_node_cpu, numa_distance(numa_node_in, numa_node_cpu), numa_distance(numa_node_cpu, numa_node_out),
#endif
                    input_size * sizeof(T) / timer.time[0],
                    input_size * sizeof(T) / timer.time[1]);
                printf(" throughput_cpu_ref_MOpps=%f throughput_MOpps=%f",
                    input_size / timer.time[0],
                    input_size / timer.time[1]);
                printall(&timer, 1);
            }
        } else {
            printf("[" ANSI_COLOR_RED "ERROR" ANSI_COLOR_RESET "] Outputs differ!\n");
        }

    }

    // Print timing results
    //printf("CPU ");
    //print(&timer, 0, p.n_reps);
    //printf("Kernel ");
    //print(&timer, 1, p.n_reps);


    // Deallocation
#if NUMA
    numa_free(A, input_size * sizeof(T));
    numa_free(C, input_size * sizeof(T));
#else
    free(A);
    free(C);
#endif

    return 0;
}
